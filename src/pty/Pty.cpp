#include "Pty.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <vector>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <util.h>
#else
#include <pty.h>
#endif

namespace gt {

Pty::Pty(QObject *parent)
    : QObject(parent)
{
}

Pty::~Pty()
{
    m_notifier.reset();
    if (m_pid > 0) {
        ::kill(static_cast<pid_t>(m_pid), SIGHUP);
        int status = 0;
        // Do not block the GUI: give the child a moment, then leave it to init.
        if (::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG) == 0) {
            ::kill(static_cast<pid_t>(m_pid), SIGKILL);
            ::waitpid(static_cast<pid_t>(m_pid), &status, 0);
        }
    }
    if (m_fd >= 0)
        ::close(m_fd);
}

QString Pty::defaultShell()
{
    const QByteArray env = qgetenv("SHELL");
    if (!env.isEmpty() && QFileInfo(QString::fromLocal8Bit(env)).isExecutable())
        return QString::fromLocal8Bit(env);
    if (const passwd *pw = ::getpwuid(::getuid())) {
        if (pw->pw_shell && *pw->pw_shell && QFileInfo(QString::fromLocal8Bit(pw->pw_shell)).isExecutable())
            return QString::fromLocal8Bit(pw->pw_shell);
    }
    return QStringLiteral("/bin/sh");
}

bool Pty::start(const QString &program, const QStringList &args, const QString &workingDirectory, int cols, int rows)
{
    if (m_pid > 0) {
        m_error = QStringLiteral("already running");
        return false;
    }

    const QString prog = program.isEmpty() ? defaultShell() : program;
    const QString dir = (!workingDirectory.isEmpty() && QDir(workingDirectory).exists()) ? workingDirectory : QDir::homePath();

    // Everything the child needs is prepared before fork(): after fork only async-signal-safe
    // calls are allowed in a multi-threaded process.
    const QByteArray progBytes = QFile::encodeName(prog);
    const QByteArray dirBytes = QFile::encodeName(dir);
    std::vector<QByteArray> argBytes;
    argBytes.push_back(program.isEmpty() ? QByteArray("-") + QFile::encodeName(QFileInfo(prog).fileName()) : progBytes);
    for (const QString &a : args)
        argBytes.push_back(a.toLocal8Bit());
    // A login shell is started with "-name" as argv[0]; for an explicit program use its path.
    if (!program.isEmpty())
        argBytes[0] = progBytes;
    std::vector<char *> argv;
    for (QByteArray &b : argBytes)
        argv.push_back(b.data());
    argv.push_back(nullptr);

    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);

    int master = -1;
    const pid_t pid = ::forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        m_error = QString::fromLocal8Bit(std::strerror(errno));
        return false;
    }
    if (pid == 0) {
        // Child
        if (::chdir(dirBytes.constData()) != 0) {
            // stay in the inherited directory
        }
        ::setenv("TERM", "xterm-256color", 1);
        ::setenv("COLORTERM", "truecolor", 1);
        ::setenv("TERM_PROGRAM", "gus-terminal", 1);
        ::unsetenv("LINES");
        ::unsetenv("COLUMNS");
        ::execvp(progBytes.constData(), argv.data());
        const char msg[] = "gus-terminal: cannot execute the program\r\n";
        if (::write(STDERR_FILENO, msg, sizeof msg - 1) < 0) {
        }
        ::_exit(127);
    }

    m_fd = master;
    m_pid = pid;
    const int flags = ::fcntl(m_fd, F_GETFL);
    ::fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
    ::fcntl(m_fd, F_SETFD, FD_CLOEXEC);

    m_notifier = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &Pty::onReadable);
    return true;
}

void Pty::write(const QByteArray &data)
{
    if (m_fd < 0 || data.isEmpty())
        return;
    qsizetype done = 0;
    int retries = 0;
    while (done < data.size()) {
        const ssize_t n = ::write(m_fd, data.constData() + done, static_cast<size_t>(data.size() - done));
        if (n > 0) {
            done += n;
            retries = 0;
        } else if (n < 0 && (errno == EAGAIN || errno == EINTR)) {
            if (++retries > 1000)
                break; // the child is not reading; drop the rest instead of freezing the UI
            ::usleep(1000);
        } else {
            break;
        }
    }
}

void Pty::resize(int cols, int rows)
{
    if (m_fd < 0)
        return;
    struct winsize ws {};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ::ioctl(m_fd, TIOCSWINSZ, &ws); // the kernel sends SIGWINCH to the foreground group
}

void Pty::onReadable()
{
    char buf[65536];
    while (true) {
        const ssize_t n = ::read(m_fd, buf, sizeof buf);
        if (n > 0) {
            emit dataReceived(QByteArray(buf, static_cast<qsizetype>(n)));
            if (n < static_cast<ssize_t>(sizeof buf))
                return;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            return;
        break; // EOF or EIO: the child closed the terminal
    }
    reap();
}

void Pty::reap()
{
    m_notifier.reset();
    int status = 0;
    pid_t r = 0;
    // The slave side is closed, so the child is exiting; wait briefly for the status.
    for (int i = 0; i < 200; ++i) {
        r = ::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);
        if (r != 0)
            break;
        ::usleep(5000);
    }
    if (r == 0) {
        ::kill(static_cast<pid_t>(m_pid), SIGKILL);
        ::waitpid(static_cast<pid_t>(m_pid), &status, 0);
    }
    ::close(m_fd);
    m_fd = -1;
    m_pid = -1;
    const bool crashed = WIFSIGNALED(status);
    emit finished(crashed ? WTERMSIG(status) : WEXITSTATUS(status), crashed);
}

} // namespace gt
