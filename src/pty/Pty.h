#pragma once

#include <QByteArray>
#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <QStringList>

#include <memory>

namespace gt {

/// A child process attached to a real pseudo-terminal created with forkpty(3).
class Pty : public QObject {
    Q_OBJECT
public:
    explicit Pty(QObject *parent = nullptr);
    ~Pty() override;

    /// Starts `program` (empty = the user's login shell). Returns false and sets errorString() on failure.
    bool start(const QString &program, const QStringList &args, const QString &workingDirectory, int cols, int rows);
    void write(const QByteArray &data);
    void resize(int cols, int rows);
    bool isRunning() const { return m_pid > 0; }
    qint64 pid() const { return m_pid; }
    QString errorString() const { return m_error; }
    /// Name of the user's shell: $SHELL, then the passwd entry, then /bin/sh.
    static QString defaultShell();

signals:
    void dataReceived(const QByteArray &data);
    void finished(int exitCode, bool crashed);

private:
    void onReadable();
    void reap();

    int m_fd = -1;
    qint64 m_pid = -1;
    QString m_error;
    std::unique_ptr<QSocketNotifier> m_notifier;
};

} // namespace gt
