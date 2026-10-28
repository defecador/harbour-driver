// Entry point for the Qt 6 builds (Android, and desktop for development).
// The Sailfish build uses main.cpp.
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLocationPermission>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickWindow>
#include <QScreen>
#include <QStyleHints>

#include "AppSettings.h"
#include "AppTools.h"
#include "HereClient.h"
#include "Navigator.h"
#include "SearchController.h"
#include "TripPlanner.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Same names as the Sailfish build so a settings.conf copied from the
    // phone lands in the right place.
    app.setApplicationName(QStringLiteral("Driver"));
    app.setOrganizationName(QStringLiteral("org.driver"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    bool fixtures = false;
    QString devScreen;
    QString devGrab;
    bool devDark = false;
    bool devLight = false;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if (a == QLatin1String("--fixtures"))
            fixtures = true;
        else if (a.startsWith(QLatin1String("--dev-screen=")))
            devScreen = a.mid(13);
        else if (a.startsWith(QLatin1String("--dev-grab=")))
            devGrab = a.mid(11);
        else if (a == QLatin1String("--dev-dark"))
            devDark = true;
        else if (a == QLatin1String("--dev-light"))
            devLight = true;
    }

    const int regular = QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/FiraSans-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/FiraSans-Medium.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/FiraSans-SemiBold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/assets/fonts/MaterialIconsRound-Regular.otf"));
    if (regular >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(regular);
        if (!families.isEmpty())
            app.setFont(QFont(families.first()));
    }

    AppSettings settings;
    HereClient client;
    if (fixtures)
        client.setFixtureDir(QStringLiteral(":/fixtures"));
    TripPlanner planner(&settings, &client);
    SearchController search(&settings, &client);
    Navigator navigator(&settings, &client);

    // Qt 6 sizes are already in density-independent pixels (1 per Android dp),
    // which is the unit the layout's `dp` expects at 1.0.
    AppTools tools(&settings, fixtures, devScreen, devDark, devLight, 1.0);

    // GPS needs a run-time grant on Android (and is a no-op on desktop).
    QLocationPermission location;
    location.setAccuracy(QLocationPermission::Precise);
    if (app.checkPermission(location) == Qt::PermissionStatus::Undetermined)
        app.requestPermission(location, &app, [](const QPermission &) {});

    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(QStringLiteral("AppSettings"), &settings);
    ctx->setContextProperty(QStringLiteral("TripPlanner"), &planner);
    ctx->setContextProperty(QStringLiteral("SearchController"), &search);
    ctx->setContextProperty(QStringLiteral("Navigator"), &navigator);
    ctx->setContextProperty(QStringLiteral("AppTools"), &tools);
    ctx->setContextProperty(QStringLiteral("devGrabPath"), devGrab);

    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &e : warnings)
            qWarning("QML: %s", qPrintable(e.toString()));
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qml/harbour-driver.qml")));

    if (!devGrab.isEmpty() && !engine.rootObjects().isEmpty()) {
        if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
            tools.grabAndQuit(w, devGrab);
    }

    return app.exec();
}
