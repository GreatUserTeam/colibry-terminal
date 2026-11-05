#include "TerminalWidget.h"

#include "term/Input.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace gt {

namespace {
constexpr int kScrollBarWidth = 10;

QColor toColor(const Rgb &c, int alpha = 255)
{
    return QColor(c.r, c.g, c.b, alpha);
}

QColor blend(const Rgb &a, const Rgb &b, double t)
{
    return QColor(static_cast<int>(a.r + (b.r - a.r) * t), static_cast<int>(a.g + (b.g - a.g) * t),
                  static_cast<int>(a.b + (b.b - a.b) * t));
}

bool isWordChar(char32_t c)
{
    return c > 0x7F || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-'
           || c == '.' || c == '/' || c == '~';
}
} // namespace

TerminalWidget::TerminalWidget(const Config &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setCursor(Qt::IBeamCursor);

    m_scheme = &schemeByName(m_config.colorScheme);
    m_screen = std::make_unique<Screen>(80, 24, m_config.scrollbackLines);
    m_parser = std::make_unique<Parser>(*m_screen);
    m_pty = std::make_unique<Pty>();

    m_screen->setReplyHandler([this](const QByteArray &b) { m_pty->write(b); });
    m_screen->setTitleHandler([this](const QString &t) { emit titleChanged(t); });
    m_screen->setBellHandler([this] { emit bell(); });

    connect(m_pty.get(), &Pty::dataReceived, this, &TerminalWidget::feed);
    connect(m_pty.get(), &Pty::finished, this, &TerminalWidget::processFinished);

    const auto surface = gus::gpu::createRenderSurface(this, [this](QPainter &p, const QRect &) { paintContents(p); });
    m_surface = surface.widget;
    m_backend = surface.backend;
    m_surface->setGeometry(rect());

    m_scrollBar = new QScrollBar(Qt::Vertical, this);
    m_scrollBar->setFixedWidth(kScrollBarWidth);
    connect(m_scrollBar, &QScrollBar::valueChanged, this, [this](int v) {
        if (m_syncingScrollBar)
            return;
        m_screen->setViewOffset(m_screen->historyLines() - v);
        repaintSurface();
    });

    m_blink.setInterval(530);
    connect(&m_blink, &QTimer::timeout, this, [this] {
        m_cursorOn = !m_cursorOn;
        repaintSurface();
    });

    updateFontMetrics();
    applyConfig(config);
}

TerminalWidget::~TerminalWidget() = default;

bool TerminalWidget::start(const QString &workingDirectory, const QString &program, const QStringList &args)
{
    const QString prog = program.isEmpty() ? m_config.shell : program;
    return m_pty->start(prog, args, workingDirectory, m_screen->cols(), m_screen->rows());
}

void TerminalWidget::applyConfig(const Config &config)
{
    m_config = config;
    m_scheme = &schemeByName(m_config.colorScheme);
    m_screen->setHistoryLimit(m_config.scrollbackLines);
    updateFontMetrics();
    recomputeGrid();
    if (m_config.cursorBlink && hasFocus()) {
        m_blink.start();
    } else {
        m_blink.stop();
        m_cursorOn = true;
    }
    syncScrollBar();
    repaintSurface();
}

void TerminalWidget::updateFontMetrics()
{
    m_font = QFont(m_config.fontFamily);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    m_font.setPointSize(m_config.fontSize);
    m_font.setKerning(false);
    // Without a real family the style hint picks any monospace font.
    if (!QFontDatabase::families().contains(m_config.fontFamily))
        m_font.setFamilies({m_config.fontFamily, QStringLiteral("DejaVu Sans Mono"), QStringLiteral("monospace")});
    m_boldFont = m_font;
    m_boldFont.setBold(true);

    const QFontMetricsF fm(m_font);
    m_cell = QSizeF(std::max(1.0, std::round(fm.horizontalAdvance(QLatin1Char('M')))), std::max(1.0, std::ceil(fm.height())));
    m_ascent = fm.ascent();
}

