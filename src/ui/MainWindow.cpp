#include "MainWindow.h"

#include "ui/PreferencesDialog.h"
#include "ui/Theme.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFile>
#include <QMenu>
#include <QPainter>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace gt {

MainWindow::MainWindow(const Config &config, const QString &configPath, QWidget *parent)
    : QMainWindow(parent)
    , m_config(config)
    , m_configPath(configPath)
{
    setWindowTitle(QStringLiteral("GUS Terminal"));
    setWindowIcon(QIcon(QStringLiteral(":/gus-terminal.svg")));
    setAttribute(Qt::WA_TranslucentBackground); // lets the window opacity setting show the desktop through
    resize(config.windowWidth, config.windowHeight);

    auto *central = new QWidget(this);
    central->setObjectName("central");
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabs = new QTabWidget(central);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setExpanding(false);
    m_tabs->tabBar()->setAutoHide(false);
    m_tabs->tabBar()->setDrawBase(false); // no light baseline under the tabs

    // Corner buttons: new tab and the main menu.
    auto *plus = new QToolButton(m_tabs);
    plus->setText(QStringLiteral("＋"));
    plus->setToolTip(tr("New tab (Ctrl+Shift+T)"));
    connect(plus, &QToolButton::clicked, this, [this] { newTab(currentDirectory()); });
    m_tabs->setCornerWidget(plus, Qt::TopLeftCorner);

    m_search = new SearchBar(central);
    m_search->hide();

    layout->addWidget(m_tabs, 1);
    layout->addWidget(m_search);
    setCentralWidget(central);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        if (auto *t = currentTerminal())
            t->setFocus();
        updateSearchStatus();
    });

    connect(m_search, &SearchBar::queryChanged, this, [this](const QString &text, bool cs) {
        if (auto *t = currentTerminal()) {
            t->setSearch(text, cs);
            updateSearchStatus();
        }
    });
    connect(m_search, &SearchBar::next, this, [this] {
        if (auto *t = currentTerminal()) {
            t->findNext();
            updateSearchStatus();
        }
    });
    connect(m_search, &SearchBar::previous, this, [this] {
        if (auto *t = currentTerminal()) {
            t->findPrevious();
            updateSearchStatus();
        }
    });
    connect(m_search, &SearchBar::closed, this, [this] { hideSearch(); });

    buildActions();
    setStyleSheet(buildStyleSheet(schemeByName(m_config.colorScheme)));
}

SplitView *MainWindow::currentView() const
{
    return qobject_cast<SplitView *>(m_tabs->currentWidget());
}

TerminalWidget *MainWindow::currentTerminal() const
{
    auto *v = currentView();
    return v ? v->focusedTerminal() : nullptr;
}

QString MainWindow::currentDirectory() const
{
    // New tabs and splits open where the current shell is (read from /proc on Linux).
    if (auto *t = currentTerminal()) {
        if (t->pty().isRunning()) {
            const QString dir = QFile::symLinkTarget(QStringLiteral("/proc/%1/cwd").arg(t->pty().pid()));
            if (!dir.isEmpty())
                return dir;
        }
    }
    return {};
}

SplitView *MainWindow::newTab(const QString &workingDirectory, const QString &program, const QStringList &args)
{
    auto *view = new SplitView(m_config, m_tabs);
    const int index = m_tabs->addTab(view, tr("Terminal"));
    m_tabs->setCurrentIndex(index);
    if (!view->startInitial(workingDirectory, program, args)) {
        // Keep the tab: the child printed its own error and exits, which closes the pane.
    }
    connect(view, &SplitView::titleChanged, this, [this, view](const QString &t) { onTabTitle(view, t); });
    connect(view, &SplitView::lastPaneClosed, this, [this, view] { closeTab(m_tabs->indexOf(view)); });
    connect(view, &SplitView::bell, this, [this, view] {
        const int i = m_tabs->indexOf(view);
        if (i >= 0 && i != m_tabs->currentIndex())
            m_tabs->setTabText(i, QStringLiteral("● ") + m_tabs->tabText(i).remove(QStringLiteral("● ")));
    });
    return view;
}

void MainWindow::onTabTitle(SplitView *view, const QString &title)
{
    const int i = m_tabs->indexOf(view);
    if (i < 0)
        return;
    QString t = title.isEmpty() ? tr("Terminal") : title;
    if (t.size() > 40)
        t = t.left(39) + QStringLiteral("…");
    m_tabs->setTabText(i, t);
    m_tabs->setTabToolTip(i, title);
    if (i == m_tabs->currentIndex())
        setWindowTitle(title.isEmpty() ? QStringLiteral("GUS Terminal") : title + QStringLiteral(" — GUS Terminal"));
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabs->count())
        return;
    QWidget *w = m_tabs->widget(index);
    m_tabs->removeTab(index);
    w->deleteLater();
    if (m_tabs->count() == 0)
        close();
}

void MainWindow::closeCurrentPane()
{
    auto *v = currentView();
    if (!v)
        return;
    if (v->paneCount() <= 1)
        closeTab(m_tabs->currentIndex());
    else
        v->closeFocused();
}

void MainWindow::splitRight()
{
    if (auto *v = currentView())
        v->split(Qt::Horizontal, currentDirectory());
}

void MainWindow::splitDown()
{
    if (auto *v = currentView())
        v->split(Qt::Vertical, currentDirectory());
}

void MainWindow::showSearch()
{
    m_search->activate();
    if (auto *t = currentTerminal(); t && !m_search->text().isEmpty()) {
        t->setSearch(m_search->text(), m_search->caseSensitive());
        updateSearchStatus();
    }
}

