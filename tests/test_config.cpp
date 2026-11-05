#include "config/Config.h"
#include "term/ColorScheme.h"

#include <QtTest>

using namespace gt;

class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void missingFileGivesDefaults()
    {
        const Config c = Config::load("/nonexistent/dir/config.json");
        QCOMPARE(c.colorScheme, QString("Colibry Dark"));
        QCOMPARE(c.fontSize, 12);
    }
    void roundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("sub/config.json");
        Config c;
        c.fontFamily = "FiraCode Nerd Font";
        c.fontSize = 15;
        c.ligatures = false;
        c.opacity = 0.8;
        c.colorScheme = "Colibry Light";
        c.cursorShape = 2;
        QVERIFY(c.save(path));
        const Config r = Config::load(path);
        QCOMPARE(r.fontFamily, c.fontFamily);
        QCOMPARE(r.fontSize, 15);
        QVERIFY(!r.ligatures);
        QCOMPARE(r.opacity, 0.8);
        QCOMPARE(r.colorScheme, QString("Colibry Light"));
        QCOMPARE(r.cursorShape, 2);
    }
    void invalidValuesAreClamped()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("c.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"fontSize": 999, "opacity": 7, "scrollbackLines": -5, "colorScheme": "nope", "cursorShape": 9})");
        f.close();
        const Config c = Config::load(path);
        QCOMPARE(c.fontSize, 48);
        QCOMPARE(c.opacity, 1.0);
        QCOMPARE(c.scrollbackLines, 0);
        QCOMPARE(c.colorScheme, QString("Colibry Dark"));
        QCOMPARE(c.cursorShape, 2);
    }
    void brokenJsonGivesDefaults()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("c.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        QCOMPARE(Config::load(path).fontSize, 12);
    }
    void defaultPathHonoursXdg()
    {
        qputenv("XDG_CONFIG_HOME", "/tmp/xdg-test");
        QCOMPARE(Config::defaultPath(), QString("/tmp/xdg-test/gus-terminal/config.json"));
        qunsetenv("XDG_CONFIG_HOME");
        QVERIFY(Config::defaultPath().endsWith("/.config/gus-terminal/config.json"));
    }
    void schemes()
    {
        QVERIFY(schemeNames().size() >= 3);
        QCOMPARE(schemeByName("unknown").name, schemeNames().first());
        const ColorScheme s = schemeByName(QStringLiteral("Colibry Dark"));
        QCOMPARE(resolveColor(Color::rgb(1, 2, 3), s, true), (Rgb{1, 2, 3}));
        QCOMPARE(resolveColor(Color{}, s, false), s.background);
        QCOMPARE(resolveColor(Color::indexed(3), s, true), s.ansi[3]);
        QCOMPARE(resolveColor(Color::indexed(16), s, true), (Rgb{0, 0, 0}));
        QCOMPARE(resolveColor(Color::indexed(231), s, true), (Rgb{255, 255, 255}));
        QCOMPARE(resolveColor(Color::indexed(232), s, true), (Rgb{8, 8, 8}));
    }
};

QTEST_GUILESS_MAIN(TestConfig)
#include "test_config.moc"
