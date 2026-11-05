#include "Parser.h"

#include <algorithm>

namespace gt {

namespace {
constexpr qsizetype kMaxOsc = 4096; // longer strings are dropped, as xterm does
constexpr int kMaxParam = 1000000;
} // namespace

Parser::Parser(Screen &screen)
    : m_screen(screen)
{
}

void Parser::reset()
{
    m_state = State::Ground;
    clearSequence();
    m_osc.clear();
    m_utf8Remaining = 0;
}

void Parser::clearSequence()
{
    m_params.clear();
    m_currentParam = -1;
    m_paramStarted = false;
    m_intermediates.clear();
    m_prefix = 0;
}

void Parser::feed(const QByteArray &bytes)
{
    for (char c : bytes)
        handleByte(static_cast<unsigned char>(c));
}

void Parser::handleUtf8(unsigned char b)
{
    if (m_utf8Remaining == 0) {
        if (b < 0x80) {
            m_screen.print(b);
        } else if ((b & 0xE0) == 0xC0) {
            m_utf8 = b & 0x1F;
            m_utf8Remaining = 1;
        } else if ((b & 0xF0) == 0xE0) {
            m_utf8 = b & 0x0F;
            m_utf8Remaining = 2;
        } else if ((b & 0xF8) == 0xF0) {
            m_utf8 = b & 0x07;
            m_utf8Remaining = 3;
        } else {
            m_screen.print(U'�'); // stray continuation byte
        }
        return;
    }
    if ((b & 0xC0) != 0x80) {
        // Broken sequence: show a replacement character and start over with this byte.
        m_screen.print(U'�');
        m_utf8Remaining = 0;
        handleUtf8(b);
        return;
    }
    m_utf8 = (m_utf8 << 6) | (b & 0x3F);
    if (--m_utf8Remaining == 0)
        m_screen.print(m_utf8);
}

void Parser::collectParam(unsigned char b)
{
    if (b == ';' || b == ':') {
        m_params.push_back(m_paramStarted ? m_currentParam : -1);
        m_currentParam = -1;
        m_paramStarted = false;
        return;
    }
    if (m_currentParam < 0)
        m_currentParam = 0;
    m_currentParam = std::min(m_currentParam * 10 + (b - '0'), kMaxParam);
    m_paramStarted = true;
}

void Parser::dispatchCsi(unsigned char final)
{
    if (m_paramStarted || !m_params.empty())
        m_params.push_back(m_paramStarted ? m_currentParam : -1);
    // A lone empty parameter list means "no parameters" (defaults apply), so drop a single -1.
    if (m_params.size() == 1 && m_params[0] == -1)
        m_params.clear();
    m_screen.csi(m_params, m_intermediates, m_prefix, static_cast<char>(final));
}

void Parser::dispatchEsc(unsigned char final)
{
    m_screen.escape(m_intermediates, static_cast<char>(final));
}

void Parser::dispatchOsc()
{
    m_screen.oscDispatch(m_osc);
    m_osc.clear();
}

void Parser::handleByte(unsigned char b)
{
    // "Anywhere" transitions: these interrupt any sequence.
    if (b == 0x18 || b == 0x1A) { // CAN / SUB
        m_state = State::Ground;
        clearSequence();
        return;
    }
    if (b == 0x1B) { // ESC
        if (m_state == State::OscString || m_state == State::DcsIgnore) {
            // ESC inside OSC/DCS may start the ST terminator (ESC \); handled below by the
            // next byte. Ending OSC here keeps broken strings from swallowing the screen.
        }
        if (m_state == State::OscString)
            dispatchOsc();
        m_state = State::Escape;
        clearSequence();
        return;
    }

    switch (m_state) {
    case State::Ground:
        if (b < 0x20) {
            m_screen.execute(b);
        } else if (b == 0x7F) {
            // DEL is ignored
        } else {
            handleUtf8(b);
        }
        return;

    case State::Escape:
        if (b < 0x20) {
            m_screen.execute(b);
            return;
        }
        if (b >= 0x20 && b <= 0x2F) {
            m_intermediates.push_back(static_cast<char>(b));
            m_state = State::EscapeIntermediate;
            return;
        }
        if (b == '[') {
            m_state = State::CsiEntry;
            return;
        }
        if (b == ']') {
            m_osc.clear();
            m_state = State::OscString;
            return;
        }
        if (b == 'P' || b == 'X' || b == '^' || b == '_') { // DCS, SOS, PM, APC: swallowed
            m_state = State::DcsIgnore;
            return;
        }
        if (b == '\\') { // ST without a string
            m_state = State::Ground;
            return;
        }
        if (b >= 0x30 && b <= 0x7E) {
            dispatchEsc(b);
            m_state = State::Ground;
        }
        return;

    case State::EscapeIntermediate:
        if (b < 0x20) {
            m_screen.execute(b);
            return;
        }
        if (b >= 0x20 && b <= 0x2F) {
            m_intermediates.push_back(static_cast<char>(b));
            return;
        }
        if (b >= 0x30 && b <= 0x7E)
            dispatchEsc(b);
        m_state = State::Ground;
        return;

    case State::CsiEntry:
        if (b < 0x20) {
            m_screen.execute(b);
            return;
        }
        if (b >= 0x30 && b <= 0x39) {
            collectParam(b);
            m_state = State::CsiParam;
            return;
        }
        if (b == ';' || b == ':') {
            collectParam(b);
            m_state = State::CsiParam;
            return;
        }
        if (b >= 0x3C && b <= 0x3F) { // < = > ?
            m_prefix = static_cast<char>(b);
            m_state = State::CsiParam;
            return;
        }
        if (b >= 0x20 && b <= 0x2F) {
            m_intermediates.push_back(static_cast<char>(b));
            m_state = State::CsiIntermediate;
            return;
        }
        if (b >= 0x40 && b <= 0x7E) {
            dispatchCsi(b);
            m_state = State::Ground;
        }
        return;

    case State::CsiParam:
        if (b < 0x20) {
            m_screen.execute(b);
            return;
        }
        if ((b >= 0x30 && b <= 0x39) || b == ';' || b == ':') {
            collectParam(b);
            return;
        }
        if (b >= 0x3C && b <= 0x3F) {
            m_state = State::CsiIgnore;
            return;
        }
        if (b >= 0x20 && b <= 0x2F) {
            m_intermediates.push_back(static_cast<char>(b));
            m_state = State::CsiIntermediate;
            return;
        }
        if (b >= 0x40 && b <= 0x7E) {
            dispatchCsi(b);
            m_state = State::Ground;
        }
        return;

    case State::CsiIntermediate:
        if (b < 0x20) {
            m_screen.execute(b);
            return;
        }
        if (b >= 0x20 && b <= 0x2F) {
            m_intermediates.push_back(static_cast<char>(b));
            return;
        }
        if (b >= 0x40 && b <= 0x7E) {
            dispatchCsi(b);
            m_state = State::Ground;
            return;
        }
        m_state = State::CsiIgnore;
        return;

    case State::CsiIgnore:
        if (b >= 0x40 && b <= 0x7E)
            m_state = State::Ground;
        else if (b < 0x20)
            m_screen.execute(b);
        return;

    case State::OscString:
        if (b == 0x07) { // BEL terminates the string
            dispatchOsc();
            m_state = State::Ground;
            return;
        }
        if (b == '\\') { // ST (ESC \) arrives as ESC first; the ESC already ended the string
            m_state = State::Ground;
            return;
        }
        if (b >= 0x20 || b == '\t') {
            if (m_osc.size() < kMaxOsc)
                m_osc.push_back(static_cast<char>(b));
        }
        return;

    case State::DcsIgnore:
        // Strings are ignored until ST (ESC \) or BEL.
        if (b == 0x07 || b == '\\')
            m_state = State::Ground;
        return;
    }
}

} // namespace gt