void TerminalWidget::recomputeGrid()
{
    const int usableW = std::max(1, width() - kScrollBarWidth);
    const int cols = std::max(2, static_cast<int>(usableW / m_cell.width()));
    const int rows = std::max(1, static_cast<int>(height() / m_cell.height()));
    if (cols != m_screen->cols() || rows != m_screen->rows()) {
        m_screen->resize(cols, rows);
        m_pty->resize(cols, rows);
        clearSelection();
        if (!m_needle.isEmpty())
            rebuildMatches();
    }
    m_scrollBar->setGeometry(width() - kScrollBarWidth, 0, kScrollBarWidth, height());
    m_scrollBar->raise();
    syncScrollBar();
}

void TerminalWidget::repaintSurface()
{
    m_surface->update();
}

void TerminalWidget::resizeEvent(QResizeEvent *)
{
    m_surface->setGeometry(rect());
    recomputeGrid();
}

void TerminalWidget::syncScrollBar()
{
    m_syncingScrollBar = true;
    const int hist = m_screen->historyLines();
    m_scrollBar->setRange(0, hist);
    m_scrollBar->setPageStep(m_screen->rows());
    m_scrollBar->setValue(hist - m_screen->viewOffset());
    m_scrollBar->setVisible(hist > 0);
    m_syncingScrollBar = false;
}

void TerminalWidget::feed(const QByteArray &bytes)
{
    m_parser->feed(bytes);
    if (!m_needle.isEmpty())
        rebuildMatches();
    syncScrollBar();
    repaintSurface();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void TerminalWidget::paintContents(QPainter &p)
{
    // The background carries the window opacity; text stays opaque.
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(QPoint(0, 0), size()), toColor(m_scheme->background, static_cast<int>(m_config.opacity * 255)));
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const int top = m_screen->viewTopLine();
    for (int row = 0; row < m_screen->rows(); ++row)
        drawRow(p, row, top + row);

    // Cursor
    if (m_screen->cursorVisible() && m_screen->viewOffset() == 0) {
        const int cx = m_screen->cursorX();
        const int cy = m_screen->cursorY();
        const QRectF cell(cx * m_cell.width(), cy * m_cell.height(), m_cell.width(), m_cell.height());
        const QColor cc = toColor(m_scheme->cursor);
        if (!hasFocus()) {
            p.setPen(cc);
            p.setBrush(Qt::NoBrush);
            p.drawRect(cell.adjusted(0.5, 0.5, -0.5, -0.5));
        } else if (m_cursorOn || !m_config.cursorBlink) {
            if (m_config.cursorShape == 1) {
                p.fillRect(QRectF(cell.left(), cell.top(), 2, cell.height()), cc);
            } else if (m_config.cursorShape == 2) {
                p.fillRect(QRectF(cell.left(), cell.bottom() - 2, cell.width(), 2), cc);
            } else {
                p.fillRect(cell, cc);
                const Cell &c = m_screen->cellAt(top + cy, cx);
                if (c.ch > U' ' && c.ch != 0) {
                    p.setPen(toColor(m_scheme->background));
                    p.setFont(c.attr.has(Bold) ? m_boldFont : m_font);
                    p.drawText(QPointF(cell.left(), cell.top() + m_ascent), QString::fromUcs4(&c.ch, 1));
                }
            }
        }
    }
}

