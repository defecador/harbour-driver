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
#include "HereClient.h"
#include "HereUrls.h"
#include "Navigator.h"
#include "SearchController.h"
#include "TripPlanner.h"

#ifndef APP_VERSION
#define APP_VERSION "0.8.1"
#endif

// Exposes small platform/dev-mode helpers to QML that don't belong on any single model.

// ---- 3D (vector) driving style helpers -------------------------------
// Flips the lightness of one CSS-style colour string (hsl/hsla/rgb/rgba/#hex)
// so a light map style becomes a dark one; any other string is returned as is.
static QString invertColorString(const QString &in)
{
    static const QRegularExpression fn(QStringLiteral("^\\s*(hsla?|rgba?)\\(([^)]*)\\)\\s*$"));
    QColor c;
    qreal alpha = 1.0;
    const QRegularExpressionMatch m = fn.match(in);
    if (m.hasMatch()) {
        QStringList parts = m.captured(2).split(QLatin1Char(','));
        for (QString &part : parts)
            part = part.trimmed().remove(QLatin1Char('%'));
        if (parts.size() < 3)
            return in;
        if (parts.size() > 3)
            alpha = parts.at(3).toDouble();
        if (m.captured(1).startsWith(QLatin1String("hsl"))) {
            c = QColor::fromHslF(qBound(0.0, parts.at(0).toDouble() / 360.0, 0.9999),
                                 qBound(0.0, parts.at(1).toDouble() / 100.0, 1.0),
                                 qBound(0.0, parts.at(2).toDouble() / 100.0, 1.0));
        } else {
            c = QColor(qBound(0, int(parts.at(0).toDouble()), 255), qBound(0, int(parts.at(1).toDouble()), 255),
                       qBound(0, int(parts.at(2).toDouble()), 255));
        }
    } else if (in.startsWith(QLatin1Char('#')) && QColor::isValidColor(in)) {
        c = QColor(in);
    } else {
        return in;
    }
    const qreal l = qBound(0.04, 1.0 - c.lightnessF(), 0.96);
    const qreal h = c.hslHueF() < 0 ? 0.0 : c.hslHueF();
    return QStringLiteral("hsla(%1,%2%,%3%,%4)")
            .arg(int(h * 360)).arg(int(c.hslSaturationF() * 100)).arg(int(l * 100)).arg(alpha);
}

static QJsonValue invertColors(const QJsonValue &v)
{
    if (v.isString())
        return invertColorString(v.toString());
    if (v.isArray()) {
        QJsonArray out;
        const QJsonArray in = v.toArray();
        for (const QJsonValue &e : in)
            out.append(invertColors(e));
        return out;
    }
    if (v.isObject()) {
        QJsonObject out;
        const QJsonObject in = v.toObject();
        for (auto it = in.begin(); it != in.end(); ++it)
            out[it.key()] = invertColors(it.value());
        return out;
    }
    return v;
}

