#pragma once

#include "config/Config.h"
#include "ui/SearchBar.h"
#include "ui/SplitView.h"

#include <QMainWindow>
#include <QTabWidget>

namespace gt {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /// `configPath` is where preferences are read from and written to.
    MainWindow(const Config &config, const QString &configPath, QWidget *parent = nullptr);

    SplitView *newTab(const QString &workingDirectory = {}, const QString &program = {}, const QStringList &args = {});
    int tabCount() const { return m_tabs->count(); }
    SplitView *currentView() const;
    TerminalWidget *currentTerminal() const;
    SearchBar *searchBar() const { return m_search; }
    const Config &config() const { return m_config; }

    void applyConfig(const Config &config, bool save);

public slots:
    void closeCurrentPane();
    void closeTab(int index);
    void splitRight();
    void splitDown();
    void showSearch();
    void zoom(int delta);
    void openPreferences();

protected:
    void closeEvent(QCloseEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    void buildActions();
    void hideSearch();
    void updateSearchStatus();
    QString currentDirectory() const;
    void onTabTitle(SplitView *view, const QString &title);

    Config m_config;
    QString m_configPath;
    QTabWidget *m_tabs;
    SearchBar *m_search;
};

} // namespace gt
