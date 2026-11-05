#pragma once

#include "config/Config.h"
#include "ui/TerminalWidget.h"

#include <QSplitter>
#include <QWidget>

namespace gt {

/// The content of one tab: a tree of nested splitters whose leaves are terminals.
class SplitView : public QWidget {
    Q_OBJECT
public:
    explicit SplitView(const Config &config, QWidget *parent = nullptr);

    /// Creates the first terminal. Returns false if the shell could not be started.
    bool startInitial(const QString &workingDirectory, const QString &program = {}, const QStringList &args = {});
    TerminalWidget *focusedTerminal() const;
    QList<TerminalWidget *> terminals() const;
    int paneCount() const { return terminals().size(); }

    /// Splits the focused pane. Horizontal puts the new pane to the right, Vertical below.
    TerminalWidget *split(Qt::Orientation orientation, const QString &workingDirectory);
    /// Closes the focused pane; returns true when it was the last one (the tab should go).
    bool closeFocused();
    void focusNeighbor(int dx, int dy);
    void applyConfig(const Config &config);

signals:
    void titleChanged(const QString &title);
    void lastPaneClosed();
    void bell();

private:
    TerminalWidget *createTerminal();
    void removeTerminal(TerminalWidget *t);
    void onProcessFinished();

    Config m_config;
    TerminalWidget *m_last = nullptr; ///< last focused pane
};

} // namespace gt