class AppTools : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool fixtures READ fixtures CONSTANT)
    Q_PROPERTY(QString devScreen READ devScreen CONSTANT)
    Q_PROPERTY(bool devDark READ devDark CONSTANT)
    Q_PROPERTY(bool devLight READ devLight CONSTANT)
    Q_PROPERTY(qreal dp READ dp CONSTANT)
    Q_PROPERTY(qreal cm READ cm CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit AppTools(AppSettings *settings, bool fixtures, const QString &devScreen, bool devDark, bool devLight, qreal dp, QObject *parent = nullptr)
        : QObject(parent), m_settings(settings), m_fixtures(fixtures), m_devScreen(devScreen), m_devDark(devDark), m_devLight(devLight), m_dp(dp)
    {
    }

    bool fixtures() const { return m_fixtures; }
    QString devScreen() const { return m_devScreen; }
    bool devDark() const { return m_devDark; }
    bool devLight() const { return m_devLight; }
    qreal dp() const { return m_dp; }
    // Pixels in one physical centimetre.
    qreal cm() const { return QGuiApplication::primaryScreen()->physicalDotsPerInch() / 2.54; }
    QString appVersion() const { return QStringLiteral(APP_VERSION); }

    Q_INVOKABLE QString cacheDir() const
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(dir);
        return dir;
    }

    // Builds a raster MapLibre/Mapbox-GL style (as JSON text) from HERE's v3 tile API.
    // style: "explore" | "satellite". Returns an empty string when no key is set,
    // so QML can fall back to the key-free OpenFreeMap style instead.
    // Diagnostic only: same source/layer/background shape as hereStyleJson,
    // but pointed at a public, keyless tile source, to test whether our
    // setStyleJson() mechanism itself works independent of HERE specifics.
    Q_INVOKABLE QString debugOsmStyleJson() const
    {
        QJsonObject src;
        src[QStringLiteral("type")] = QStringLiteral("raster");
        src[QStringLiteral("tiles")] = QJsonArray{QStringLiteral("https://tile.openstreetmap.org/{z}/{x}/{y}.png")};
        src[QStringLiteral("tileSize")] = 256;

        QJsonObject layer;
        layer[QStringLiteral("id")] = QStringLiteral("osm");
        layer[QStringLiteral("type")] = QStringLiteral("raster");
        layer[QStringLiteral("source")] = QStringLiteral("osm");

        QJsonObject bgPaint;
        bgPaint[QStringLiteral("background-color")] = QStringLiteral("#f2f2f2");
        QJsonObject bgLayer;
        bgLayer[QStringLiteral("id")] = QStringLiteral("background");
        bgLayer[QStringLiteral("type")] = QStringLiteral("background");
        bgLayer[QStringLiteral("paint")] = bgPaint;

        QJsonObject sources;
        sources[QStringLiteral("osm")] = src;

        QJsonObject root;
        root[QStringLiteral("version")] = 8;
        root[QStringLiteral("sources")] = sources;
        root[QStringLiteral("layers")] = QJsonArray{bgLayer, layer};
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    Q_INVOKABLE QString hereStyleJson(const QString &style, bool traffic, bool night) const
    {
        const QString key = m_settings->hereApiKey();
        if (key.isEmpty())
            return QString();

        const bool satellite = style == QLatin1String("satellite");
        const QString styleId = satellite
                ? QStringLiteral("satellite.day")
                : night ? QStringLiteral("explore.night") : QStringLiteral("explore.day");

        QJsonObject baseSource;
        baseSource[QStringLiteral("type")] = QStringLiteral("raster");
        baseSource[QStringLiteral("tiles")] = QJsonArray{HereUrls::baseTiles(key, styleId, m_settings->lang())};
        baseSource[QStringLiteral("tileSize")] = 256;

        QJsonObject baseLayer;
        baseLayer[QStringLiteral("id")] = QStringLiteral("here-base");
        baseLayer[QStringLiteral("type")] = QStringLiteral("raster");
        baseLayer[QStringLiteral("source")] = QStringLiteral("here-base");

        QJsonObject backgroundPaint;
        backgroundPaint[QStringLiteral("background-color")] = night ? QStringLiteral("#1a1a1a") : QStringLiteral("#f2f2f2");
        QJsonObject backgroundLayer;
        backgroundLayer[QStringLiteral("id")] = QStringLiteral("background");
        backgroundLayer[QStringLiteral("type")] = QStringLiteral("background");
        backgroundLayer[QStringLiteral("paint")] = backgroundPaint;

        QJsonObject sources;
        sources[QStringLiteral("here-base")] = baseSource;
        QJsonArray layers{backgroundLayer, baseLayer};

        if (traffic && !satellite) {
            QJsonObject trafficSource;
            trafficSource[QStringLiteral("type")] = QStringLiteral("raster");
            trafficSource[QStringLiteral("tiles")] = QJsonArray{HereUrls::trafficTiles(key, night)};
            trafficSource[QStringLiteral("tileSize")] = 256;
            sources[QStringLiteral("here-traffic")] = trafficSource;

            QJsonObject trafficLayer;
            trafficLayer[QStringLiteral("id")] = QStringLiteral("here-traffic");
            trafficLayer[QStringLiteral("type")] = QStringLiteral("raster");
            trafficLayer[QStringLiteral("source")] = QStringLiteral("here-traffic");
            layers.append(trafficLayer);
        }

        QJsonObject root;
        root[QStringLiteral("version")] = 8;
        root[QStringLiteral("glyphs")] = QStringLiteral("http://fonts.openmaptiles.org/{fontstack}/{range}.pbf");
        root[QStringLiteral("sources")] = sources;
        root[QStringLiteral("layers")] = layers;
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

    // Driving-view style: OpenFreeMap's "liberty" vector style (it has
    // extruded 3D buildings), darkened for night, with HERE's traffic flow
    // overlaid when there is a key and traffic is on. The HERE key is only
    // ever put into the style here in C++. Result arrives via style3dReady;
    // an empty string means it could not be built (caller falls back).
    Q_INVOKABLE void request3dStyle(bool traffic, bool night)
    {
        if (!m_ofmStyle.isEmpty()) {
            emit style3dReady(build3dStyle(traffic, night));
            return;
        }
        QNetworkRequest req(QUrl(QStringLiteral("https://tiles.openfreemap.org/styles/liberty")));
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        QNetworkReply *reply = m_nam.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, traffic, night]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (doc.isObject())
                    m_ofmStyle = doc.object();
            }
            emit style3dReady(m_ofmStyle.isEmpty() ? QString() : build3dStyle(traffic, night));
        });
    }

    // Called by MapView once the map is idle, when --dev-grab=<path> was passed.
    Q_INVOKABLE void grabAndQuit(QQuickWindow *window, const QString &path)
    {
        if (!window || path.isEmpty())
            return;
        QTimer::singleShot(11000, window, [window, path]() {
            const QImage img = window->grabWindow();
            img.save(path);
            qApp->quit();
        });
    }

