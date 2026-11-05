#include "PreferencesDialog.h"

#include "term/ColorScheme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace gt {

PreferencesDialog::PreferencesDialog(const Config &config, QWidget *parent)
    : QDialog(parent)
    , m_base(config)
{
    setWindowTitle(tr("Preferences"));
    setMinimumWidth(420);

    m_scheme = new QComboBox(this);
    m_scheme->addItems(schemeNames());
    m_scheme->setCurrentText(config.colorScheme);

    m_font = new QFontComboBox(this);
    m_font->setFontFilters(QFontComboBox::MonospacedFonts);
    m_font->setEditable(true); // allows families that Qt does not flag as monospaced (Nerd Fonts)
    m_font->setCurrentText(config.fontFamily);

    m_size = new QSpinBox(this);
    m_size->setRange(6, 48);
    m_size->setValue(config.fontSize);

    m_ligatures = new QCheckBox(tr("Font ligatures"), this);
    m_ligatures->setChecked(config.ligatures);

    m_opacity = new QSlider(Qt::Horizontal, this);
    m_opacity->setRange(30, 100);
    m_opacity->setValue(static_cast<int>(config.opacity * 100));

    m_scrollback = new QSpinBox(this);
    m_scrollback->setRange(0, 1000000);
    m_scrollback->setSingleStep(1000);
    m_scrollback->setValue(config.scrollbackLines);

    m_cursor = new QComboBox(this);
    m_cursor->addItems({tr("Block"), tr("Bar"), tr("Underline")});
    m_cursor->setCurrentIndex(config.cursorShape);
    m_blink = new QCheckBox(tr("Blinking cursor"), this);
    m_blink->setChecked(config.cursorBlink);

    m_shell = new QLineEdit(config.shell, this);
    m_shell->setPlaceholderText(tr("Login shell"));

    auto *form = new QFormLayout;
    form->addRow(tr("Colour scheme"), m_scheme);
    form->addRow(tr("Font"), m_font);
    form->addRow(tr("Font size"), m_size);
    form->addRow(QString(), m_ligatures);
    form->addRow(tr("Opacity"), m_opacity);
    form->addRow(tr("Scrollback lines"), m_scrollback);
    form->addRow(tr("Cursor"), m_cursor);
    form->addRow(QString(), m_blink);
    form->addRow(tr("Shell"), m_shell);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(buttons);

    connect(m_scheme, &QComboBox::currentTextChanged, this, [this] { emitChanged(); });
    connect(m_font, &QFontComboBox::currentTextChanged, this, [this] { emitChanged(); });
    connect(m_size, &QSpinBox::valueChanged, this, [this] { emitChanged(); });
    connect(m_ligatures, &QCheckBox::toggled, this, [this] { emitChanged(); });
    connect(m_opacity, &QSlider::valueChanged, this, [this] { emitChanged(); });
    connect(m_scrollback, &QSpinBox::valueChanged, this, [this] { emitChanged(); });
    connect(m_cursor, &QComboBox::currentIndexChanged, this, [this] { emitChanged(); });
    connect(m_blink, &QCheckBox::toggled, this, [this] { emitChanged(); });
    connect(m_shell, &QLineEdit::textChanged, this, [this] { emitChanged(); });
}

Config PreferencesDialog::config() const
{
    Config c = m_base;
    c.colorScheme = m_scheme->currentText();
    c.fontFamily = m_font->currentText();
    c.fontSize = m_size->value();
    c.ligatures = m_ligatures->isChecked();
    c.opacity = m_opacity->value() / 100.0;
    c.scrollbackLines = m_scrollback->value();
    c.cursorShape = m_cursor->currentIndex();
    c.cursorBlink = m_blink->isChecked();
    c.shell = m_shell->text().trimmed();
    c.sanitize();
    return c;
}

void PreferencesDialog::emitChanged()
{
    emit changed(config());
}

} // namespace gt
