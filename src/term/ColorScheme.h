#pragma once

#include "Cell.h"

#include <array>
#include <QString>
#include <QStringList>

namespace gt {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb &) const = default;
};

/// A complete colour set: default colours plus the 16 ANSI colours.
struct ColorScheme {
    QString name;
    Rgb foreground;
    Rgb background;
    Rgb cursor;
    Rgb selection;
    std::array<Rgb, 16> ansi{};
    bool dark = true;
};

/// Built-in schemes. The first one is the default.
const std::vector<ColorScheme> &builtinSchemes();
/// Returns the scheme with that name, or the default one when the name is unknown.
const ColorScheme &schemeByName(const QString &name);
QStringList schemeNames();

/// Turns what a program asked for into a concrete colour.
/// `foreground` selects the default colour used for Color::Kind::Default.
Rgb resolveColor(const Color &color, const ColorScheme &scheme, bool foreground);

} // namespace gt
