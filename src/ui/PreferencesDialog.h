#pragma once

#include "config/Config.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QSlider;
class QSpinBox;

namespace gt {

/// Settings dialog; changes are reported through `changed` while the user edits (live preview).
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    PreferencesDialog(const Config &config, QWidget *parent = nullptr);
    Config config() const;

signals:
    void changed(const Config &config);

private:
    void emitChanged();

    Config m_base;
    QComboBox *m_scheme;
    QFontComboBox *m_font;
    QSpinBox *m_size;
    QCheckBox *m_ligatures;
    QSlider *m_opacity;
    QSpinBox *m_scrollback;
    QComboBox *m_cursor;
    QCheckBox *m_blink;
    QLineEdit *m_shell;
};

} // namespace gt
