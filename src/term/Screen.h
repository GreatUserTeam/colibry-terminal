#pragma once

#include "Cell.h"

#include <QByteArray>
#include <QString>

#include <deque>
#include <functional>
#include <vector>

namespace gt {

/// One occurrence of a search string. `line` is an absolute line index (see Screen::lineAt).
struct Match {
    int line = 0;
    int col = 0;
    int length = 0; ///< in columns
};

/// The terminal state: the visible grid, the scrollback history, the cursor and the modes.
///
/// The parser (Parser) calls the dispatch functions; the widget reads the state through the
/// accessors. Nothing here knows about Qt widgets or about the pseudo-terminal.
class Screen {
public:
    using ReplyFn = std::function<void(const QByteArray &)>;
    using TitleFn = std::function<void(const QString &)>;
    using BellFn = std::function<void()>;

    Screen(int cols, int rows, int historyLimit);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    void resize(int cols, int rows);
    void setHistoryLimit(int lines);
    int historyLimit() const { return m_historyLimit; }

    // ---- dispatch (called by Parser) -----------------------------------
    void print(char32_t codepoint);
    void execute(unsigned char control);
    void csi(const std::vector<int> &params, const QByteArray &intermediates, char prefix, char final);
    void escape(const QByteArray &intermediates, char final);
    void oscDispatch(const QByteArray &payload);

    // ---- callbacks -------------------------------------------------------
    void setReplyHandler(ReplyFn fn) { m_reply = std::move(fn); }
    void setTitleHandler(TitleFn fn) { m_title = std::move(fn); }
    void setBellHandler(BellFn fn) { m_bell = std::move(fn); }

    // ---- cursor and modes ----------------------------------------------
    int cursorX() const { return m_cx; }
    int cursorY() const { return m_cy; }
    bool cursorVisible() const { return m_cursorVisible; }
    bool applicationCursorKeys() const { return m_appCursor; }
    bool bracketedPaste() const { return m_bracketedPaste; }
    bool altScreen() const { return m_alt; }
    const QString &title() const { return m_title_text; }

    // ---- lines and the viewport ----------------------------------------
    /// History lines plus the screen lines (only the screen while the alternate screen is active).
    int totalLines() const;
    int historyLines() const;
    /// Lines scrolled back from the bottom (0 = following the output).
    int viewOffset() const { return m_viewOffset; }
    void setViewOffset(int lines);
    /// Absolute index of the first line shown in the viewport.
    int viewTopLine() const;
    const Line &lineAt(int absLine) const;
    const Cell &cellAt(int absLine, int col) const;
    /// Text of one line with trailing blanks removed.
    QString lineText(int absLine) const;

    // ---- search and selection text -------------------------------------
    std::vector<Match> findAll(const QString &needle, Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive) const;
    /// Text between two absolute positions (both ends inclusive); line breaks are kept where the
    /// program really printed one.
    QString textBetween(int startLine, int startCol, int endLine, int endCol) const;

    /// Clears the whole screen and the history, the way the "Clear" command does.
    void clearHistory();
    /// Full reset (RIS, ESC c).
    void reset();

private:
    std::vector<Line> &grid() { return m_alt ? m_altGrid : m_grid; }
    const std::vector<Line> &grid() const { return m_alt ? m_altGrid : m_grid; }
    Cell blankCell() const;
    Line blankLine() const;
    void setCursor(int x, int y);
    void moveCursor(int dx, int dy);
    void index();
    void reverseIndex();
    void newlineWrapped();
    void scrollUp(int n);
    void scrollDown(int n);
    void pushHistory(const Line &line);
    void eraseDisplay(int mode);
    void eraseLine(int mode);
    void insertLines(int n);
    void deleteLines(int n);
    void insertChars(int n);
    void deleteChars(int n);
    void eraseChars(int n);
    void sgr(const std::vector<int> &params);
    void privateModes(const std::vector<int> &params, bool set);
    void enterAlt(bool saveCursor);
    void leaveAlt(bool restoreCursor);
    void saveCursor();
    void restoreCursor();
    void reply(const QByteArray &bytes) const;

    int m_cols;
    int m_rows;
    int m_historyLimit;

    std::vector<Line> m_grid;
    std::vector<Line> m_altGrid;
    std::deque<Line> m_history;
    bool m_alt = false;

    int m_cx = 0;
    int m_cy = 0;
    bool m_pendingWrap = false;
    Attr m_cur;
    int m_top = 0;
    int m_bottom = 0;
    std::vector<bool> m_tabStops;

    struct Saved {
        int x = 0;
        int y = 0;
        Attr attr;
        bool originMode = false;
        bool autowrap = true;
    };
    Saved m_savedCursor;
    Saved m_savedAltCursor;

    bool m_autowrap = true;
    bool m_originMode = false;
    bool m_insertMode = false;
    bool m_cursorVisible = true;
    bool m_appCursor = false;
    bool m_bracketedPaste = false;
    char32_t m_lastPrinted = 0; ///< for REP (CSI n b)
    bool m_graphics = false; ///< DEC special graphics character set in G0

    int m_viewOffset = 0;
    QString m_title_text;

    ReplyFn m_reply;
    TitleFn m_title;
    BellFn m_bell;
};

} // namespace gt
