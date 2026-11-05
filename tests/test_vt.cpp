#include "term/Parser.h"
#include "term/Screen.h"

#include <QtTest>

using namespace gt;

class TestVt : public QObject {
    Q_OBJECT

    static QString row(const Screen &s, int y) { return s.lineText(s.historyLines() + y); }

private slots:
    void printsText()
    {
        Screen s(20, 5, 100);
        Parser p(s);
        p.feed("Hello, мир");
        QCOMPARE(row(s, 0), QString::fromUtf8("Hello, мир"));
        QCOMPARE(s.cursorX(), 10);
    }

    void utf8SplitAcrossReads()
    {
        Screen s(20, 5, 100);
        Parser p(s);
        const QByteArray bytes = QString::fromUtf8("€").toUtf8();
        p.feed(bytes.left(1));
        p.feed(bytes.mid(1));
        QCOMPARE(row(s, 0), QString::fromUtf8("€"));
    }

    void newlineAndCarriageReturn()
    {
        Screen s(20, 5, 100);
        Parser p(s);
        p.feed("ab\r\ncd");
        QCOMPARE(row(s, 0), QString("ab"));
        QCOMPARE(row(s, 1), QString("cd"));
    }

    void autowrapMarksLine()
    {
        Screen s(5, 3, 100);
        Parser p(s);
        p.feed("abcdefgh");
        QCOMPARE(row(s, 0), QString("abcde"));
        QCOMPARE(row(s, 1), QString("fgh"));
        QVERIFY(s.lineAt(s.historyLines()).wrapped);
        // A wrapped line is copied without a newline in the middle.
        QCOMPARE(s.textBetween(s.historyLines(), 0, s.historyLines() + 1, 2), QString("abcdefgh"));
    }

    void cursorMovement()
    {
        Screen s(20, 5, 100);
        Parser p(s);
        p.feed("\x1b[3;5H");
        QCOMPARE(s.cursorY(), 2);
        QCOMPARE(s.cursorX(), 4);
        p.feed("\x1b[2A\x1b[3D");
        QCOMPARE(s.cursorY(), 0);
        QCOMPARE(s.cursorX(), 1);
        p.feed("\x1b[H");
        QCOMPARE(s.cursorX(), 0);
        QCOMPARE(s.cursorY(), 0);
    }

    void eraseInLineAndDisplay()
    {
        Screen s(10, 3, 100);
        Parser p(s);
        p.feed("0123456789\r\nabcdefghij");
        p.feed("\x1b[1;4H\x1b[K");
        QCOMPARE(row(s, 0), QString("012"));
        p.feed("\x1b[2J");
        QCOMPARE(row(s, 1), QString(""));
    }

    void scrollbackGrows()
    {
        Screen s(10, 3, 100);
        Parser p(s);
        for (int i = 0; i < 10; ++i)
            p.feed(QByteArray("line") + QByteArray::number(i) + "\r\n");
        QVERIFY(s.historyLines() >= 8);
        QCOMPARE(s.lineText(0), QString("line0"));
    }

    void historyLimitIsRespected()
    {
        Screen s(10, 3, 5);
        Parser p(s);
        for (int i = 0; i < 50; ++i)
            p.feed("x\r\n");
        QCOMPARE(s.historyLines(), 5);
    }

    void sgrColours()
    {
        Screen s(20, 3, 100);
        Parser p(s);
        p.feed("\x1b[31mR\x1b[0m\x1b[38;5;200mI\x1b[38;2;10;20;30mT\x1b[1;4mB\x1b[0mN");
        const int l = s.historyLines();
        QCOMPARE(s.cellAt(l, 0).attr.fg, Color::indexed(1));
        QCOMPARE(s.cellAt(l, 1).attr.fg, Color::indexed(200));
        QCOMPARE(s.cellAt(l, 2).attr.fg, Color::rgb(10, 20, 30));
        QVERIFY(s.cellAt(l, 3).attr.has(Bold));
        QVERIFY(s.cellAt(l, 3).attr.has(Underline));
        QCOMPARE(s.cellAt(l, 4).attr.flags, std::uint16_t(0));
        QCOMPARE(s.cellAt(l, 4).attr.fg, Color{});
    }

