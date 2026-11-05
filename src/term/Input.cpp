#include "Input.h"

namespace gt {

namespace {

/// xterm modifier parameter: 1 + shift(1) + alt(2) + ctrl(4).
int modifierParam(Qt::KeyboardModifiers m)
{
    int v = 1;
    if (m & Qt::ShiftModifier) v += 1;
    if (m & Qt::AltModifier) v += 2;
    if (m & Qt::ControlModifier) v += 4;
    return v;
}

QByteArray csiKey(char final, Qt::KeyboardModifiers m, bool app)
{
    const int mod = modifierParam(m);
    if (mod > 1)
        return "\x1b[1;" + QByteArray::number(mod) + final;
    return QByteArray(app ? "\x1bO" : "\x1b[") + final;
}

QByteArray tildeKey(int code, Qt::KeyboardModifiers m)
{
    const int mod = modifierParam(m);
    if (mod > 1)
        return "\x1b[" + QByteArray::number(code) + ';' + QByteArray::number(mod) + '~';
    return "\x1b[" + QByteArray::number(code) + '~';
}

} // namespace

QByteArray encodeKey(const QKeyEvent &e, bool app)
{
    const Qt::KeyboardModifiers m = e.modifiers() & (Qt::ShiftModifier | Qt::AltModifier | Qt::ControlModifier);

    switch (e.key()) {
    case Qt::Key_Up: return csiKey('A', m, app);
    case Qt::Key_Down: return csiKey('B', m, app);
    case Qt::Key_Right: return csiKey('C', m, app);
    case Qt::Key_Left: return csiKey('D', m, app);
    case Qt::Key_Home: return csiKey('H', m, app);
    case Qt::Key_End: return csiKey('F', m, app);
    case Qt::Key_Insert: return tildeKey(2, m);
    case Qt::Key_Delete: return tildeKey(3, m);
    case Qt::Key_PageUp: return tildeKey(5, m);
    case Qt::Key_PageDown: return tildeKey(6, m);
    case Qt::Key_F1: return m ? "\x1b[1;" + QByteArray::number(modifierParam(m)) + 'P' : QByteArray("\x1bOP");
    case Qt::Key_F2: return m ? "\x1b[1;" + QByteArray::number(modifierParam(m)) + 'Q' : QByteArray("\x1bOQ");
    case Qt::Key_F3: return m ? "\x1b[1;" + QByteArray::number(modifierParam(m)) + 'R' : QByteArray("\x1bOR");
    case Qt::Key_F4: return m ? "\x1b[1;" + QByteArray::number(modifierParam(m)) + 'S' : QByteArray("\x1bOS");
    case Qt::Key_F5: return tildeKey(15, m);
    case Qt::Key_F6: return tildeKey(17, m);
    case Qt::Key_F7: return tildeKey(18, m);
    case Qt::Key_F8: return tildeKey(19, m);
    case Qt::Key_F9: return tildeKey(20, m);
    case Qt::Key_F10: return tildeKey(21, m);
    case Qt::Key_F11: return tildeKey(23, m);
    case Qt::Key_F12: return tildeKey(24, m);
    case Qt::Key_Backtab: return "\x1b[Z";
    case Qt::Key_Tab: return (m & Qt::ShiftModifier) ? QByteArray("\x1b[Z") : QByteArray("\t");
    case Qt::Key_Return:
    case Qt::Key_Enter: return (m & Qt::AltModifier) ? QByteArray("\x1b\r") : QByteArray("\r");
    case Qt::Key_Backspace:
        if (m & Qt::ControlModifier) return "\x08";
        return (m & Qt::AltModifier) ? QByteArray("\x1b\x7f") : QByteArray("\x7f");
    case Qt::Key_Escape: return "\x1b";
    default: break;
    }

    // Ctrl+letter and friends produce C0 control codes.
    if ((m & Qt::ControlModifier) && !(m & Qt::AltModifier)) {
        const int k = e.key();
        if (k >= Qt::Key_A && k <= Qt::Key_Z)
            return QByteArray(1, static_cast<char>(k - Qt::Key_A + 1));
        switch (k) {
        case Qt::Key_Space:
        case Qt::Key_At: return QByteArray(1, '\0');
        case Qt::Key_BracketLeft: return "\x1b";
        case Qt::Key_Backslash: return "\x1c";
        case Qt::Key_BracketRight: return "\x1d";
        case Qt::Key_AsciiCircum:
        case Qt::Key_6: return "\x1e";
        case Qt::Key_Underscore:
        case Qt::Key_Minus: return "\x1f";
        default: break;
        }
    }

    const QString text = e.text();
    if (text.isEmpty() || text.at(0).unicode() < 0x20)
        return {};
    QByteArray bytes = text.toUtf8();
    if (m & Qt::AltModifier)
        bytes.prepend('\x1b'); // Alt sends an escape prefix (meta)
    return bytes;
}

QByteArray encodePaste(const QString &text, bool bracketed)
{
    QString t = text;
    t.replace(QStringLiteral("\r\n"), QStringLiteral("\r"));
    t.replace(QLatin1Char('\n'), QLatin1Char('\r'));
    QByteArray data = t.toUtf8();
    if (bracketed) {
        // The end marker must never be injected from inside the pasted text.
        data.replace("\x1b[201~", "");
        return "\x1b[200~" + data + "\x1b[201~";
    }
    return data;
}

} // namespace gt