signals:
    void style3dReady(const QString &json);

private:
    QString build3dStyle(bool traffic, bool night) const
    {
        QJsonObject style = m_ofmStyle;
        QJsonArray layers = style.value(QStringLiteral("layers")).toArray();
        int firstExtrusion = layers.size();
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers.at(i).toObject();
            if (layer.value(QStringLiteral("type")).toString() == QLatin1String("fill-extrusion")
                    && firstExtrusion == layers.size())
                firstExtrusion = i;
            // Texture patterns (pedestrian areas, wetlands) read as visual
            // noise on a moving map: use a plain tint instead.
            QJsonObject plain = layer.value(QStringLiteral("paint")).toObject();
            if (plain.contains(QStringLiteral("fill-pattern"))) {
                plain.remove(QStringLiteral("fill-pattern"));
                plain[QStringLiteral("fill-color")] = layer.value(QStringLiteral("id")).toString().contains(QLatin1String("wetland"))
                        ? QStringLiteral("hsl(130,25%,82%)") : QStringLiteral("hsl(40,6%,84%)");
                plain[QStringLiteral("fill-opacity")] = 0.6;
                layer[QStringLiteral("paint")] = plain;
                layers[i] = layer;
            }
            if (night) {
                QJsonObject paint = layer.value(QStringLiteral("paint")).toObject();
                for (auto it = paint.begin(); it != paint.end(); ++it) {
                    if (it.key().endsWith(QLatin1String("color")))
                        it.value() = invertColors(it.value());
                }
                if (layer.value(QStringLiteral("type")).toString() == QLatin1String("fill-extrusion")) {
                    paint[QStringLiteral("fill-extrusion-color")] = QStringLiteral("hsl(220,12%,30%)");
                    paint[QStringLiteral("fill-extrusion-opacity")] = 0.92;
                }
                layer[QStringLiteral("paint")] = paint;
                layers[i] = layer;
            }
        }
        const QString key = m_settings->hereApiKey();
        if (traffic && !key.isEmpty()) {
            QJsonObject src;
            src[QStringLiteral("type")] = QStringLiteral("raster");
            src[QStringLiteral("tiles")] = QJsonArray{HereUrls::trafficTiles(key, night)};
            src[QStringLiteral("tileSize")] = 256;
            QJsonObject sources = style.value(QStringLiteral("sources")).toObject();
            sources[QStringLiteral("here-traffic")] = src;
            style[QStringLiteral("sources")] = sources;

            QJsonObject tl;
            tl[QStringLiteral("id")] = QStringLiteral("here-traffic");
            tl[QStringLiteral("type")] = QStringLiteral("raster");
            tl[QStringLiteral("source")] = QStringLiteral("here-traffic");
            QJsonArray out;
            for (int i = 0; i < layers.size(); ++i) {
                if (i == firstExtrusion)
                    out.append(tl);
                out.append(layers.at(i));
            }
            if (firstExtrusion == layers.size())
                out.append(tl);
            layers = out;
        }
        style[QStringLiteral("layers")] = layers;
        return QString::fromUtf8(QJsonDocument(style).toJson(QJsonDocument::Compact));
    }

    QNetworkAccessManager m_nam;
    QJsonObject m_ofmStyle;
    AppSettings *m_settings;
    bool m_fixtures;
    QString m_devScreen;
    bool m_devDark;
    bool m_devLight;
    qreal m_dp;
};

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

#include "main.moc"
