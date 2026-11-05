#include "Screen.h"

#include <QString>

#include <algorithm>

namespace gt {

namespace {

/// Number of columns a code point occupies: 0 (combining / control), 1 or 2 (East Asian wide, emoji).
int charWidth(char32_t cp)
{
    if (cp == 0)
        return 0;
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
        return 0;
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x200B && cp <= 0x200F) || (cp >= 0xFE00 && cp <= 0xFE0F))
        return 0;
    const bool wide = (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7A3)
                      || (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE30 && cp <= 0xFE4F) || (cp >= 0xFF00 && cp <= 0xFF60)
                      || (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1F64F)
                      || (cp >= 0x1F900 && cp <= 0x1F9FF) || (cp >= 0x20000 && cp <= 0x3FFFD);
    return wide ? 2 : 1;
}

/// DEC special graphics ("line drawing") set, selected by ESC ( 0. Used by ncurses programs for boxes.
char32_t decGraphic(char32_t cp)
{
    static const char32_t table[] = {
        U' ',      U'◆', U'▒', U'␉', U'␌', U'␍', U'␊', U'°', // 0x5f-0x66
        U'±', U'␤', U'␋', U'┘', U'┐', U'┌', U'└', U'┼', // 0x67-0x6e
        U'⎺', U'⎻', U'─', U'⎼', U'⎽', U'├', U'┤', U'┴', // 0x6f-0x76
        U'┬', U'│', U'≤', U'≥', U'π', U'≠', U'£', U'·', // 0x77-0x7e
    };
    if (cp < 0x5F || cp > 0x7E)
        return cp;
    return table[cp - 0x5F];
}

constexpr int kTabWidth = 8;

} // namespace

Screen::Screen(int cols, int rows, int historyLimit)
    : m_cols(std::max(1, cols))
    , m_rows(std::max(1, rows))
    , m_historyLimit(std::max(0, historyLimit))
{
    m_grid.assign(m_rows, blankLine());
    m_altGrid.assign(m_rows, blankLine());
    m_bottom = m_rows - 1;
    m_tabStops.assign(m_cols, false);
    for (int c = kTabWidth; c < m_cols; c += kTabWidth)
        m_tabStops[c] = true;
}

Cell Screen::blankCell() const
{
    Cell c;
    c.attr.bg = m_cur.bg; // erased cells take the current background colour
    return c;
}

Line Screen::blankLine() const
{
    Line l;
    l.cells.assign(m_cols, blankCell());
    return l;
}

void Screen::resize(int cols, int rows)
{
    cols = std::max(1, cols);
    rows = std::max(1, rows);
    const int oldRows = m_rows;

    // New cells and lines must already have the new width, so switch the size first.
    m_cols = cols;
    for (auto *g : {&m_grid, &m_altGrid}) {
        for (Line &l : *g)
            l.cells.resize(cols, blankCell());
    }

    if (rows < oldRows && !m_alt) {
        // Keep the bottom of the screen: the lines that no longer fit go into the history.
        const int excess = oldRows - rows;
        for (int i = 0; i < excess && i < static_cast<int>(m_grid.size()); ++i) {
            if (i < m_cy)
                pushHistory(m_grid[i]);
        }
        m_grid.erase(m_grid.begin(), m_grid.begin() + std::min<int>(excess, static_cast<int>(m_grid.size())));
        m_cy = std::max(0, m_cy - excess);
    }
    m_rows = rows;
    m_grid.resize(rows, blankLine());
    m_altGrid.resize(rows, blankLine());

    m_tabStops.assign(m_cols, false);
    for (int c = kTabWidth; c < m_cols; c += kTabWidth)
        m_tabStops[c] = true;
    m_top = 0;
    m_bottom = m_rows - 1;
    m_cx = std::clamp(m_cx, 0, m_cols - 1);
    m_cy = std::clamp(m_cy, 0, m_rows - 1);
    m_pendingWrap = false;
    m_savedCursor.x = std::min(m_savedCursor.x, m_cols - 1);
    m_savedCursor.y = std::min(m_savedCursor.y, m_rows - 1);
    setViewOffset(m_viewOffset);
}

void Screen::setHistoryLimit(int lines)
{
    m_historyLimit = std::max(0, lines);
    while (static_cast<int>(m_history.size()) > m_historyLimit)
        m_history.pop_front();
    setViewOffset(m_viewOffset);
}

// ---------------------------------------------------------------------------
// Lines and the viewport
// ---------------------------------------------------------------------------

