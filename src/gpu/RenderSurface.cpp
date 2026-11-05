#include "RenderSurface.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QSurfaceFormat>

#ifdef GUS_GPU_AVAILABLE
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLWidget>
#endif

namespace gus::gpu {

namespace {

class SoftwareSurface : public QWidget {
public:
    SoftwareSurface(QWidget *parent, PaintFunction fn)
        : QWidget(parent)
        , m_paint(std::move(fn))
    {
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        m_paint(p, rect());
    }

private:
    PaintFunction m_paint;
};

#ifdef GUS_GPU_AVAILABLE
class GlSurface : public QOpenGLWidget {
public:
    GlSurface(QWidget *parent, PaintFunction fn)
        : QOpenGLWidget(parent)
        , m_paint(std::move(fn))
    {
        setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    }

protected:
    void paintGL() override
    {
        QPainter p(this);
        m_paint(p, rect());
    }

private:
    PaintFunction m_paint;
};
#endif

} // namespace

QString requestedRenderer()
{
    const QString v = qEnvironmentVariable("GUS_RENDERER", QStringLiteral("auto")).trimmed().toLower();
    if (v == QLatin1String("gpu") || v == QLatin1String("opengl"))
        return QStringLiteral("gpu");
    if (v == QLatin1String("software") || v == QLatin1String("cpu"))
        return QStringLiteral("software");
    return QStringLiteral("auto");
}

bool openGlUsable()
{
#ifdef GUS_GPU_AVAILABLE
    static const bool usable = [] {
        if (!QGuiApplication::instance())
            return false;
        // These platforms cannot present OpenGL widgets (Qt 6.4's offscreen backing store even
        // crashes in rhiFlush when a QOpenGLWidget is shown), so they always use software.
        const QString platform = QGuiApplication::platformName();
        if (platform == QLatin1String("minimal") || platform == QLatin1String("vnc")
            || platform == QLatin1String("linuxfb") || platform == QLatin1String("offscreen"))
            return false;
        QOpenGLContext ctx;
        if (!ctx.create())
            return false;
        QOffscreenSurface surface;
        surface.setFormat(ctx.format());
        surface.create();
        return surface.isValid() && ctx.makeCurrent(&surface);
    }();
    return usable;
#else
    return false;
#endif
}

Backend selectBackend()
{
    if (requestedRenderer() == QLatin1String("software"))
        return Backend::Software;
    return openGlUsable() ? Backend::OpenGL : Backend::Software;
}

QString backendName(Backend backend)
{
    return backend == Backend::OpenGL ? QStringLiteral("OpenGL") : QStringLiteral("Software");
}

void configureDefaultFormat()
{
#ifdef GUS_GPU_AVAILABLE
    QSurfaceFormat format;
    format.setAlphaBufferSize(8);   // translucent windows
    format.setSwapInterval(1);      // vsync
    format.setSamples(0);           // text does not need MSAA; saves memory bandwidth
    QSurfaceFormat::setDefaultFormat(format);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts); // one GL context for all tabs and splits
#endif
}

RenderSurface createRenderSurface(QWidget *parent, PaintFunction paint)
{
    RenderSurface result;
#ifdef GUS_GPU_AVAILABLE
    if (selectBackend() == Backend::OpenGL) {
        result.widget = new GlSurface(parent, std::move(paint));
        result.backend = Backend::OpenGL;
    }
#endif
    if (!result.widget) {
        result.widget = new SoftwareSurface(parent, std::move(paint));
        result.backend = Backend::Software;
    }
    result.widget->setAttribute(Qt::WA_TransparentForMouseEvents);
    result.widget->setFocusPolicy(Qt::NoFocus);
    return result;
}

} // namespace gus::gpu
