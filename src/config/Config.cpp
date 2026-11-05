#include "Config.h"

#include "term/ColorScheme.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace gt {

QString Config::defaultPath()
{
    QString base = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config");
    return base + QStringLiteral("/gus-terminal/config.json");
}

void Config::sanitize()
{
    fontSize = std::clamp(fontSize, 6, 48);
    opacity = std::clamp(opacity, 0.3, 1.0);
    scrollbackLines = std::clamp(scrollbackLines, 0, 1000000);
    cursorShape = std::clamp(cursorShape, 0, 2);
    windowWidth = std::clamp(windowWidth, 320, 10000);
    windowHeight = std::clamp(windowHeight, 200, 10000);
    if (fontFamily.trimmed().isEmpty())
        fontFamily = QStringLiteral("JetBrains Mono");
    if (!schemeNames().contains(colorScheme))
        colorScheme = schemeNames().value(0);
}

Config Config::load(const QString &path)
{
    Config c;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const QJsonObject o = doc.object();
            c.colorScheme = o.value("colorScheme").toString(c.colorScheme);
            c.fontFamily = o.value("fontFamily").toString(c.fontFamily);
            c.fontSize = o.value("fontSize").toInt(c.fontSize);
            c.ligatures = o.value("ligatures").toBool(c.ligatures);
            c.opacity = o.value("opacity").toDouble(c.opacity);
            c.scrollbackLines = o.value("scrollbackLines").toInt(c.scrollbackLines);
            c.cursorShape = o.value("cursorShape").toInt(c.cursorShape);
            c.cursorBlink = o.value("cursorBlink").toBool(c.cursorBlink);
            c.shell = o.value("shell").toString(c.shell);
            c.windowWidth = o.value("windowWidth").toInt(c.windowWidth);
            c.windowHeight = o.value("windowHeight").toInt(c.windowHeight);
        }
    }
    c.sanitize();
    return c;
}

bool Config::save(const QString &path) const
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QJsonObject o;
    o["colorScheme"] = colorScheme;
    o["fontFamily"] = fontFamily;
    o["fontSize"] = fontSize;
    o["ligatures"] = ligatures;
    o["opacity"] = opacity;
    o["scrollbackLines"] = scrollbackLines;
    o["cursorShape"] = cursorShape;
    o["cursorBlink"] = cursorBlink;
    o["shell"] = shell;
    o["windowWidth"] = windowWidth;
    o["windowHeight"] = windowHeight;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}

} // namespace gt