void TerminalWidget::drawRow(QPainter &p, int viewRow, int absLine)
{
    const Line &line = m_screen->lineAt(absLine);
    const int cols = m_screen->cols();
    const qreal y = viewRow * m_cell.height();
    const qreal cw = m_cell.width();

    // Search highlights and selection first, so text goes on top.
    auto fillCols = [&](int c0, int c1, const QColor &color) {
        p.fillRect(QRectF(c0 * cw, y, (c1 - c0) * cw, m_cell.height()), color);
    };

    // Resolve colours per cell and group equal neighbours into runs.
    struct Run {
        int start;
        int end;
        Attr attr;
    };
    std::vector<Run> runs;
    for (int c = 0; c < cols && c < static_cast<int>(line.cells.size()); ++c) {
        const Cell &cell = line.cells[c];
        if (cell.attr.has(WideTail))
            continue;
        Attr key = cell.attr;
        const bool wide = key.has(WideLead);
        const int span = wide ? 2 : 1;
        if (!runs.empty() && !wide && !(runs.back().attr.flags & WideLead) && runs.back().end == c
            && runs.back().attr == key) {
            runs.back().end = c + span;
        } else {
            runs.push_back({c, c + span, key});
        }
    }

    for (const Run &r : runs) {
        Rgb fg = resolveColor(r.attr.fg, *m_scheme, true);
        Rgb bg = resolveColor(r.attr.bg, *m_scheme, false);
        if (r.attr.has(Inverse))
            std::swap(fg, bg);
        const bool defaultBg = r.attr.bg.kind == Color::Kind::Default && !r.attr.has(Inverse);
        if (!defaultBg)
            fillCols(r.start, r.end, toColor(bg));
    }

    for (int c = 0; c < cols; ++c) {
        if (isSelected(absLine, c))
            fillCols(c, c + 1, toColor(m_scheme->selection));
    }
    for (std::size_t i = 0; i < m_matches.size(); ++i) {
        const Match &m = m_matches[i];
        if (m.line != absLine)
            continue;
        const bool current = static_cast<int>(i) == m_current;
        fillCols(m.col, std::min(cols, m.col + m.length), current ? QColor(255, 170, 0, 150) : QColor(255, 210, 60, 80));
    }

    for (const Run &r : runs) {
        Rgb fgc = resolveColor(r.attr.fg, *m_scheme, true);
        Rgb bgc = resolveColor(r.attr.bg, *m_scheme, false);
        if (r.attr.has(Inverse))
            std::swap(fgc, bgc);
        QColor fg = toColor(fgc);
        if (r.attr.has(Dim))
            fg = blend(fgc, bgc, 0.5);

        QString text;
        bool blank = true;
        for (int c = r.start; c < r.end; ++c) {
            const Cell &cell = line.cells[c];
            if (cell.attr.has(WideTail))
                continue;
            const char32_t ch = cell.ch == 0 ? U' ' : cell.ch;
            if (ch != U' ')
                blank = false;
            text += QString::fromUcs4(&ch, 1);
        }

        if (!blank) {
            p.setPen(fg);
            p.setFont(r.attr.has(Bold) ? m_boldFont : m_font);
            QFont f = p.font();
            f.setItalic(r.attr.has(Italic));
            p.setFont(f);
            if (m_config.ligatures && !r.attr.has(WideLead)) {
                // One call per run: the shaper can substitute ligatures such as "->" or "!=".
                p.drawText(QPointF(r.start * cw, y + m_ascent), text);
            } else {
                int col = r.start;
                for (qsizetype i = 0; i < text.size(); ++i) {
                    const QChar ch = text.at(i);
                    QString s(ch);
                    if (ch.isHighSurrogate() && i + 1 < text.size())
                        s += text.at(++i);
                    p.drawText(QPointF(col * cw, y + m_ascent), s);
                    col += (r.attr.has(WideLead) ? 2 : 1);
                }
            }
        }
        if (r.attr.has(Underline))
            p.fillRect(QRectF(r.start * cw, y + m_cell.height() - 2, (r.end - r.start) * cw, 1), fg);
        if (r.attr.has(Strike))
            p.fillRect(QRectF(r.start * cw, y + m_cell.height() / 2, (r.end - r.start) * cw, 1), fg);
    }
}

// ---------------------------------------------------------------------------
// Keyboard and input method
// ---------------------------------------------------------------------------

