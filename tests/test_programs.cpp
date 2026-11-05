// Runs real terminal programs through the PTY and the emulator and checks what ends up on screen.
// Programs that are not installed are skipped.
#include "PtySession.h"

#include <QStandardPaths>
#include <QtTest>

class TestPrograms : public QObject {
    Q_OBJECT

    static bool have(const QString &name) { return !QStandardPaths::findExecutable(name).isEmpty(); }

    static QString screenOnly(const Screen &s)
    {
        QString out;
        for (int y = 0; y < s.rows(); ++y)
            out += s.lineText(s.historyLines() + y) + '\n';
        return out;
    }

private slots:
    void terminfoReportsColours()
    {
        if (!have("tput"))
            QSKIP("tput not installed", SkipSingle);
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {"-c", "echo colors=$(tput colors) cols=$(tput cols) lines=$(tput lines)"}, {}, 90, 30));
        QVERIFY(s.waitFinished());
        QVERIFY2(s.text().contains("colors=256 cols=90 lines=30"), qPrintable(s.text()));
    }

    void lessPagesAFile()
    {
        if (!have("less"))
            QSKIP("less not installed", SkipSingle);
        QTemporaryDir dir;
        QFile f(dir.filePath("long.txt"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        for (int i = 1; i <= 200; ++i)
            f.write("line number " + QByteArray::number(i) + "\n");
        f.close();
        Session s;
        QVERIFY(s.pty.start("less", {f.fileName()}, {}, 80, 24));
        QVERIFY2(s.waitFor("line number 1\n"), qPrintable(s.text()));
        QVERIFY(s.screen.altScreen());
        s.pty.write(" "); // next page
        QVERIFY2(s.waitFor("line number 24"), qPrintable(screenOnly(s.screen)));
        s.pty.write("q");
        QVERIFY(s.waitFinished());
        QVERIFY(!s.screen.altScreen()); // the shell screen is restored
    }

    void vimEditsAFile()
    {
        if (!have("vim"))
            QSKIP("vim not installed", SkipSingle);
        QTemporaryDir dir;
        const QString path = dir.filePath("note.txt");
        Session s;
        QVERIFY(s.pty.start("vim", {"-u", "NONE", "-N", path}, {}, 80, 24));
        QVERIFY2(s.waitFor("note.txt"), qPrintable(screenOnly(s.screen)));
        s.pty.write("ihello from vim\x1b:wq\r");
        QVERIFY(s.waitFinished());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("hello from vim\n"));
    }

    void nanoShowsItsInterface()
    {
        if (!have("nano"))
            QSKIP("nano not installed", SkipSingle);
        Session s;
        QVERIFY(s.pty.start("nano", {"--ignorercfiles"}, {}, 80, 24));
        QVERIFY2(s.waitFor("GNU nano"), qPrintable(screenOnly(s.screen)));
        QVERIFY(s.text().contains("^X"));
        s.pty.write("\x18"); // Ctrl+X
        QVERIFY(s.waitFinished());
    }

    void htopDrawsAndQuits()
    {
        if (!have("htop"))
            QSKIP("htop not installed", SkipSingle);
        Session s(120, 35);
        QVERIFY(s.pty.start("htop", {}, {}, 120, 35));
        QVERIFY2(s.waitFor("PID", 8000), qPrintable(screenOnly(s.screen)));
        QVERIFY(s.screen.altScreen());
        s.pty.write("q");
        QVERIFY(s.waitFinished());
    }

    void zshAndFishPrompts()
    {
        for (const QString &shell : {QStringLiteral("zsh"), QStringLiteral("fish")}) {
            if (!have(shell))
                continue;
            Session s;
            const QStringList args = shell == "zsh" ? QStringList{"-f"} : QStringList{"--no-config"};
            QVERIFY(s.pty.start(shell, args, {}, 80, 24));
            s.pty.write("printf 'shell-%s-ok\\n' 42\r");
            QVERIFY2(s.waitFor("shell-42-ok", 8000), qPrintable(shell + ":\n" + s.text()));
            s.pty.write("exit\r");
            QVERIFY2(s.waitFinished(8000), qPrintable(shell + " did not exit:\n" + s.text()));
        }
    }

    void unicodeRoundTripThroughCat()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {}, {}, 80, 24));
        s.pty.write(QString::fromUtf8("echo 'привет ✓ 日本'\n").toUtf8());
        QVERIFY2(s.waitFor(QString::fromUtf8("привет ✓ 日本")), qPrintable(s.text()));
        s.pty.write("exit\n");
        QVERIFY(s.waitFinished());
    }
};

QTEST_GUILESS_MAIN(TestPrograms)
#include "test_programs.moc"
