#include "PtySession.h"

#include <QtTest>

using namespace gt;


class TestPty : public QObject {
    Q_OBJECT
private slots:
    void runsProgramAndReportsExitCode()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {"-c", "echo hello-from-pty; exit 7"}, {}, 80, 24));
        QVERIFY(s.waitFinished());
        QVERIFY(s.text().contains("hello-from-pty"));
        QCOMPARE(s.exitCode, 7);
        QVERIFY(!s.crashed);
    }

    void childSeesRealTerminalOfRightSize()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {"-c", "stty size; test -t 0 && echo is-a-tty; echo T=$TERM C=$COLORTERM"}, {}, 100, 30));
        QVERIFY(s.waitFinished());
        QVERIFY2(s.text().contains("30 100"), qPrintable(s.text()));
        QVERIFY(s.text().contains("is-a-tty"));
        QVERIFY(s.text().contains("T=xterm-256color C=truecolor"));
    }

    void resizeReachesTheChild()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {}, {}, 80, 24));
        s.pty.write("stty size\n");
        QVERIFY(s.waitFor("24 80"));
        s.pty.resize(120, 40);
        s.pty.write("stty size\n");
        QVERIFY2(s.waitFor("40 120"), qPrintable(s.text()));
        s.pty.write("exit\n");
        QVERIFY(s.waitFinished());
    }

    void workingDirectory()
    {
        QTemporaryDir dir;
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {"-c", "pwd"}, dir.path(), 80, 24));
        QVERIFY(s.waitFinished());
        QVERIFY2(s.text().contains(QFileInfo(dir.path()).canonicalFilePath()), qPrintable(s.text()));
    }

    void interactiveBashEchoesAndRunsCommands()
    {
        if (!QFileInfo("/bin/bash").exists())
            QSKIP("bash not installed", SkipSingle);
        Session s;
        QVERIFY(s.pty.start("/bin/bash", {"--norc", "--noprofile"}, {}, 80, 24));
        s.pty.write("echo $((6*7))\n");
        QVERIFY2(s.waitFor("42"), qPrintable(s.text()));
        s.pty.write("exit 3\n");
        QVERIFY(s.waitFinished());
        QCOMPARE(s.exitCode, 3);
    }

    void trueColourAndEscapesArriveIntact()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {"-c", "printf '\\033[38;2;255;100;0mX\\033[0m'"}, {}, 80, 24));
        QVERIFY(s.waitFinished());
        QCOMPARE(s.screen.cellAt(0, 0).attr.fg, Color::rgb(255, 100, 0));
    }

    void signalKilledChildIsReportedAsCrashed()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {"-c", "kill -9 $$"}, {}, 80, 24));
        QVERIFY(s.waitFinished());
        QVERIFY(s.crashed);
        QCOMPARE(s.exitCode, 9);
    }

    void missingProgramExits127()
    {
        Session s;
        QVERIFY(s.pty.start("/nonexistent/program", {}, {}, 80, 24));
        QVERIFY(s.waitFinished());
        QCOMPARE(s.exitCode, 127);
    }

    void largeOutputIsNotLost()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {"-c", "seq 1 20000; echo END-MARK"}, {}, 80, 24));
        QVERIFY(s.waitFinished(15000));
        QVERIFY(s.waitFor("END-MARK"));
        QVERIFY(s.text().contains("\n20000\n"));
        QVERIFY(s.screen.historyLines() == 1000); // older lines were trimmed by the limit
    }

    void ctrlCInterruptsForegroundProgram()
    {
        Session s;
        QVERIFY(s.pty.start("/bin/sh", {}, {}, 80, 24));
        s.pty.write("sleep 30; echo after-sleep\n");
        QTest::qWait(200);
        s.pty.write("\x03"); // the line discipline turns this into SIGINT
        s.pty.write("echo alive-after-int\n");
        QVERIFY2(s.waitFor("alive-after-int"), qPrintable(s.text()));
        QVERIFY(!s.text().contains("\nafter-sleep"));
        s.pty.write("exit\n");
        QVERIFY(s.waitFinished());
    }

    void defaultShellIsExecutable() { QVERIFY(QFileInfo(Pty::defaultShell()).isExecutable()); }
};

QTEST_GUILESS_MAIN(TestPty)
#include "test_pty.moc"
