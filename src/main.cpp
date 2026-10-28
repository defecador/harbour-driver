#include <sailfishapp.h>

#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QColor>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickView>
#include <QScreen>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>

#include "AppSettings.h"
#include "AppTools.h"
#include "HereClient.h"
#include "HereUrls.h"
#include "Navigator.h"
#include "SearchController.h"
#include "TripPlanner.h"


int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    // Must match the [X-Sailjail] OrganizationName/ApplicationName in harbour-driver.desktop:
    // Sailjail binds its persistent, writable per-app data directory based on those names, and
    // QSettings resolves its config path from these same names, so a mismatch here silently
    // sends every settings write into a throwaway sandbox view that never survives a restart.
    app->setApplicationName(QStringLiteral("Driver"));
    app->setOrganizationName(QStringLiteral("org.driver"));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    bool fixtures = false;
    QString devScreen;
    QString devGrab;
    bool devDark = false;
    bool devLight = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
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

    const int regular = QFontDatabase::addApplicationFont(SailfishApp::pathTo(QStringLiteral("assets/fonts/FiraSans-Regular.ttf")).toLocalFile());
    QFontDatabase::addApplicationFont(SailfishApp::pathTo(QStringLiteral("assets/fonts/FiraSans-Medium.ttf")).toLocalFile());
    QFontDatabase::addApplicationFont(SailfishApp::pathTo(QStringLiteral("assets/fonts/FiraSans-SemiBold.ttf")).toLocalFile());
    QFontDatabase::addApplicationFont(SailfishApp::pathTo(QStringLiteral("assets/fonts/MaterialIconsRound-Regular.otf")).toLocalFile());
    if (regular >= 0) {
        const QStringList families = QFontDatabase::applicationFontFamilies(regular);
        if (!families.isEmpty())
            app->setFont(QFont(families.first()));
    }

    QQuickView *view = SailfishApp::createView();

    AppSettings settings;
    HereClient client;
    if (fixtures)
        client.setFixtureDir(SailfishApp::pathTo(QStringLiteral("fixtures")).toLocalFile());
    TripPlanner planner(&settings, &client);
    SearchController search(&settings, &client);
    Navigator navigator(&settings, &client);

    const qreal shortSide = qMin(view->screen()->size().width(), view->screen()->size().height());
    const qreal dp = shortSide / 360.0;
    AppTools tools(&settings, fixtures, devScreen, devDark, devLight, dp);

    QQmlContext *ctx = view->rootContext();
    ctx->setContextProperty(QStringLiteral("AppSettings"), &settings);
    ctx->setContextProperty(QStringLiteral("TripPlanner"), &planner);
    ctx->setContextProperty(QStringLiteral("SearchController"), &search);
    ctx->setContextProperty(QStringLiteral("Navigator"), &navigator);
    ctx->setContextProperty(QStringLiteral("AppTools"), &tools);
    ctx->setContextProperty(QStringLiteral("devGrabPath"), devGrab);

    view->engine()->addImportPath(SailfishApp::pathTo(QStringLiteral("qml")).toLocalFile());
    QObject::connect(view->engine(), &QQmlEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &e : warnings)
            qWarning("QML: %s", qPrintable(e.toString()));
    });
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-driver.qml")));
    if (view->status() == QQuickView::Error) {
        for (const QQmlError &e : view->errors())
            qWarning("QML load error: %s", qPrintable(e.toString()));
    }
    view->setTitle(QStringLiteral("Driver"));
    view->show();

    if (!devGrab.isEmpty())
        tools.grabAndQuit(view, devGrab);

    return app->exec();
}

