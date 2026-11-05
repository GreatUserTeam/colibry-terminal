#include "term/Input.h"

#include <QtTest>

using namespace gt;

class TestInput : public QObject {
    Q_OBJECT

    static QByteArray key(int k, Qt::KeyboardModifiers m = Qt::NoModifier, const QString &text = {}, bool app = false)
    {
        QKeyEvent e(QEvent::KeyPress, k, m, text);
        return encodeKey(e, app);
    }

private slots:
    void plainText() { QCOMPARE(key(Qt::Key_A, Qt::NoModifier, "a"), QByteArray("a")); }
    void unicodeText() { QCOMPARE(key(Qt::Key_unknown, Qt::NoModifier, QString::fromUtf8("ж")), QString::fromUtf8("ж").toUtf8()); }
    void enterBackspaceTabEscape()
    {
        QCOMPARE(key(Qt::Key_Return), QByteArray("\r"));
        QCOMPARE(key(Qt::Key_Backspace), QByteArray("\x7f"));
        QCOMPARE(key(Qt::Key_Tab), QByteArray("\t"));
        QCOMPARE(key(Qt::Key_Backtab, Qt::ShiftModifier), QByteArray("\x1b[Z"));
        QCOMPARE(key(Qt::Key_Escape), QByteArray("\x1b"));
    }
    void controlLetters()
    {
        QCOMPARE(key(Qt::Key_C, Qt::ControlModifier, "\x03"), QByteArray("\x03"));
        QCOMPARE(key(Qt::Key_D, Qt::ControlModifier, "\x04"), QByteArray("\x04"));
        QCOMPARE(key(Qt::Key_Z, Qt::ControlModifier), QByteArray("\x1a"));
        QCOMPARE(key(Qt::Key_Space, Qt::ControlModifier, " "), QByteArray(1, '\0'));
    }
    void altPrefixesEscape() { QCOMPARE(key(Qt::Key_B, Qt::AltModifier, "b"), QByteArray("\x1b" "b")); }
    void arrowsNormalAndApplication()
    {
        QCOMPARE(key(Qt::Key_Up), QByteArray("\x1b[A"));
        QCOMPARE(key(Qt::Key_Up, Qt::NoModifier, {}, true), QByteArray("\x1bOA"));
        QCOMPARE(key(Qt::Key_Left, Qt::ControlModifier), QByteArray("\x1b[1;5D"));
        QCOMPARE(key(Qt::Key_Right, Qt::ShiftModifier | Qt::AltModifier), QByteArray("\x1b[1;4C"));
    }
    void navigationAndFunctionKeys()
    {
        QCOMPARE(key(Qt::Key_Delete), QByteArray("\x1b[3~"));
        QCOMPARE(key(Qt::Key_PageUp), QByteArray("\x1b[5~"));
        QCOMPARE(key(Qt::Key_Home), QByteArray("\x1b[H"));
        QCOMPARE(key(Qt::Key_F1), QByteArray("\x1bOP"));
        QCOMPARE(key(Qt::Key_F5), QByteArray("\x1b[15~"));
        QCOMPARE(key(Qt::Key_F12), QByteArray("\x1b[24~"));
    }
    void bareModifiersProduceNothing() { QVERIFY(key(Qt::Key_Shift, Qt::ShiftModifier).isEmpty()); }
    void pasteBracketed()
    {
        QCOMPARE(encodePaste("a\nb", true), QByteArray("\x1b[200~a\rb\x1b[201~"));
        QCOMPARE(encodePaste("a\r\nb", false), QByteArray("a\rb"));
    }
    void pasteCannotEscapeBrackets()
    {
        const QByteArray out = encodePaste(QString::fromUtf8("x\x1b[201~rm -rf"), true);
        QCOMPARE(out.count("\x1b[201~"), 1); // only the real terminator
    }
};

QTEST_GUILESS_MAIN(TestInput)
#include "test_input.moc"