int Screen::historyLines() const
{
    return m_alt ? 0 : static_cast<int>(m_history.size());
}

int Screen::totalLines() const
{
    return historyLines() + m_rows;
}

void Screen::setViewOffset(int lines)
{
    m_viewOffset = m_alt ? 0 : std::clamp(lines, 0, historyLines());
}

int Screen::viewTopLine() const
{
    return std::max(0, totalLines() - m_rows - m_viewOffset);
}

const Line &Screen::lineAt(int absLine) const
{
    static const Line empty;
    if (absLine < 0 || absLine >= totalLines())
        return empty;
    if (m_alt)
        return m_altGrid[absLine];
    const int hist = static_cast<int>(m_history.size());
    if (absLine < hist)
        return m_history[absLine];
    return m_grid[absLine - hist];
}

const Cell &Screen::cellAt(int absLine, int col) const
{
    static const Cell blank;
    const Line &l = lineAt(absLine);
    if (col < 0 || col >= static_cast<int>(l.cells.size()))
        return blank;
    return l.cells[col];
}

QString Screen::lineText(int absLine) const
{
    QString text;
    for (const Cell &c : lineAt(absLine).cells) {
        if (c.attr.has(WideTail) || c.ch == 0)
            continue;
        text += QString::fromUcs4(&c.ch, 1);
    }
    while (text.endsWith(QLatin1Char(' ')))
        text.chop(1);
    return text;
}

// ---------------------------------------------------------------------------
// Search and text extraction
// ---------------------------------------------------------------------------

std::vector<Match> Screen::findAll(const QString &needle, Qt::CaseSensitivity sensitivity) const
{
    std::vector<Match> result;
    if (needle.isEmpty())
        return result;

    for (int abs = 0; abs < totalLines(); ++abs) {
        // Build the line text and remember which column each UTF-16 unit came from.
        QString text;
        std::vector<int> colOf;
        for (int col = 0; col < m_cols; ++col) {
            const Cell &c = cellAt(abs, col);
            if (c.attr.has(WideTail) || c.ch == 0)
                continue;
            const QString unit = QString::fromUcs4(&c.ch, 1);
            text += unit;
            for (qsizetype i = 0; i < unit.size(); ++i)
                colOf.push_back(col);
        }
        qsizetype from = 0;
        while (true) {
            const qsizetype pos = text.indexOf(needle, from, sensitivity);
            if (pos < 0)
                break;
            const int startCol = colOf[pos];
            const int endCol = colOf[pos + needle.size() - 1];
            result.push_back({abs, startCol, endCol - startCol + 1});
            from = pos + 1;
        }
    }
    return result;
}

QString Screen::textBetween(int startLine, int startCol, int endLine, int endCol) const
{
    QString out;
    for (int abs = startLine; abs <= endLine && abs < totalLines(); ++abs) {
        const int from = abs == startLine ? startCol : 0;
        const int to = abs == endLine ? endCol : m_cols - 1;
        QString part;
        for (int col = std::max(0, from); col <= std::min(to, m_cols - 1); ++col) {
            const Cell &c = cellAt(abs, col);
            if (c.attr.has(WideTail) || c.ch == 0)
                continue;
            part += QString::fromUcs4(&c.ch, 1);
        }
        while (part.endsWith(QLatin1Char(' ')) && abs != endLine)
            part.chop(1);
        out += part;
        if (abs != endLine && !lineAt(abs).wrapped)
            out += QLatin1Char('\n');
    }
    return out;
}

void Screen::clearHistory()
{
    m_history.clear();
    m_viewOffset = 0;
}

