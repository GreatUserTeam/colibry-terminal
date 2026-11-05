#pragma once

#include <cstdint>
#include <vector>

namespace gt {

/// A colour as written by the program: the default, one of the 256 palette entries, or 24-bit RGB.
struct Color {
    enum class Kind : std::uint8_t { Default, Indexed, Rgb };

    Kind kind = Kind::Default;
    std::uint8_t r = 0; ///< red, or the palette index for Kind::Indexed
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    static Color indexed(std::uint8_t index)
    {
        Color c;
        c.kind = Kind::Indexed;
        c.r = index;
        return c;
    }
    static Color rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
    {
        Color c;
        c.kind = Kind::Rgb;
        c.r = red;
        c.g = green;
        c.b = blue;
        return c;
    }
    bool operator==(const Color &) const = default;
};

enum AttrFlag : std::uint16_t {
    Bold = 1 << 0,
    Italic = 1 << 1,
    Underline = 1 << 2,
    Inverse = 1 << 3,
    Strike = 1 << 4,
    Dim = 1 << 5,
    WideLead = 1 << 6, ///< first cell of a double-width character
    WideTail = 1 << 7, ///< second cell of a double-width character (holds no glyph)
};

struct Attr {
    Color fg;
    Color bg;
    std::uint16_t flags = 0;

    bool has(std::uint16_t flag) const { return (flags & flag) != 0; }
    bool operator==(const Attr &) const = default;
};

struct Cell {
    char32_t ch = U' ';
    Attr attr;
};

struct Line {
    std::vector<Cell> cells;
    bool wrapped = false; ///< the text continued on the next line (no newline was typed)
};

} // namespace gt