void MainWindow::hideSearch()
{
    m_search->hide();
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *v = qobject_cast<SplitView *>(m_tabs->widget(i))) {
            for (TerminalWidget *t : v->terminals())
                t->clearSearch();
        }
    }
    if (auto *t = currentTerminal())
        t->setFocus();
}

void MainWindow::updateSearchStatus()
{
    if (auto *t = currentTerminal())
        m_search->setResult(t->currentMatch(), t->matchCount());
}

void MainWindow::zoom(int delta)
{
    Config c = m_config;
    c.fontSize = delta == 0 ? 12 : m_config.fontSize + delta;
    c.sanitize();
    applyConfig(c, true);
}

void MainWindow::openPreferences()
{
    const Config before = m_config;
    PreferencesDialog dlg(m_config, this);
    connect(&dlg, &PreferencesDialog::changed, this, [this](const Config &c) { applyConfig(c, false); });
    if (dlg.exec() == QDialog::Accepted)
        applyConfig(dlg.config(), true);
    else
        applyConfig(before, false);
}

void MainWindow::applyConfig(const Config &config, bool save)
{
    m_config = config;
    setStyleSheet(buildStyleSheet(schemeByName(m_config.colorScheme)));
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto *v = qobject_cast<SplitView *>(m_tabs->widget(i)))
            v->applyConfig(m_config);
    }
    if (save)
        m_config.save(m_configPath);
}

void MainWindow::paintEvent(QPaintEvent *)
{
    // The window is translucent, so the chrome colour is painted here with the configured opacity.
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    QColor c = chromeColor(schemeByName(m_config.colorScheme));
    c.setAlphaF(m_config.opacity);
    p.fillRect(rect(), c);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    m_config.windowWidth = width();
    m_config.windowHeight = height();
    m_config.save(m_configPath);
    e->accept();
}

void MainWindow::buildActions()
{
    auto add = [this](const QString &text, const QKeySequence &key, auto slot) {
        auto *a = new QAction(text, this);
        a->setShortcut(key);
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, slot);
        addAction(a);
        return a;
    };

    auto *newTabA = add(tr("New tab"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T), [this] { newTab(currentDirectory()); });
    auto *closeA = add(tr("Close pane / tab"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_W), [this] { closeCurrentPane(); });
    auto *copyA = add(tr("Copy"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), [this] {
        if (auto *t = currentTerminal())
            t->copy();
    });
    auto *pasteA = add(tr("Paste"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), [this] {
        if (auto *t = currentTerminal())
            t->paste();
    });
    auto *findA = add(tr("Find…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), [this] { showSearch(); });
    auto *splitRightA = add(tr("Split right"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), [this] { splitRight(); });
    auto *splitDownA = add(tr("Split down"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), [this] { splitDown(); });
    auto *selectAllA = add(tr("Select all"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A), [this] {
        if (auto *t = currentTerminal())
            t->selectAll();
    });
    auto *clearA = add(tr("Clear scrollback"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K), [this] {
        if (auto *t = currentTerminal())
            t->clearScrollback();
    });
    auto *zoomIn = add(tr("Zoom in"), QKeySequence(Qt::CTRL | Qt::Key_Plus), [this] { zoom(1); });
    auto *zoomOut = add(tr("Zoom out"), QKeySequence(Qt::CTRL | Qt::Key_Minus), [this] { zoom(-1); });
    auto *zoomReset = add(tr("Reset zoom"), QKeySequence(Qt::CTRL | Qt::Key_0), [this] { zoom(0); });
    auto *prefs = add(tr("Preferences…"), QKeySequence(Qt::CTRL | Qt::Key_Comma), [this] { openPreferences(); });
    auto *quit = add(tr("Quit"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Q), [this] { close(); });

    add(tr("Next tab"), QKeySequence(Qt::CTRL | Qt::Key_PageDown), [this] {
        m_tabs->setCurrentIndex((m_tabs->currentIndex() + 1) % std::max(1, m_tabs->count()));
    });
    add(tr("Previous tab"), QKeySequence(Qt::CTRL | Qt::Key_PageUp), [this] {
        m_tabs->setCurrentIndex((m_tabs->currentIndex() + m_tabs->count() - 1) % std::max(1, m_tabs->count()));
    });
    add({}, QKeySequence(Qt::ALT | Qt::Key_Left), [this] { if (auto *v = currentView()) v->focusNeighbor(-1, 0); });
    add({}, QKeySequence(Qt::ALT | Qt::Key_Right), [this] { if (auto *v = currentView()) v->focusNeighbor(1, 0); });
    add({}, QKeySequence(Qt::ALT | Qt::Key_Up), [this] { if (auto *v = currentView()) v->focusNeighbor(0, -1); });
    add({}, QKeySequence(Qt::ALT | Qt::Key_Down), [this] { if (auto *v = currentView()) v->focusNeighbor(0, 1); });
    // Ctrl+= is what most keyboards send for "zoom in" without Shift.
    add({}, QKeySequence(Qt::CTRL | Qt::Key_Equal), [this] { zoom(1); });

    auto *menu = new QMenu(this);
    for (QAction *a : {newTabA, splitRightA, splitDownA})
        menu->addAction(a);
    menu->addSeparator();
    for (QAction *a : {copyA, pasteA, selectAllA, findA, clearA})
        menu->addAction(a);
    menu->addSeparator();
    for (QAction *a : {zoomIn, zoomOut, zoomReset, prefs})
        menu->addAction(a);
    menu->addSeparator();
    menu->addAction(closeA);
    menu->addAction(quit);

    auto *button = new QToolButton(m_tabs);
    button->setText(QStringLiteral("☰"));
    button->setToolTip(tr("Menu"));
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    m_tabs->setCornerWidget(button, Qt::TopRightCorner);
}

} // namespace gt
