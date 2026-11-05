#pragma once

#include "config/Config.h"
#include "gpu/RenderSurface.h"
#include "pty/Pty.h"
#include "term/ColorScheme.h"
#include "term/Parser.h"
#include "term/Screen.h"

#include <QFont>
#include <QScrollBar>
#include <QTimer>
#include <QWidget>

#include <memory>
#include <vector>

namespace gt {

/// One terminal: a PTY, the VT emulator and the view that draws it.
class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(const Config &config, QWidget *parent = nullptr);
    ~TerminalWidget() override;

    /// Starts the shell (or `program`) in the PTY. Returns false if the process could not be started.
    bool start(const QString &workingDirectory = {}, const QString &program = {}, const QStringList &args = {});
    void applyConfig(const Config &config);

    Screen &screen() { return *m_screen; }
    const Screen &screen() const { return *m_screen; }
    Pty &pty() { return *m_pty; }
    int columns() const { return m_screen->cols(); }
    int rowCount() const { return m_screen->rows(); }
    QSizeF cellSize() const { return m_cell; }
    QString title() const { return m_screen->title(); }

    /// Feeds bytes to the emulator as if the program had printed them (used by the PTY and by tests).
    void feed(const QByteArray &bytes);

    // clipboard
    QString selectedText() const;
    bool hasSelection() const { return m_hasSelection; }
    void clearSelection();
    void selectAll();
    void copy();
    void paste();

    // search
    int setSearch(const QString &needle, bool caseSensitive);
    void clearSearch();
    bool findNext();
    bool findPrevious();
    int matchCount() const { return static_cast<int>(m_matches.size()); }
    int currentMatch() const { return m_current; }

    void scrollToBottom();
    void clearScrollback();

    /// Which renderer draws this terminal (OpenGL on the GPU, or software).
    gus::gpu::Backend renderBackend() const { return m_backend; }
    /// Draws the whole terminal. Called by the render surface; public so tests can render offscreen.
    void paintContents(QPainter &painter);

signals:
    void titleChanged(const QString &title);
    void processFinished(int exitCode, bool crashed);
    void bell();
    void focused();

protected:
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void inputMethodEvent(QInputMethodEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void focusInEvent(QFocusEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    bool focusNextPrevChild(bool) override { return false; }
    QVariant inputMethodQuery(Qt::InputMethodQuery) const override;

private:
    struct Pos {
        int line = 0;
        int col = 0;
        bool operator<(const Pos &o) const { return line != o.line ? line < o.line : col < o.col; }
        bool operator==(const Pos &o) const = default;
    };

    void updateFontMetrics();
    void recomputeGrid();
    void syncScrollBar();
    Pos posAt(const QPoint &p) const;
    bool isSelected(int line, int col) const;
    void goToMatch(int index);
    void rebuildMatches();
    void drawRow(QPainter &p, int viewRow, int absLine);
    void repaintSurface();

    Config m_config;
    std::unique_ptr<Screen> m_screen;
    std::unique_ptr<Parser> m_parser;
    std::unique_ptr<Pty> m_pty;

    QFont m_font;
    QFont m_boldFont;
    QSizeF m_cell{8, 16};
    qreal m_ascent = 12;
    const ColorScheme *m_scheme = nullptr;

    QWidget *m_surface = nullptr;
    gus::gpu::Backend m_backend = gus::gpu::Backend::Software;
    QScrollBar *m_scrollBar = nullptr;
    bool m_syncingScrollBar = false;

    QTimer m_blink;
    bool m_cursorOn = true;

    Pos m_selStart;
    Pos m_selEnd;
    bool m_hasSelection = false;
    bool m_dragging = false;

    QString m_needle;
    bool m_caseSensitive = false;
    std::vector<Match> m_matches;
    int m_current = -1;
};

} // namespace gt
