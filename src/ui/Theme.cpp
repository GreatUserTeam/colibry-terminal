#include "Theme.h"

#include <QColor>

namespace gt {

namespace {
QString hex(const Rgb &c)
{
    return QColor(c.r, c.g, c.b).name();
}
QString mix(const Rgb &a, const Rgb &b, double t)
{
    return QColor(int(a.r + (b.r - a.r) * t), int(a.g + (b.g - a.g) * t), int(a.b + (b.b - a.b) * t)).name();
}
} // namespace

QColor chromeColor(const ColorScheme &s)
{
    return QColor(mix(s.background, s.foreground, s.dark ? 0.06 : 0.04));
}

QString buildStyleSheet(const ColorScheme &s)
{
    const QString bg = hex(s.background);
    const QString fg = hex(s.foreground);
    const QString chrome = mix(s.background, s.foreground, s.dark ? 0.06 : 0.04);
    const QString hover = mix(s.background, s.foreground, s.dark ? 0.12 : 0.09);
    const QString border = mix(s.background, s.foreground, 0.18);
    const QString muted = mix(s.background, s.foreground, 0.55);
    const QString accent = QStringLiteral("#3B82F6");

    QString css = QStringLiteral(R"(
QDialog { background: %CHROME%; color: %FG%; }
QMainWindow { color: %FG%; }
QWidget#central { background: transparent; }
QTabWidget::pane { border: none; }
QTabBar { background: transparent; qproperty-drawBase: 0; }
QTabBar::tab {
    background: transparent; color: %MUTED%;
    padding: 6px 14px; margin: 6px 2px 4px 2px;
    border-radius: 8px; min-width: 80px; max-width: 220px;
}
QTabBar::tab:hover { background: %HOVER%; color: %FG%; }
QTabBar::tab:selected { background: %HOVER%; color: %FG%; border-bottom: 2px solid %ACCENT%; }
QTabBar::close-button { subcontrol-position: right; border-radius: 4px; }
QTabBar::close-button:hover { background: %BORDER%; }
QToolButton { background: transparent; color: %FG%; border: none; border-radius: 8px; padding: 4px 8px; }
QToolButton:hover { background: %HOVER%; }
QToolButton::menu-indicator { image: none; }
QMenu { background: %CHROME%; color: %FG%; border: 1px solid %BORDER%; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 6px 22px 6px 14px; border-radius: 6px; }
QMenu::item:selected { background: %ACCENT%; color: white; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 5px 8px; }
QLineEdit, QComboBox, QSpinBox, QFontComboBox {
    background: %BG%; color: %FG%; border: 1px solid %BORDER%; border-radius: 8px; padding: 5px 8px;
    selection-background-color: %ACCENT%;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid %ACCENT%; }
QComboBox QAbstractItemView { background: %CHROME%; color: %FG%; border: 1px solid %BORDER%; selection-background-color: %ACCENT%; }
QPushButton { background: %HOVER%; color: %FG%; border: none; border-radius: 8px; padding: 6px 16px; }
QPushButton:hover { background: %BORDER%; }
QPushButton:default { background: %ACCENT%; color: white; }
QCheckBox, QLabel { color: %FG%; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px; border: 1px solid %BORDER%; background: %BG%; }
QCheckBox::indicator:checked { background: %ACCENT%; border-color: %ACCENT%; }
QSlider::groove:horizontal { height: 4px; background: %BORDER%; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; margin: -5px 0; border-radius: 7px; background: %ACCENT%; }
QWidget#searchBar { background: %CHROME%; border-bottom: 1px solid %BORDER%; }
QSplitter::handle { background: %BORDER%; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: %BORDER%; border-radius: 4px; min-height: 28px; margin: 2px; }
QScrollBar::handle:vertical:hover { background: %MUTED%; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QToolTip { background: %CHROME%; color: %FG%; border: 1px solid %BORDER%; border-radius: 6px; padding: 4px; }
)");
    css.replace("%CHROME%", chrome).replace("%FG%", fg).replace("%BG%", bg).replace("%HOVER%", hover)
        .replace("%BORDER%", border).replace("%MUTED%", muted).replace("%ACCENT%", accent);
    return css;
}

} // namespace gt
