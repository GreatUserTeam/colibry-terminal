#pragma once

#include "Screen.h"

#include <QByteArray>

#include <vector>

namespace gt {

/// Byte-stream parser for VT100/ANSI/xterm sequences, following the state machine described by
/// Paul Williams (vt100.net/emu/dec_ansi_parser). It feeds a Screen and keeps its state across
/// calls, so sequences may be split between reads from the PTY.
class Parser {
public:
    explicit Parser(Screen &screen);

    void feed(const QByteArray &bytes);
    void reset();

private:
    enum class State { Ground, Escape, EscapeIntermediate, CsiEntry, CsiParam, CsiIntermediate, CsiIgnore, OscString, DcsIgnore };

    void handleByte(unsigned char b);
    void handleUtf8(unsigned char b);
    void clearSequence();
    void collectParam(unsigned char b);
    void dispatchCsi(unsigned char final);
    void dispatchEsc(unsigned char final);
    void dispatchOsc();

    Screen &m_screen;
    State m_state = State::Ground;

    std::vector<int> m_params;
    int m_currentParam = -1;
    bool m_paramStarted = false;
    QByteArray m_intermediates;
    char m_prefix = 0;
    QByteArray m_osc;

    // UTF-8 decoder state
    char32_t m_utf8 = 0;
    int m_utf8Remaining = 0;
};

} // namespace gt
