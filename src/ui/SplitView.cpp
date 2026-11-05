#include "SplitView.h"

#include <QApplication>
#include <QTimer>
#include <QVBoxLayout>

#include <limits>

namespace gt {

SplitView::SplitView(const Config &config, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}

TerminalWidget *SplitView::createTerminal()
{
    auto *t = new TerminalWidget(m_config, this);
    connect(t, &TerminalWidget::focused, this, [this, t] {
        m_last = t;
        emit titleChanged(t->title());
    });
    connect(t, &TerminalWidget::titleChanged, this, [this, t](const QString &title) {
        if (t == focusedTerminal())
            emit titleChanged(title);
    });
    connect(t, &TerminalWidget::bell, this, &SplitView::bell);
    connect(t, &TerminalWidget::processFinished, this, [this, t] {
        // Remove after the signal has finished: the PTY object that emitted it lives inside t.
        QTimer::singleShot(0, this, [this, t] { removeTerminal(t); });
    });
    return t;
}

bool SplitView::startInitial(const QString &workingDirectory, const QString &program, const QStringList &args)
{
    TerminalWidget *t = createTerminal();
    layout()->addWidget(t);
    m_last = t;
    t->show();
    t->setFocus();
    return t->start(workingDirectory, program, args);
}

QList<TerminalWidget *> SplitView::terminals() const
{
    return findChildren<TerminalWidget *>(QString(), Qt::FindChildrenRecursively);
}

TerminalWidget *SplitView::focusedTerminal() const
{
    for (TerminalWidget *t : terminals()) {
        if (t->hasFocus())
            return t;
    }
    if (m_last && terminals().contains(m_last))
        return m_last;
    const auto all = terminals();
    return all.isEmpty() ? nullptr : all.first();
}

TerminalWidget *SplitView::split(Qt::Orientation orientation, const QString &workingDirectory)
{
    TerminalWidget *current = focusedTerminal();
    if (!current)
        return nullptr;
    TerminalWidget *fresh = createTerminal();

    QWidget *parent = current->parentWidget();
    auto *parentSplitter = qobject_cast<QSplitter *>(parent);
    if (parentSplitter && parentSplitter->orientation() == orientation) {
        const int idx = parentSplitter->indexOf(current);
        parentSplitter->insertWidget(idx + 1, fresh);
        QList<int> sizes;
        for (int i = 0; i < parentSplitter->count(); ++i)
            sizes << 1;
        parentSplitter->setSizes(sizes);
    } else {
        auto *sp = new QSplitter(orientation);
        sp->setChildrenCollapsible(false);
        sp->setHandleWidth(3);
        if (parentSplitter) {
            const int idx = parentSplitter->indexOf(current);
            const QList<int> keep = parentSplitter->sizes();
            parentSplitter->insertWidget(idx, sp);
            parentSplitter->setSizes(keep);
        } else {
            layout()->addWidget(sp);
        }
        sp->addWidget(current);
        sp->addWidget(fresh);
        sp->setSizes({1, 1});
        sp->show();
    }
    fresh->show();
    fresh->setFocus();
    m_last = fresh;
    fresh->start(workingDirectory);
    return fresh;
}

void SplitView::removeTerminal(TerminalWidget *t)
{
    if (!terminals().contains(t))
        return;
    QWidget *parent = t->parentWidget();
    auto *sp = qobject_cast<QSplitter *>(parent);
    if (!sp) { // the only pane
        t->hide();
        t->deleteLater();
        emit lastPaneClosed();
        return;
    }

    const int idx = sp->indexOf(t);
    const int neighbor = idx > 0 ? idx - 1 : 1;
    QWidget *next = sp->widget(neighbor);
    t->hide();
    t->setParent(nullptr);
    t->deleteLater();

    if (sp->count() == 1) {
        QWidget *only = sp->widget(0);
        QWidget *gp = sp->parentWidget();
        if (auto *gps = qobject_cast<QSplitter *>(gp)) {
            const int at = gps->indexOf(sp);
            const QList<int> keep = gps->sizes();
            gps->insertWidget(at, only);
            gps->setSizes(keep);
        } else {
            layout()->addWidget(only);
        }
        sp->hide();
        sp->setParent(nullptr);
        sp->deleteLater();
        next = only;
    }

    // Focus the closest terminal below `next`.
    TerminalWidget *target = qobject_cast<TerminalWidget *>(next);
    if (!target && next)
        target = next->findChild<TerminalWidget *>();
    if (target) {
        m_last = target;
        target->setFocus();
    }
}

bool SplitView::closeFocused()
{
    TerminalWidget *t = focusedTerminal();
    if (!t)
        return true;
    const bool last = paneCount() == 1;
    removeTerminal(t);
    return last;
}

void SplitView::focusNeighbor(int dx, int dy)
{
    TerminalWidget *cur = focusedTerminal();
    if (!cur)
        return;
    const QPoint c = cur->mapTo(this, cur->rect().center());
    TerminalWidget *best = nullptr;
    int bestDist = std::numeric_limits<int>::max();
    for (TerminalWidget *t : terminals()) {
        if (t == cur)
            continue;
        const QPoint p = t->mapTo(this, t->rect().center());
        const int ddx = p.x() - c.x();
        const int ddy = p.y() - c.y();
        const bool ok = (dx > 0 && ddx > 0) || (dx < 0 && ddx < 0) || (dy > 0 && ddy > 0) || (dy < 0 && ddy < 0);
        if (!ok)
            continue;
        // Prefer panes in the movement direction, penalise sideways offset.
        const int dist = dx != 0 ? std::abs(ddx) + 3 * std::abs(ddy) : std::abs(ddy) + 3 * std::abs(ddx);
        if (dist < bestDist) {
            bestDist = dist;
            best = t;
        }
    }
    if (best) {
        best->setFocus();
        m_last = best;
    }
}

void SplitView::applyConfig(const Config &config)
{
    m_config = config;
    for (TerminalWidget *t : terminals())
        t->applyConfig(config);
}

} // namespace gt