void Screen::reset()
{
    m_grid.assign(m_rows, blankLine());
    m_altGrid.assign(m_rows, blankLine());
    m_history.clear();
    m_alt = false;
    m_cx = m_cy = 0;
    m_pendingWrap = false;
    m_cur = Attr{};
    m_top = 0;
    m_bottom = m_rows - 1;
    m_autowrap = true;
    m_originMode = false;
    m_insertMode = false;
    m_cursorVisible = true;
    m_appCursor = false;
    m_bracketedPaste = false;
    m_graphics = false;
    m_viewOffset = 0;
    for (int c = 0; c < m_cols; ++c)
        m_tabStops[c] = (c % kTabWidth == 0 && c > 0);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Screen::print(char32_t cp)
{
    m_lastPrinted = cp;
    if (m_graphics)
        cp = decGraphic(cp);
    const int w = charWidth(cp);
    if (w == 0)
        return; // combining marks are dropped; the terminal keeps the base character

    if (m_pendingWrap) {
        m_pendingWrap = false;
        newlineWrapped();
    }
    if (m_cx + w > m_cols) {
        if (m_autowrap)
            newlineWrapped();
        else
            m_cx = m_cols - w;
    }

    auto &line = grid()[m_cy];
    if (m_insertMode)
        insertChars(w);

    Attr lead = m_cur;
    lead.flags = static_cast<std::uint16_t>(lead.flags | (w == 2 ? WideLead : 0));
    line.cells[m_cx] = Cell{cp, lead};
    if (w == 2) {
        Attr tail = m_cur;
        tail.flags = static_cast<std::uint16_t>(tail.flags | WideTail);
        line.cells[m_cx + 1] = Cell{0, tail};
    }

    m_cx += w;
    if (m_cx >= m_cols) {
        m_cx = m_cols - 1;
        if (m_autowrap)
            m_pendingWrap = true;
    }
}

void Screen::execute(unsigned char c)
{
    switch (c) {
    case 0x07: // BEL
        if (m_bell)
            m_bell();
        break;
    case 0x08: // BS
        m_pendingWrap = false;
        m_cx = std::max(0, m_cx - 1);
        break;
    case 0x09: // HT
        m_pendingWrap = false;
        for (int x = m_cx + 1; x < m_cols; ++x) {
            if (m_tabStops[x]) {
                m_cx = x;
                break;
            }
            if (x == m_cols - 1)
                m_cx = m_cols - 1;
        }
        break;
    case 0x0A: // LF
    case 0x0B: // VT
    case 0x0C: // FF
        m_pendingWrap = false;
        index();
        break;
    case 0x0D: // CR
        m_pendingWrap = false;
        m_cx = 0;
        break;
    default: // SO / SI (charset switching through G1) are not used by modern programs
        break;
    }
}

void Screen::index()
{
    if (m_cy == m_bottom)
        scrollUp(1);
    else if (m_cy < m_rows - 1)
        ++m_cy;
}

void Screen::reverseIndex()
{
    if (m_cy == m_top)
        scrollDown(1);
    else if (m_cy > 0)
        --m_cy;
}

void Screen::newlineWrapped()
{
    grid()[m_cy].wrapped = true;
    m_cx = 0;
    index();
}

void Screen::pushHistory(const Line &line)
{
    if (m_alt || m_historyLimit == 0)
        return;
    m_history.push_back(line);
    while (static_cast<int>(m_history.size()) > m_historyLimit)
        m_history.pop_front();
    if (m_viewOffset > 0) // keep the same text in view while the program prints
        m_viewOffset = std::min(m_viewOffset + 1, historyLines());
}

void Screen::scrollUp(int n)
{
    auto &g = grid();
    const bool fullScreen = m_top == 0 && m_bottom == m_rows - 1;
    n = std::min(n, m_bottom - m_top + 1); // scrolling further only produces blank lines
    for (int k = 0; k < n; ++k) {
        if (fullScreen)
            pushHistory(g[m_top]);
        std::rotate(g.begin() + m_top, g.begin() + m_top + 1, g.begin() + m_bottom + 1);
        g[m_bottom] = blankLine();
    }
}

void Screen::scrollDown(int n)
{
    auto &g = grid();
    n = std::min(n, m_bottom - m_top + 1);
    for (int k = 0; k < n; ++k) {
        std::rotate(g.begin() + m_top, g.begin() + m_bottom, g.begin() + m_bottom + 1);
        g[m_top] = blankLine();
    }
}

void Screen::setCursor(int x, int y)
{
    m_pendingWrap = false;
    m_cx = std::clamp(x, 0, m_cols - 1);
    if (m_originMode)
        m_cy = std::clamp(y, m_top, m_bottom);
    else
        m_cy = std::clamp(y, 0, m_rows - 1);
}

void Screen::moveCursor(int dx, int dy)
{
    m_pendingWrap = false;
    m_cx = std::clamp(m_cx + dx, 0, m_cols - 1);
    // Movement stops at the scroll region when the cursor starts inside it.
    const bool inside = m_cy >= m_top && m_cy <= m_bottom;
    const int lo = inside ? m_top : 0;
    const int hi = inside ? m_bottom : m_rows - 1;
    m_cy = std::clamp(m_cy + dy, lo, hi);
}

void Screen::eraseDisplay(int mode)
{
    auto &g = grid();
    m_pendingWrap = false;
    switch (mode) {
    case 0:
        eraseLine(0);
        for (int y = m_cy + 1; y < m_rows; ++y)
            g[y] = blankLine();
        break;
    case 1:
        for (int y = 0; y < m_cy; ++y)
            g[y] = blankLine();
        eraseLine(1);
        break;
    case 3:
        clearHistory();
        [[fallthrough]];
    default:
        for (Line &l : g)
            l = blankLine();
        break;
    }
}

void Screen::eraseLine(int mode)
{
    m_pendingWrap = false;
    auto &cells = grid()[m_cy].cells;
    int from = 0;
    int to = m_cols;
    if (mode == 0) {
        from = m_cx;
    } else if (mode == 1) {
        to = m_cx + 1;
    }
    for (int x = from; x < to; ++x)
        cells[x] = blankCell();
}

void Screen::insertLines(int n)
{
    if (m_cy < m_top || m_cy > m_bottom)
        return;
    m_cx = 0;
    m_pendingWrap = false;
    auto &g = grid();
    n = std::min(n, m_bottom - m_cy + 1);
    for (int k = 0; k < n; ++k) {
        std::rotate(g.begin() + m_cy, g.begin() + m_bottom, g.begin() + m_bottom + 1);
        g[m_cy] = blankLine();
    }
}

void Screen::deleteLines(int n)
{
    if (m_cy < m_top || m_cy > m_bottom)
        return;
    m_cx = 0;
    m_pendingWrap = false;
    auto &g = grid();
    n = std::min(n, m_bottom - m_cy + 1);
    for (int k = 0; k < n; ++k) {
        std::rotate(g.begin() + m_cy, g.begin() + m_cy + 1, g.begin() + m_bottom + 1);
        g[m_bottom] = blankLine();
    }
}

void Screen::insertChars(int n)
{
    m_pendingWrap = false;
    auto &cells = grid()[m_cy].cells;
    n = std::min(n, m_cols - m_cx);
    if (n <= 0)
        return;
    std::rotate(cells.begin() + m_cx, cells.end() - n, cells.end());
    for (int x = m_cx; x < m_cx + n; ++x)
        cells[x] = blankCell();
}

void Screen::deleteChars(int n)
{
    m_pendingWrap = false;
    auto &cells = grid()[m_cy].cells;
    n = std::min(n, m_cols - m_cx);
    if (n <= 0)
        return;
    std::rotate(cells.begin() + m_cx, cells.begin() + m_cx + n, cells.end());
    for (int x = m_cols - n; x < m_cols; ++x)
        cells[x] = blankCell();
}

void Screen::eraseChars(int n)
{
    m_pendingWrap = false;
    auto &cells = grid()[m_cy].cells;
    for (int x = m_cx; x < std::min(m_cols, m_cx + n); ++x)
        cells[x] = blankCell();
}

void Screen::saveCursor()
{
    m_savedCursor = Saved{m_cx, m_cy, m_cur, m_originMode, m_autowrap};
}

void Screen::restoreCursor()
{
    m_pendingWrap = false;
    m_cx = std::clamp(m_savedCursor.x, 0, m_cols - 1);
    m_cy = std::clamp(m_savedCursor.y, 0, m_rows - 1);
    m_cur = m_savedCursor.attr;
    m_originMode = m_savedCursor.originMode;
    m_autowrap = m_savedCursor.autowrap;
}

void Screen::enterAlt(bool saveCursorToo)
{
    if (m_alt)
        return;
    if (saveCursorToo)
        m_savedAltCursor = Saved{m_cx, m_cy, m_cur, m_originMode, m_autowrap};
    m_alt = true;
    m_viewOffset = 0;
    for (Line &l : m_altGrid)
        l = blankLine();
}

void Screen::leaveAlt(bool restoreCursorToo)
{
    if (!m_alt)
        return;
    m_alt = false;
    m_viewOffset = 0;
    if (restoreCursorToo) {
        m_cx = std::clamp(m_savedAltCursor.x, 0, m_cols - 1);
        m_cy = std::clamp(m_savedAltCursor.y, 0, m_rows - 1);
        m_cur = m_savedAltCursor.attr;
        m_originMode = m_savedAltCursor.originMode;
        m_autowrap = m_savedAltCursor.autowrap;
    }
    m_pendingWrap = false;
}

void Screen::reply(const QByteArray &bytes) const
{
    if (m_reply)
        m_reply(bytes);
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void Screen::csi(const std::vector<int> &p, const QByteArray &intermediates, char prefix, char final)
{
    if (!intermediates.isEmpty())
        return; // e.g. CSI SP q (cursor shape): accepted, not drawn differently

    // A parameter that is missing or 0 means the default.
    auto arg = [&](std::size_t i, int def) { return (i < p.size() && p[i] > 0) ? p[i] : def; };
    // Missing (-1) and explicit 0 are treated differently by a few commands, see the cases below.
    auto raw = [&](std::size_t i, int def) { return (i < p.size() && p[i] >= 0) ? p[i] : def; };

    if (prefix == '?') {
        if (final == 'h' || final == 'l')
            privateModes(p, final == 'h');
        return;
    }
    if (prefix != 0)
        return; // '>' and '=' sequences (secondary DA, modifyOtherKeys...) are not supported

    switch (final) {
    case 'A': moveCursor(0, -arg(0, 1)); break;                          // CUU
    case 'B': moveCursor(0, arg(0, 1)); break;                           // CUD
    case 'C': moveCursor(arg(0, 1), 0); break;                           // CUF
    case 'D': moveCursor(-arg(0, 1), 0); break;                          // CUB
    case 'E': moveCursor(-m_cx, arg(0, 1)); break;                       // CNL
    case 'F': moveCursor(-m_cx, -arg(0, 1)); break;                      // CPL
    case 'G': case '`': setCursor(arg(0, 1) - 1, m_cy); break;           // CHA / HPA
    case 'd': setCursor(m_cx, arg(0, 1) - 1); break;                     // VPA
    case 'H': case 'f': setCursor(arg(1, 1) - 1, arg(0, 1) - 1); break;  // CUP / HVP
    case 'J': eraseDisplay(raw(0, 0)); break;                            // ED
    case 'K': eraseLine(raw(0, 0)); break;                               // EL
    case 'L': insertLines(arg(0, 1)); break;                             // IL
    case 'M': deleteLines(arg(0, 1)); break;                             // DL
    case '@': insertChars(arg(0, 1)); break;                             // ICH
    case 'P': deleteChars(arg(0, 1)); break;                             // DCH
    case 'X': eraseChars(arg(0, 1)); break;                              // ECH
    case 'b':                                                            // REP: repeat the last character
        if (m_lastPrinted != 0) {
            const int n = std::min(arg(0, 1), m_cols * m_rows);
            const char32_t ch = m_lastPrinted;
            for (int i = 0; i < n; ++i)
                print(ch);
        }
        break;
    case 'S': scrollUp(arg(0, 1)); break;                                // SU
    case 'T': scrollDown(arg(0, 1)); break;                              // SD
    case 'r': {                                                          // DECSTBM
        const int top = arg(0, 1) - 1;
        const int bottom = arg(1, m_rows) - 1;
        if (top < bottom && bottom < m_rows) {
            m_top = top;
            m_bottom = bottom;
        } else {
            m_top = 0;
            m_bottom = m_rows - 1;
        }
        setCursor(0, 0);
        break;
    }
    case 'm': sgr(p); break;
    case 'n':                                                            // DSR
        if (raw(0, 0) == 6) {
            reply("\x1b[" + QByteArray::number(m_cy + 1) + ';' + QByteArray::number(m_cx + 1) + 'R');
        } else if (raw(0, 0) == 5) {
            reply("\x1b[0n");
        }
        break;
    case 'c':                                                            // DA
        if (raw(0, 0) == 0)
            reply("\x1b[?62;22c");
        break;
    case 's': saveCursor(); break;
    case 'u': restoreCursor(); break;
    case 'g':                                                            // TBC
        if (raw(0, 0) == 0) {
            if (m_cx < m_cols)
                m_tabStops[m_cx] = false;
        } else if (raw(0, 0) == 3) {
            std::fill(m_tabStops.begin(), m_tabStops.end(), false);
        }
        break;
    case 'h': case 'l':                                                  // SM / RM (ANSI modes)
        if (raw(0, 0) == 4)
            m_insertMode = final == 'h';
        break;
    default:
        break;
    }
}

void Screen::privateModes(const std::vector<int> &p, bool set)
{
    for (int mode : p) {
        switch (mode) {
        case 1: m_appCursor = set; break;                              // DECCKM
        case 6: m_originMode = set; setCursor(0, 0); break;            // DECOM
        case 7: m_autowrap = set; break;                               // DECAWM
        case 25: m_cursorVisible = set; break;                         // DECTCEM
        case 47: case 1047:
            set ? enterAlt(false) : leaveAlt(false);
            break;
        case 1049:
            set ? enterAlt(true) : leaveAlt(true);
            break;
        case 2004: m_bracketedPaste = set; break;
        default:
            break; // mouse reporting and other modes are accepted and ignored
        }
    }
}

void Screen::sgr(const std::vector<int> &p)
{
    if (p.empty()) {
        m_cur = Attr{};
        return;
    }
    auto setFlag = [&](std::uint16_t flag, bool on) {
        m_cur.flags = static_cast<std::uint16_t>(on ? (m_cur.flags | flag) : (m_cur.flags & ~flag));
    };

    for (std::size_t i = 0; i < p.size(); ++i) {
        const int v = p[i] < 0 ? 0 : p[i];
        if (v == 0) m_cur = Attr{};
        else if (v == 1) setFlag(Bold, true);
        else if (v == 2) setFlag(Dim, true);
        else if (v == 3) setFlag(Italic, true);
        else if (v == 4) setFlag(Underline, true);
        else if (v == 7) setFlag(Inverse, true);
        else if (v == 9) setFlag(Strike, true);
        else if (v == 22) setFlag(Bold | Dim, false);
        else if (v == 23) setFlag(Italic, false);
        else if (v == 24) setFlag(Underline, false);
        else if (v == 27) setFlag(Inverse, false);
        else if (v == 29) setFlag(Strike, false);
        else if (v >= 30 && v <= 37) m_cur.fg = Color::indexed(static_cast<std::uint8_t>(v - 30));
        else if (v == 39) m_cur.fg = Color{};
        else if (v >= 40 && v <= 47) m_cur.bg = Color::indexed(static_cast<std::uint8_t>(v - 40));
        else if (v == 49) m_cur.bg = Color{};
        else if (v >= 90 && v <= 97) m_cur.fg = Color::indexed(static_cast<std::uint8_t>(v - 90 + 8));
        else if (v >= 100 && v <= 107) m_cur.bg = Color::indexed(static_cast<std::uint8_t>(v - 100 + 8));
        else if (v == 38 || v == 48) {
            Color c;
            bool ok = false;
            if (i + 1 < p.size() && p[i + 1] == 5 && i + 2 < p.size()) {
                c = Color::indexed(static_cast<std::uint8_t>(std::clamp(p[i + 2], 0, 255)));
                i += 2;
                ok = true;
            } else if (i + 1 < p.size() && p[i + 1] == 2) {
                // 38;2;r;g;b  or  38;2::r:g:b (the colon form arrives with an empty colour-space slot)
                std::size_t k = i + 2;
                if (k < p.size() && p[k] == -1)
                    ++k;
                if (k + 2 < p.size()) {
                    c = Color::rgb(static_cast<std::uint8_t>(std::clamp(p[k], 0, 255)),
                                   static_cast<std::uint8_t>(std::clamp(p[k + 1], 0, 255)),
                                   static_cast<std::uint8_t>(std::clamp(p[k + 2], 0, 255)));
                    i = k + 2;
                    ok = true;
                }
            }
            if (ok) {
                if (v == 38) m_cur.fg = c;
                else m_cur.bg = c;
            } else {
                break; // malformed: ignore the rest of the sequence
            }
        }
        // Underline colour (58/59) and other attributes are accepted and ignored.
    }
}

void Screen::escape(const QByteArray &intermediates, char final)
{
    if (!intermediates.isEmpty()) {
        if (intermediates == "(") {
            m_graphics = final == '0';
        } else if (intermediates == "#" && final == '8') { // DECALN: fill with 'E'
            for (Line &l : grid())
                for (Cell &c : l.cells)
                    c = Cell{U'E', Attr{}};
        }
        return; // other charsets (G1..G3) are not used
    }

    switch (final) {
    case '7': saveCursor(); break;
    case '8': restoreCursor(); break;
    case 'D': index(); break;
    case 'E': m_cx = 0; index(); break;
    case 'M': reverseIndex(); break;
    case 'H':
        if (m_cx < m_cols)
            m_tabStops[m_cx] = true;
        break;
    case 'c': reset(); break;
    default: break; // keypad modes and unknown sequences
    }
}

void Screen::oscDispatch(const QByteArray &payload)
{
    const int semi = payload.indexOf(';');
    const QByteArray code = semi < 0 ? payload : payload.left(semi);
    if (code == "0" || code == "2") {
        m_title_text = QString::fromUtf8(semi < 0 ? QByteArray() : payload.mid(semi + 1));
        if (m_title)
            m_title(m_title_text);
    }
}

} // namespace gt