    void sgrColonFormTrueColour()
    {
        Screen s(20, 3, 100);
        Parser p(s);
        p.feed("\x1b[48:2::1:2:3mX");
        QCOMPARE(s.cellAt(s.historyLines(), 0).attr.bg, Color::rgb(1, 2, 3));
    }

    void brightColours()
    {
        Screen s(20, 3, 100);
        Parser p(s);
        p.feed("\x1b[91;102mX");
        const Cell &c = s.cellAt(s.historyLines(), 0);
        QCOMPARE(c.attr.fg, Color::indexed(9));
        QCOMPARE(c.attr.bg, Color::indexed(10));
    }

    void scrollRegion()
    {
        Screen s(10, 5, 100);
        Parser p(s);
        p.feed("1\r\n2\r\n3\r\n4\r\n5");
        p.feed("\x1b[2;4r"); // lines 2..4 scroll
        p.feed("\x1b[4;1H\n");
        QCOMPARE(row(s, 0), QString("1"));
        QCOMPARE(row(s, 1), QString("3"));
        QCOMPARE(row(s, 2), QString("4"));
        QCOMPARE(row(s, 3), QString(""));
        QCOMPARE(row(s, 4), QString("5"));
        QCOMPARE(s.historyLines(), 0); // a partial region never feeds the history
    }

    void alternateScreenRestoresContent()
    {
        Screen s(10, 3, 100);
        Parser p(s);
        p.feed("main");
        p.feed("\x1b[?1049h");
        QVERIFY(s.altScreen());
        p.feed("\x1b[Halt");
        QCOMPARE(row(s, 0), QString("alt"));
        p.feed("\x1b[?1049l");
        QVERIFY(!s.altScreen());
        QCOMPARE(row(s, 0), QString("main"));
        QCOMPARE(s.cursorX(), 4);
    }

    void decLineDrawing()
    {
        Screen s(10, 3, 100);
        Parser p(s);
        p.feed("\x1b(0lqk\x1b(B");
        QCOMPARE(row(s, 0), QString::fromUtf8("┌─┐"));
    }

    void wideCharactersTakeTwoCells()
    {
        Screen s(10, 3, 100);
        Parser p(s);
        p.feed(QString::fromUtf8("日本a").toUtf8());
        const int l = s.historyLines();
        QVERIFY(s.cellAt(l, 0).attr.has(WideLead));
        QVERIFY(s.cellAt(l, 1).attr.has(WideTail));
        QCOMPARE(s.cellAt(l, 4).ch, char32_t(U'a'));
        QCOMPARE(row(s, 0), QString::fromUtf8("日本a"));
    }

    void repeatPrecedingCharacter()
    {
        Screen s(20, 3, 100);
        Parser p(s);
        p.feed("x\x1b[4b");
        QCOMPARE(row(s, 0), QString("xxxxx"));
    }

    void insertDeleteChars()
    {
        Screen s(10, 3, 100);
        Parser p(s);
        p.feed("abcdef\x1b[1;3H\x1b[2@");
        QCOMPARE(row(s, 0), QString("ab  cdef"));
        p.feed("\x1b[3P");
        QCOMPARE(row(s, 0), QString("abdef"));
    }

    void insertDeleteLines()
    {
        Screen s(10, 4, 100);
        Parser p(s);
        p.feed("a\r\nb\r\nc\r\nd\x1b[2;1H\x1b[L");
        QCOMPARE(row(s, 1), QString(""));
        QCOMPARE(row(s, 2), QString("b"));
        p.feed("\x1b[M");
        QCOMPARE(row(s, 1), QString("b"));
    }

    void cursorPositionReport()
    {
        Screen s(10, 4, 100);
        Parser p(s);
        QByteArray reply;
        s.setReplyHandler([&](const QByteArray &b) { reply += b; });
        p.feed("\x1b[3;7H\x1b[6n");
        QCOMPARE(reply, QByteArray("\x1b[3;7R"));
        reply.clear();
        p.feed("\x1b[c");
        QVERIFY(reply.startsWith("\x1b[?"));
    }

