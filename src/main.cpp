#include "config/Config.h"
#include "gpu/RenderSurface.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    gus::gpu::configureDefaultFormat(); // must run before the application object exists
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("gus-terminal"));
    QApplication::setApplicationDisplayName(QStringLiteral("GUS Terminal"));
    QApplication::setOrganizationName(QStringLiteral("GreatUserTeam"));
    QApplication::setApplicationVersion(QStringLiteral(GUS_TERMINAL_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("gus-terminal"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("GUS Terminal — terminal emulator of the Colibry desktop"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption cwdOpt({QStringLiteral("w"), QStringLiteral("working-directory")},
                              QStringLiteral("Start in <directory>."), QStringLiteral("directory"));
    QCommandLineOption configOpt(QStringLiteral("config"), QStringLiteral("Use <file> as the configuration file."),
                                 QStringLiteral("file"));
    QCommandLineOption execOpt({QStringLiteral("e"), QStringLiteral("execute")},
                               QStringLiteral("Run <program> instead of the shell (extra arguments follow)."),
                               QStringLiteral("program"));
    parser.addOption(cwdOpt);
    parser.addOption(configOpt);
    parser.addOption(execOpt);
    parser.addPositionalArgument(QStringLiteral("args"), QStringLiteral("Arguments for --execute."), QStringLiteral("[args...]"));
    parser.process(app);

    const QString configPath = parser.isSet(configOpt) ? parser.value(configOpt) : gt::Config::defaultPath();
    const gt::Config config = gt::Config::load(configPath);

    gt::MainWindow window(config, configPath);
    window.show();
    window.newTab(parser.value(cwdOpt), parser.value(execOpt), parser.positionalArguments());
    return app.exec();
}
