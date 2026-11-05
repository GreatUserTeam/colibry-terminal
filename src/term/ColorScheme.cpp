#include "ColorScheme.h"

namespace gt {

namespace {

constexpr Rgb rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return Rgb{r, g, b};
}

std::vector<ColorScheme> makeSchemes()
{
    std::vector<ColorScheme> s;

    // Colibry: the GUS house colours - a calm dark base with a blue accent.
    ColorScheme dark;
    dark.name = QStringLiteral("Colibry Dark");
    dark.foreground = rgb(0xE6, 0xE8, 0xEE);
    dark.background = rgb(0x1B, 0x1E, 0x24);
    dark.cursor = rgb(0x3B, 0x82, 0xF6);
    dark.selection = rgb(0x2C, 0x3A, 0x55);
    dark.ansi = {rgb(0x1B, 0x1E, 0x24), rgb(0xF7, 0x76, 0x8E), rgb(0x9E, 0xCE, 0x6A), rgb(0xE0, 0xAF, 0x68),
                 rgb(0x7A, 0xA2, 0xF7), rgb(0xBB, 0x9A, 0xF7), rgb(0x7D, 0xCF, 0xFF), rgb(0xC0, 0xC5, 0xCE),
                 rgb(0x4A, 0x50, 0x5E), rgb(0xFF, 0x9E, 0xB0), rgb(0xB5, 0xE8, 0x8A), rgb(0xF2, 0xCB, 0x8A),
                 rgb(0x9C, 0xB8, 0xFF), rgb(0xD2, 0xB8, 0xFF), rgb(0xA6, 0xE4, 0xFF), rgb(0xF5, 0xF6, 0xF8)};
    s.push_back(dark);

    ColorScheme light;
    light.name = QStringLiteral("Colibry Light");
    light.foreground = rgb(0x1F, 0x23, 0x30);
    light.background = rgb(0xF5, 0xF6, 0xF8);
    light.cursor = rgb(0x25, 0x63, 0xEB);
    light.selection = rgb(0xCF, 0xE0, 0xFF);
    light.dark = false;
    light.ansi = {rgb(0x1F, 0x23, 0x30), rgb(0xC8, 0x23, 0x3B), rgb(0x2F, 0x7D, 0x32), rgb(0x9A, 0x6B, 0x00),
                  rgb(0x1D, 0x4E, 0xD8), rgb(0x7C, 0x3A, 0xED), rgb(0x0E, 0x74, 0x90), rgb(0x6B, 0x72, 0x80),
                  rgb(0x4B, 0x55, 0x63), rgb(0xE0, 0x3A, 0x52), rgb(0x3F, 0x9E, 0x44), rgb(0xB4, 0x84, 0x00),
                  rgb(0x3B, 0x82, 0xF6), rgb(0x9D, 0x5C, 0xF7), rgb(0x06, 0x92, 0xB0), rgb(0xF5, 0xF6, 0xF8)};
    s.push_back(light);

    ColorScheme solarized;
    solarized.name = QStringLiteral("Solarized Dark");
    solarized.foreground = rgb(0x83, 0x94, 0x96);
    solarized.background = rgb(0x00, 0x2B, 0x36);
    solarized.cursor = rgb(0x93, 0xA1, 0xA1);
    solarized.selection = rgb(0x07, 0x36, 0x42);
    solarized.ansi = {rgb(0x07, 0x36, 0x42), rgb(0xDC, 0x32, 0x2F), rgb(0x85, 0x99, 0x00), rgb(0xB5, 0x89, 0x00),
                      rgb(0x26, 0x8B, 0xD2), rgb(0xD3, 0x36, 0x82), rgb(0x2A, 0xA1, 0x98), rgb(0xEE, 0xE8, 0xD5),
                      rgb(0x00, 0x2B, 0x36), rgb(0xCB, 0x4B, 0x16), rgb(0x58, 0x6E, 0x75), rgb(0x65, 0x7B, 0x83),
                      rgb(0x83, 0x94, 0x96), rgb(0x6C, 0x71, 0xC4), rgb(0x93, 0xA1, 0xA1), rgb(0xFD, 0xF6, 0xE3)};
    s.push_back(solarized);
    return s;
}

Rgb xterm256(int index)
{
    static const std::uint8_t levels[6] = {0x00, 0x5F, 0x87, 0xAF, 0xD7, 0xFF};
    if (index >= 232) {
        const std::uint8_t v = static_cast<std::uint8_t>(8 + (index - 232) * 10);
        return rgb(v, v, v);
    }
    const int i = index - 16;
    return rgb(levels[i / 36], levels[(i / 6) % 6], levels[i % 6]);
}

} // namespace

const std::vector<ColorScheme> &builtinSchemes()
{
    static const std::vector<ColorScheme> schemes = makeSchemes();
    return schemes;
}

const ColorScheme &schemeByName(const QString &name)
{
    for (const ColorScheme &s : builtinSchemes())
        if (s.name == name)
            return s;
    return builtinSchemes().front();
}

QStringList schemeNames()
{
    QStringList names;
    for (const ColorScheme &s : builtinSchemes())
        names << s.name;
    return names;
}

Rgb resolveColor(const Color &color, const ColorScheme &scheme, bool foreground)
{
    switch (color.kind) {
    case Color::Kind::Default:
        return foreground ? scheme.foreground : scheme.background;
    case Color::Kind::Indexed:
        return color.r < 16 ? scheme.ansi[color.r] : xterm256(color.r);
    case Color::Kind::Rgb:
        return rgb(color.r, color.g, color.b);
    }
    return foreground ? scheme.foreground : scheme.background;
}

} // namespace gt