    void titleViaOscBelAndSt()
    {
        Screen s(10, 4, 100);
        Parser p(s);
        QStringList titles;
        s.setTitleHandler([&](const QString &t) { titles << t; });
        p.feed("\x1b]0;first\x07");
        p.feed("\x1b]2;second\x1b\\");
        p.feed("ok");
        QCOMPARE(titles, (QStringList{"first", "second"}));
        QCOMPARE(row(s, 0), QString("ok"));
    }

    void unknownSequencesAreSwallowed()
    {
        Screen s(20, 4, 100);
        Parser p(s);
        p.feed("a\x1bP1$rjunk\x1b\\b\x1b[?9999hc\x1b[>0cd\x1b_apc\x1b\\e");
        QCOMPARE(row(s, 0), QString("abcde"));
    }

    void cursorVisibilityAndBracketedPaste()
    {
        Screen s(10, 4, 100);
        Parser p(s);
        p.feed("\x1b[?25l\x1b[?2004h\x1b[?1h");
        QVERIFY(!s.cursorVisible());
        QVERIFY(s.bracketedPaste());
        QVERIFY(s.applicationCursorKeys());
        p.feed("\x1b[?25l\x1b[?25h\x1b[?2004l");
        QVERIFY(s.cursorVisible());
        QVERIFY(!s.bracketedPaste());
    }

    void backspaceTabAndBell()
    {
        Screen s(20, 3, 100);
        Parser p(s);
        int bells = 0;
        s.setBellHandler([&] { ++bells; });
        p.feed("ab\b\bX\tY\a");
        QCOMPARE(row(s, 0), QString("Xb      Y"));
        QCOMPARE(bells, 1);
    }

    void resizeKeepsBottomAndCursor()
    {
        Screen s(10, 5, 100);
        Parser p(s);
        p.feed("1\r\n2\r\n3\r\n4\r\n5");
        s.resize(10, 3);
        QCOMPARE(s.rows(), 3);
        QCOMPARE(row(s, 2), QString("5"));
        QCOMPARE(s.cursorY(), 2);
        QCOMPARE(s.lineText(0), QString("1"));
        s.resize(6, 3);
        QCOMPARE(s.cols(), 6);
    }

    void searchFindsInHistoryAndScreen()
    {
        Screen s(20, 3, 100);
        Parser p(s);
        for (int i = 0; i < 10; ++i)
            p.feed(QByteArray("needle ") + QByteArray::number(i) + "\r\n");
        const auto matches = s.findAll("NEEDLE");
        QCOMPARE(int(matches.size()), 10);
        QCOMPARE(int(s.findAll("NEEDLE", Qt::CaseSensitive).size()), 0);
        QCOMPARE(matches[0].line, 0);
        QCOMPARE(matches[0].length, 6);
        QCOMPARE(int(s.findAll("needle 7").size()), 1);
    }

    void searchColumnAccountsForWideCharacters()
    {
        Screen s(20, 3, 100);
        Parser p(s);
        p.feed(QString::fromUtf8("日本 abc").toUtf8());
        const auto m = s.findAll("abc");
        QCOMPARE(int(m.size()), 1);
        QCOMPARE(m[0].col, 5);
    }

    void viewOffsetFollowsOutput()
    {
        Screen s(10, 3, 100);
        Parser p(s);
        for (int i = 0; i < 20; ++i)
            p.feed("x\r\n");
        s.setViewOffset(5);
        const int top = s.viewTopLine();
        p.feed("y\r\n");
        QCOMPARE(s.viewTopLine(), top); // the same text stays in view while output arrives
        s.setViewOffset(0);
        QCOMPARE(s.viewOffset(), 0);
    }

    void fuzzDoesNotCrash()
    {
        Screen s(40, 12, 50);
        Parser p(s);
        QRandomGenerator rng(12345);
        for (int round = 0; round < 300; ++round) {
            QByteArray junk;
            for (int i = 0; i < 200; ++i)
                junk.append(static_cast<char>(rng.bounded(256)));
            p.feed(junk);
            if (round % 50 == 0)
                s.resize(10 + rng.bounded(60), 3 + rng.bounded(30));
        }
        QVERIFY(s.cursorX() >= 0 && s.cursorX() < s.cols());
        QVERIFY(s.cursorY() >= 0 && s.cursorY() < s.rows());
    }
};

QTEST_GUILESS_MAIN(TestVt)
#include "test_vt.moc"
