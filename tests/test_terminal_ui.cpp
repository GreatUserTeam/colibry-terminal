#include "ui/MainWindow.h"
#include "ui/SplitView.h"
#include "ui/TerminalWidget.h"

#include <QClipboard>
#include <QtTest>

using namespace gt;

namespace {
bool waitForText(TerminalWidget *t, const QString &needle, int ms = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) {
        for (int i = 0; i < t->screen().totalLines(); ++i)
            if (t->screen().lineText(i).contains(needle))
                return true;
        QTest::qWait(20);
    }
    return false;
}
} // namespace

class TestTerminalUi : public QObject {
    Q_OBJECT

    Config cfg() const
    {
        Config c;
        c.shell = "/bin/sh";
        return c;
    }

private slots:
    void gridFollowsWidgetSize()
    {
        TerminalWidget t(cfg());
        t.resize(800, 400);
        t.show();
        QTest::qWait(50);
        QVERIFY(t.columns() > 40);
        QVERIFY(t.rowCount() > 10);
        const int cols = t.columns();
        t.resize(500, 400);
        QTest::qWait(50);
        QVERIFY(t.columns() < cols);
    }

    void typingReachesShell()
    {
        TerminalWidget t(cfg());
        t.resize(800, 400);
        t.show();
        QVERIFY(t.start());
        t.setFocus();
        QTest::keyClicks(&t, "echo typed-$((1+2))");
        QTest::keyClick(&t, Qt::Key_Return);
        QVERIFY(waitForText(&t, "typed-3"));
    }

    void rendersWithoutCrashAndShowsColours()
    {
        TerminalWidget t(cfg());
        t.resize(640, 320);
        t.show();
        t.feed("\x1b[31mred\x1b[0m \x1b[48;2;0;80;160mblue-bg\x1b[0m \xE6\x97\xA5\xE6\x9C\xAC \x1b[1;4mB\x1b[0m\r\n");
        const QImage img = t.grab().toImage();
        QVERIFY(!img.isNull());
        // The first cell of "red" must contain pixels that differ from the background.
        const QColor bg = img.pixelColor(img.width() - 30, img.height() - 5);
        bool foundText = false;
        for (int y = 0; y < t.cellSize().height() && !foundText; ++y)
            for (int x = 0; x < t.cellSize().width() * 3; ++x)
                if (img.pixelColor(x, y) != bg) {
                    foundText = true;
                    break;
                }
        QVERIFY(foundText);
    }

    void ligaturesToggleBothRender()
    {
        Config c = cfg();
        TerminalWidget t(c);
        t.resize(400, 200);
        t.show();
        t.feed("-> != == >=\r\n");
        QVERIFY(!t.grab().isNull());
        c.ligatures = false;
        t.applyConfig(c);
        QVERIFY(!t.grab().isNull());
    }

