#include "headers/GACPD/Methods/appPath.h"
#include "headers/helpers/startupStorage.h"
#include "headers/mainwindow.h"
#include "headers/results/ResultsViewerController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

static QIcon applicationIcon()
{
#ifdef Q_OS_MACOS
    return QIcon(movisPath("assets/icons/main.icns"));
#else
    return QIcon(movisPath("assets/icons/main.ico"));
#endif
}

// Shown instead of the main window when MOVis has nowhere it can write to.
static int showStartupFailure(QGuiApplication &app, const StartupFailure &failure)
{
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("startupTitle", failure.title);
    engine.rootContext()->setContextProperty("startupMessage", failure.message);
    engine.load(QUrl(QStringLiteral("qrc:/qml/StartupError.qml")));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    app.exec();
    return 1;
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName("Evol");
    QCoreApplication::setApplicationName("MOVis");
    app.setWindowIcon(applicationIcon());

    // The look in qml/Theme.qml is drawn by the controls themselves, which the
    // native platform styles do not allow. Basic is the style that honours the
    // backgrounds and content items the components define.
    QQuickStyle::setStyle("Basic");

    const StartupFailure storageFailure = prepareStorage();
    if (storageFailure) {
        return showStartupFailure(app, storageFailure);
    }

    qDebug() << "MOVis immutable bundle resources:" << movisLocalRoot();
    qDebug() << "MOVis writable working root:" << movisWorkingRoot();
    qDebug() << "MOVis public results root:" << movisResultsRoot();

    // Each results viewer is its own window, created from QML on demand.
    qmlRegisterType<ResultsViewerController>(
        "MOVis", 1, 0, "ResultsViewerController");

    MainWindow controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &controller);

    const QUrl mainUrl(QStringLiteral("qrc:/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [mainUrl](QObject *object, const QUrl &objectUrl) {
            if (!object && objectUrl == mainUrl) {
                QCoreApplication::exit(1);
            }
        },
        Qt::QueuedConnection);

    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                     &controller, &MainWindow::saveSettings);

    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return app.exec();
}
