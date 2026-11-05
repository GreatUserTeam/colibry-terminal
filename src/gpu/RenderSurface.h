#pragma once

#include <QRect>
#include <QString>
#include <QWidget>

#include <functional>

class QPainter;

/// Reusable GPU-rendering module for Colibry (GUS) Qt Widgets applications.
///
/// A render surface is a child widget that fills its parent and draws through a callback. It is
/// backed by QOpenGLWidget (QPainter runs on Qt's OpenGL paint engine, i.e. on the GPU) when
/// OpenGL is usable, and by a plain QWidget otherwise. The surface is transparent to mouse
/// events, so the parent keeps receiving input.
namespace gus::gpu {

enum class Backend { Software, OpenGL };

/// What the user asked for through GUS_RENDERER: "auto" (default), "gpu"/"opengl" or "software".
QString requestedRenderer();

/// True if this build has OpenGL support and a context can really be created on this system
/// (the probe runs once and is cached).
bool openGlUsable();

/// Resolves the request: OpenGL when usable and not disabled, otherwise software.
Backend selectBackend();
QString backendName(Backend backend);

/// Call before QApplication is constructed: asks for an alpha channel (translucent windows),
/// vsync and shared contexts between all GL surfaces of the process.
void configureDefaultFormat();

using PaintFunction = std::function<void(QPainter &painter, const QRect &rect)>;

struct RenderSurface {
    QWidget *widget = nullptr;
    Backend backend = Backend::Software;
};

/// Creates a surface as a child of `parent`. The caller keeps `widget` and resizes it with the parent.
RenderSurface createRenderSurface(QWidget *parent, PaintFunction paint);

} // namespace gus::gpu