    void selectionAndCopy()
    {
        TerminalWidget t(cfg());
        t.resize(640, 320);
        t.show();
        t.feed("hello world\r\n");
        QTest::mouseDClick(&t, Qt::LeftButton, Qt::NoModifier, QPoint(int(t.cellSize().width() * 2), int(t.cellSize().height() / 2)));
        QVERIFY(t.hasSelection());
        QCOMPARE(t.selectedText(), QString("hello"));
        t.copy();
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("hello"));
    }

    void searchFindsAndNavigates()
    {
        TerminalWidget t(cfg());
        t.resize(640, 200);
        t.show();
        for (int i = 0; i < 100; ++i)
            t.feed("row " + QByteArray::number(i) + " needle\r\n");
        QCOMPARE(t.setSearch("needle", false), 100);
        QVERIFY(t.findPrevious());
        QVERIFY(t.currentMatch() >= 0);
        t.setSearch("row 3 ", true);
        QCOMPARE(t.matchCount(), 1);
        // The match is in the history, so the view must have scrolled back to it.
        QVERIFY(t.screen().viewOffset() > 0);
        QCOMPARE(t.setSearch("no-such-text", false), 0);
        QVERIFY(!t.findNext());
        t.clearSearch();
        QCOMPARE(t.matchCount(), 0);
    }

    void scrollbackViaWheel()
    {
        TerminalWidget t(cfg());
        t.resize(640, 200);
        t.show();
        for (int i = 0; i < 100; ++i)
            t.feed("line\r\n");
        QCOMPARE(t.screen().viewOffset(), 0);
        QWheelEvent e(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&t, &e);
        QVERIFY(t.screen().viewOffset() > 0);
        t.scrollToBottom();
        QCOMPARE(t.screen().viewOffset(), 0);
    }

    void splitsAndClose()
    {
        SplitView v(cfg());
        v.resize(800, 500);
        v.show();
        QVERIFY(v.startInitial({}));
        QCOMPARE(v.paneCount(), 1);
        v.split(Qt::Horizontal, {});
        v.split(Qt::Vertical, {});
        QCOMPARE(v.paneCount(), 3);
        QVERIFY(!v.closeFocused());
        QTest::qWait(50);
        QCOMPARE(v.paneCount(), 2);
        QVERIFY(!v.closeFocused());
        QTest::qWait(50);
        QCOMPARE(v.paneCount(), 1);
        QVERIFY(v.closeFocused());
    }

    void exitingShellClosesPane()
    {
        SplitView v(cfg());
        v.resize(800, 500);
        v.show();
        QSignalSpy spy(&v, &SplitView::lastPaneClosed);
        QVERIFY(v.startInitial({}));
        v.focusedTerminal()->pty().write("exit\n");
        QVERIFY(spy.wait(5000));
    }

    void mainWindowTabsAndShortcuts()
    {
        QTemporaryDir dir;
        MainWindow w(cfg(), dir.filePath("config.json"));
        w.show();
        if (!QTest::qWaitForWindowActive(&w))
            QSKIP("the window cannot become active (no window manager); shortcuts need an active window", SkipSingle);
        w.newTab();
        QCOMPARE(w.tabCount(), 1);
        QTest::keyClick(&w, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(w.tabCount(), 2);
        QTest::keyClick(&w, Qt::Key_D, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(w.currentView()->paneCount(), 2);
        QTest::keyClick(&w, Qt::Key_W, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::qWait(50);
        QCOMPARE(w.currentView()->paneCount(), 1);
        QTest::keyClick(&w, Qt::Key_W, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::qWait(50);
        QCOMPARE(w.tabCount(), 1);
    }

    void copyPasteShortcuts()
    {
        QTemporaryDir dir;
        MainWindow w(cfg(), dir.filePath("config.json"));
        w.show();
        if (!QTest::qWaitForWindowActive(&w))
            QSKIP("the window cannot become active (no window manager); shortcuts need an active window", SkipSingle);
        w.newTab();
        TerminalWidget *t = w.currentTerminal();
        QVERIFY(t);
        QGuiApplication::clipboard()->setText("echo pasted-ok");
        QTest::keyClick(&w, Qt::Key_V, Qt::ControlModifier | Qt::ShiftModifier);
        QTest::keyClick(t, Qt::Key_Return);
        QVERIFY(waitForText(t, "pasted-ok"));
    }

    void searchBarShortcut()
    {
        QTemporaryDir dir;
        MainWindow w(cfg(), dir.filePath("config.json"));
        w.show();
        if (!QTest::qWaitForWindowActive(&w))
            QSKIP("the window cannot become active (no window manager); shortcuts need an active window", SkipSingle);
        w.newTab();
        w.currentTerminal()->feed("find-me\r\n");
        QTest::keyClick(&w, Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(w.searchBar()->isVisible());
        QTest::keyClicks(w.searchBar()->findChild<QLineEdit *>(), "find-me");
        QCOMPARE(w.currentTerminal()->matchCount(), 1);
    }

    void settingsAreSavedOnChange()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("config.json");
        MainWindow w(cfg(), path);
        w.show();
        w.newTab();
        w.zoom(3);
        QCOMPARE(Config::load(path).fontSize, 15);
    }
};

QTEST_MAIN(TestTerminalUi)
#include "test_terminal_ui.moc"
