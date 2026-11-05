#include "SearchBar.h"

#include <QHBoxLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QToolButton>

namespace gt {

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("searchBar");
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(8, 6, 8, 6);
    l->setSpacing(6);

    m_edit = new QLineEdit(this);
    m_edit->setPlaceholderText(tr("Search in output…"));
    m_edit->setClearButtonEnabled(true);
    m_case = new QCheckBox(tr("Aa"), this);
    m_case->setToolTip(tr("Match case"));
    m_count = new QLabel(this);
    m_count->setMinimumWidth(64);
    m_count->setAlignment(Qt::AlignCenter);

    auto *prev = new QToolButton(this);
    prev->setText(QStringLiteral("↑"));
    prev->setToolTip(tr("Previous (Shift+Enter)"));
    auto *next = new QToolButton(this);
    next->setText(QStringLiteral("↓"));
    next->setToolTip(tr("Next (Enter)"));
    auto *close = new QToolButton(this);
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Close (Esc)"));

    l->addWidget(m_edit, 1);
    l->addWidget(m_case);
    l->addWidget(m_count);
    l->addWidget(prev);
    l->addWidget(next);
    l->addWidget(close);

    connect(m_edit, &QLineEdit::textChanged, this, [this] { emit queryChanged(m_edit->text(), m_case->isChecked()); });
    connect(m_case, &QCheckBox::toggled, this, [this] { emit queryChanged(m_edit->text(), m_case->isChecked()); });
    connect(m_edit, &QLineEdit::returnPressed, this, [this] {
        if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
            emit previous();
        else
            emit this->next();
    });
    connect(prev, &QToolButton::clicked, this, &SearchBar::previous);
    connect(next, &QToolButton::clicked, this, &SearchBar::next);
    connect(close, &QToolButton::clicked, this, &SearchBar::closed);
    setFocusProxy(m_edit);
}

void SearchBar::activate()
{
    show();
    m_edit->setFocus();
    m_edit->selectAll();
}

void SearchBar::setResult(int current, int total)
{
    if (m_edit->text().isEmpty())
        m_count->clear();
    else if (total == 0)
        m_count->setText(tr("No results"));
    else
        m_count->setText(QStringLiteral("%1 / %2").arg(current + 1).arg(total));
}

void SearchBar::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape) {
        emit closed();
        return;
    }
    QWidget::keyPressEvent(e);
}

} // namespace gt
