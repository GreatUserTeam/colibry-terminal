# GUS Terminal

Terminal emulator of the Colibry desktop. C++20, Qt 6 Widgets, a real PTY (`forkpty`) and its own
VT100/xterm emulator. No dependencies besides Qt.

## Build

```sh
cmake -B build
cmake --build build
ctest --test-dir build          # unit, PTY and UI tests
sudo cmake --install build      # binary, .desktop file and SVG icon
```

Requirements: Qt 6 (Core, Gui, Widgets; Test for the tests; OpenGLWidgets is optional), CMake 3.16+,
a C++20 compiler, Linux / BSD / macOS. Useful options:

| Option | Default | Meaning |
| --- | --- | --- |
| `GUS_TERMINAL_BUILD_TESTS` | ON | build `tests/` |
| `GUS_TERMINAL_WERROR` | OFF | warnings are errors |
| `GUS_GPU` | ON | OpenGL rendering when `Qt6::OpenGLWidgets` is found |

## Features

* PTY: bash, zsh, fish, vim, nano, less, htop run as in any terminal; `TERM=xterm-256color`,
  `COLORTERM=truecolor`; the window size is forwarded (`SIGWINCH`).
* Emulator: Paul Williams' parser state machine, UTF-8, wide (CJK) characters, 16 / 256 / 24-bit colours,
  bold / italic / underline / inverse / strike / dim, scroll regions, alternate screen, DEC line drawing,
  bracketed paste, scrollback, title (OSC 0/2), DSR/DA replies.
* Tabs and splits (right / down, nested), pane focus with `Alt+Arrows`.
* Colour schemes: Colibry Dark, Colibry Light, Solarized Dark. Window opacity 30–100 %.
* Fonts: any installed family (`JetBrains Mono`, `FiraCode Nerd Font`, …); ligatures can be switched off.
* Search in the output and the scrollback (case sensitive or not).
* GPU rendering through `QOpenGLWidget` with automatic fallback to software (see below).

## Keys

| Key | Action |
| --- | --- |
| `Ctrl+Shift+T` / `Ctrl+Shift+W` | new tab / close pane (last pane closes the tab) |
| `Ctrl+Shift+C` / `Ctrl+Shift+V` | copy / paste |
| `Ctrl+Shift+F` | search (`Enter` next, `Shift+Enter` previous, `Esc` close) |
| `Ctrl+Shift+D` / `Ctrl+Shift+E` | split right / split down |
| `Alt+Arrows` | move between panes |
| `Ctrl+PageUp/PageDown` | previous / next tab |
| `Ctrl++`, `Ctrl+-`, `Ctrl+0` | zoom |
| `Shift+PageUp/PageDown` | scroll the history |
| `Ctrl+Shift+K` | clear scrollback |
| `Ctrl+,` | preferences |

## Configuration

`~/.config/gus-terminal/config.json` (`$XDG_CONFIG_HOME` is honoured). Values are validated and clamped.

```json
{
  "colorScheme": "Colibry Dark",
  "fontFamily": "JetBrains Mono",
  "fontSize": 12,
  "ligatures": true,
  "opacity": 1.0,
  "scrollbackLines": 10000,
  "cursorShape": 0,
  "cursorBlink": true,
  "shell": "",
  "windowWidth": 980,
  "windowHeight": 620
}
```

Command line: `gus-terminal [-w DIR] [--config FILE] [-e PROGRAM [ARGS...]]`.

## GPU rendering and reuse in other Colibry apps

`src/gpu/RenderSurface.{h,cpp}` and `cmake/GusGpu.cmake` are a self-contained module:

```cmake
include(cmake/GusGpu.cmake)        # option GUS_GPU
gus_enable_gpu(my_app_core)        # links Qt6::OpenGLWidgets, defines GUS_GPU_AVAILABLE
```

```cpp
gus::gpu::configureDefaultFormat();                    // before QApplication
auto s = gus::gpu::createRenderSurface(this, [](QPainter &p, const QRect &r) { /* draw */ });
```

The surface is a `QOpenGLWidget` (QPainter on Qt's OpenGL paint engine) when a GL context can be
created, otherwise a plain `QWidget`. Override with `GUS_RENDERER=gpu|software|auto`.
The `offscreen`, `minimal`, `vnc` and `linuxfb` platforms always use software.

## Limits (be aware)

* The emulator is custom, not qtermwidget/VTE. Mouse reporting (xterm 1000/1006), Sixel, hyperlinks (OSC 8),
  combining characters and right-to-left text are not implemented.
* Blur behind the window is not implemented: only translucency (needs a compositing window manager).
* Ligatures need a font that has them (JetBrains Mono, Fira Code); Qt 6.4 draws them per text run.
* Verified on Linux with Qt 6.4.2, GCC 13 and clang. Not verified: Windows (no `forkpty`), macOS, Wayland,
  a physical GPU (OpenGL was exercised through Mesa software rendering).

## Layout

```
CMakeLists.txt
cmake/GusGpu.cmake
src/        main.cpp, gpu/, term/ (Screen, Parser, Input, ColorScheme), pty/, config/, ui/
packaging/  gus-terminal.desktop, gus-terminal.svg
tests/      test_vt, test_input, test_config, test_pty, test_programs, test_terminal_ui, test_gpu
```

License: GPL-3.0 (see `LICENSE`).