void TerminalWidget::keyPressEvent(QKeyEvent *e)
{
    const auto mods = e->modifiers();
    if ((mods & Qt::ShiftModifier) && !(mods & Qt::ControlModifier) && !m_screen->altScreen()) {
        if (e->key() == Qt::Key_PageUp) {
            m_screen->setViewOffset(m_screen->viewOffset() + m_screen->rows() - 1);
            syncScrollBar();
            repaintSurface();
            return;
        }
        if (e->key() == Qt::Key_PageDown) {
            m_screen->setViewOffset(m_screen->viewOffset() - (m_screen->rows() - 1));
            syncScrollBar();
            repaintSurface();
            return;
        }
    }
    const QByteArray bytes = encodeKey(*e, m_screen->applicationCursorKeys());
    if (bytes.isEmpty()) {
        QWidget::keyPressEvent(e);
        return;
    }
    scrollToBottom();
    m_pty->write(bytes);
    m_cursorOn = true;
    e->accept();
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent *e)
{
    if (!e->commitString().isEmpty()) {
        scrollToBottom();
        m_pty->write(e->commitString().toUtf8());
    }
    e->accept();
}

QVariant TerminalWidget::inputMethodQuery(Qt::InputMethodQuery q) const
{
    if (q == Qt::ImCursorRectangle)
        return QRectF(m_screen->cursorX() * m_cell.width(), m_screen->cursorY() * m_cell.height(), m_cell.width(), m_cell.height());
    return QWidget::inputMethodQuery(q);
}

void TerminalWidget::focusInEvent(QFocusEvent *e)
{
    QWidget::focusInEvent(e);
    m_cursorOn = true;
    if (m_config.cursorBlink)
        m_blink.start();
    emit focused();
    repaintSurface();
}

void TerminalWidget::focusOutEvent(QFocusEvent *e)
{
    QWidget::focusOutEvent(e);
    m_blink.stop();
    m_cursorOn = true;
    repaintSurface();
}

void TerminalWidget::scrollToBottom()
{
    if (m_screen->viewOffset() != 0) {
        m_screen->setViewOffset(0);
        syncScrollBar();
        repaintSurface();
    }
}

void TerminalWidget::clearScrollback()
{
    m_screen->clearHistory();
    clearSelection();
    rebuildMatches();
    syncScrollBar();
    repaintSurface();
}

// ---------------------------------------------------------------------------
// Mouse, selection, clipboard
// ---------------------------------------------------------------------------

TerminalWidget::Pos TerminalWidget::posAt(const QPoint &pt) const
{
    const int col = std::clamp(static_cast<int>(pt.x() / m_cell.width()), 0, m_screen->cols() - 1);
    const int row = std::clamp(static_cast<int>(pt.y() / m_cell.height()), 0, m_screen->rows() - 1);
    return {m_screen->viewTopLine() + row, col};
}

bool TerminalWidget::isSelected(int line, int col) const
{
    if (!m_hasSelection)
        return false;
    Pos a = m_selStart;
    Pos b = m_selEnd;
    if (b < a)
        std::swap(a, b);
    const Pos p{line, col};
    return !(p < a) && !(b < p);
}

void TerminalWidget::mousePressEvent(QMouseEvent *e)
{
    setFocus();
    if (e->button() == Qt::LeftButton) {
        m_selStart = m_selEnd = posAt(e->pos());
        m_hasSelection = false;
        m_dragging = true;
        repaintSurface();
    } else if (e->button() == Qt::MiddleButton) {
        const QString text = QApplication::clipboard()->text(QClipboard::Selection);
        if (!text.isEmpty())
            m_pty->write(encodePaste(text, m_screen->bracketedPaste()));
    }
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragging)
        return;
    m_selEnd = posAt(e->pos());
    m_hasSelection = !(m_selStart == m_selEnd);
    repaintSurface();
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_dragging = false;
    if (m_hasSelection && QApplication::clipboard()->supportsSelection())
        QApplication::clipboard()->setText(selectedText(), QClipboard::Selection);
}

void TerminalWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    const Pos p = posAt(e->pos());
    int a = p.col;
    int b = p.col;
    const int cols = m_screen->cols();
    const bool word = isWordChar(m_screen->cellAt(p.line, p.col).ch);
    while (a > 0 && isWordChar(m_screen->cellAt(p.line, a - 1).ch) == word)
        --a;
    while (b < cols - 1 && isWordChar(m_screen->cellAt(p.line, b + 1).ch) == word)
        ++b;
    m_selStart = {p.line, a};
    m_selEnd = {p.line, b};
    m_hasSelection = true;
    repaintSurface();
}

void TerminalWidget::wheelEvent(QWheelEvent *e)
{
    const int steps = e->angleDelta().y() / 40; // 3 lines per notch
    if (steps == 0)
        return;
    if (m_screen->altScreen()) {
        // Full-screen programs (less, vim) have no scrollback here: send arrow keys instead.
        const QByteArray key = m_screen->applicationCursorKeys() ? (steps > 0 ? "\x1bOA" : "\x1bOB")
                                                                  : (steps > 0 ? "\x1b[A" : "\x1b[B");
        for (int i = 0; i < std::abs(steps); ++i)
            m_pty->write(key);
    } else {
        m_screen->setViewOffset(m_screen->viewOffset() + steps);
        syncScrollBar();
        repaintSurface();
    }
    e->accept();
}

QString TerminalWidget::selectedText() const
{
    if (!m_hasSelection)
        return {};
    Pos a = m_selStart;
    Pos b = m_selEnd;
    if (b < a)
        std::swap(a, b);
    return m_screen->textBetween(a.line, a.col, b.line, b.col);
}

void TerminalWidget::clearSelection()
{
    m_hasSelection = false;
    repaintSurface();
}

void TerminalWidget::selectAll()
{
    m_selStart = {0, 0};
    m_selEnd = {m_screen->totalLines() - 1, m_screen->cols() - 1};
    m_hasSelection = true;
    repaintSurface();
}

void TerminalWidget::copy()
{
    if (m_hasSelection)
        QApplication::clipboard()->setText(selectedText());
}

void TerminalWidget::paste()
{
    const QString text = QApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    scrollToBottom();
    m_pty->write(encodePaste(text, m_screen->bracketedPaste()));
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

void TerminalWidget::rebuildMatches()
{
    const int previousLine = (m_current >= 0 && m_current < static_cast<int>(m_matches.size())) ? m_matches[m_current].line : -1;
    m_matches = m_screen->findAll(m_needle, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    if (m_matches.empty()) {
        m_current = -1;
    } else if (previousLine < 0 || m_current >= static_cast<int>(m_matches.size())) {
        m_current = std::min(m_current, static_cast<int>(m_matches.size()) - 1);
    }
}

int TerminalWidget::setSearch(const QString &needle, bool caseSensitive)
{
    m_needle = needle;
    m_caseSensitive = caseSensitive;
    m_current = -1;
    rebuildMatches();
    if (!m_matches.empty())
        goToMatch(static_cast<int>(m_matches.size()) - 1); // start from the newest output
    repaintSurface();
    return static_cast<int>(m_matches.size());
}

void TerminalWidget::clearSearch()
{
    m_needle.clear();
    m_matches.clear();
    m_current = -1;
    repaintSurface();
}

void TerminalWidget::goToMatch(int index)
{
    if (m_matches.empty())
        return;
    const int n = static_cast<int>(m_matches.size());
    m_current = ((index % n) + n) % n;
    const Match &m = m_matches[m_current];
    const int top = m_screen->viewTopLine();
    if (m.line < top || m.line >= top + m_screen->rows()) {
        // Put the match in the middle of the view.
        const int target = std::max(0, m.line - m_screen->rows() / 2);
        const int maxTop = m_screen->totalLines() - m_screen->rows();
        m_screen->setViewOffset(maxTop - std::min(target, maxTop));
    }
    syncScrollBar();
    repaintSurface();
}

bool TerminalWidget::findNext()
{
    if (m_matches.empty())
        return false;
    goToMatch(m_current + 1);
    return true;
}

bool TerminalWidget::findPrevious()
{
    if (m_matches.empty())
        return false;
    goToMatch(m_current - 1);
    return true;
}

} // namespace gt
