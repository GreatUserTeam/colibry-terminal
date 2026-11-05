#include "gpu/RenderSurface.h"
#include "ui/TerminalWidget.h"

#include <QtTest>

using namespace gt;

class TestGpu : public QObject {
    Q_OBJECT
private slots:
    void softwareCanAlwaysBeForced()
    {
        qputenv("GUS_RENDERER", "software");
        QCOMPARE(gus::gpu::requestedRenderer(), QString("software"));
        QCOMPARE(gus::gpu::selectBackend(), gus::gpu::Backend::Software);
        qunsetenv("GUS_RENDERER");
    }

    void requestIsParsed()
    {
        qputenv("GUS_RENDERER", "OpenGL");
        QCOMPARE(gus::gpu::requestedRenderer(), QString("gpu"));
        qputenv("GUS_RENDERER", "nonsense");
        QCOMPARE(gus::gpu::requestedRenderer(), QString("auto"));
        qunsetenv("GUS_RENDERER");
    }

    void terminalRendersOnTheSelectedBackend()
    {
        Config c;
        c.shell = "/bin/sh";
        TerminalWidget t(c);
        t.resize(640, 320);
        t.show();
        QVERIFY(QTest::qWaitForWindowExposed(&t));
        t.feed("\x1b[48;2;200;0;0m      \x1b[0m gpu\r\n");
        QTest::qWait(100);
        const QImage img = t.grab().toImage();
        QVERIFY(!img.isNull());
        // The red block is in the first row, so its pixels prove that painting reached the surface.
        bool red = false;
        for (int x = 0; x < 6 * t.cellSize().width() && !red; ++x)
            red = img.pixelColor(x, 2).red() > 150 && img.pixelColor(x, 2).green() < 60;
        QVERIFY2(red, qPrintable(gus::gpu::backendName(t.renderBackend())));
    }

    void openGlBackendWhenAvailable()
    {
        if (!gus::gpu::openGlUsable())
            QSKIP("OpenGL is not usable on this platform; software fallback is used", SkipSingle);
        qputenv("GUS_RENDERER", "gpu");
        Config c;
        TerminalWidget t(c);
        QCOMPARE(t.renderBackend(), gus::gpu::Backend::OpenGL);
        qunsetenv("GUS_RENDERER");
    }
};

QTEST_MAIN(TestGpu)
#include "test_gpu.moc"
