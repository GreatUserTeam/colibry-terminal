#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

namespace gt {

/// The search strip shown above the terminal (Ctrl+Shift+F).
class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget *parent = nullptr);

    void activate();
    void setResult(int current, int total);
    QString text() const { return m_edit->text(); }
    bool caseSensitive() const { return m_case->isChecked(); }

signals:
    void queryChanged(const QString &text, bool caseSensitive);
    void next();
    void previous();
    void closed();

protected:
    void keyPressEvent(QKeyEvent *) override;

private:
    QLineEdit *m_edit;
    QCheckBox *m_case;
    QLabel *m_count;
};

} // namespace gt
