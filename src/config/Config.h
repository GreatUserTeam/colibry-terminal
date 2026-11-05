#pragma once

#include <QString>

namespace gt {

/// User settings, stored as JSON in ~/.config/gus-terminal/config.json.
struct Config {
    QString colorScheme = QStringLiteral("Colibry Dark");
    QString fontFamily = QStringLiteral("JetBrains Mono");
    int fontSize = 12;
    bool ligatures = true;
    double opacity = 1.0;         ///< window opacity, 0.3 .. 1.0
    int scrollbackLines = 10000;
    int cursorShape = 0;          ///< 0 block, 1 bar, 2 underline
    bool cursorBlink = true;
    QString shell;                ///< empty = login shell
    int windowWidth = 980;
    int windowHeight = 620;

    /// Path of the config file ($XDG_CONFIG_HOME/gus-terminal/config.json).
    static QString defaultPath();
    /// Reads a file; a missing or broken file gives defaults. Values are clamped to valid ranges.
    static Config load(const QString &path);
    /// Writes the file atomically. Returns false on failure.
    bool save(const QString &path) const;
    /// Forces every field into its valid range.
    void sanitize();
};

} // namespace gt
