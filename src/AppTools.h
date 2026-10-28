#ifndef APPTOOLS_H
#define APPTOOLS_H

#include <QColor>
#include <QCryptographicHash>
#include <QFile>
#include <QUrl>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>

#include "AppSettings.h"
#include "HereUrls.h"

#ifndef APP_VERSION
#define APP_VERSION "0.8.3"
#endif

// ---- 3D (vector) driving style helpers -------------------------------
// Flips the lightness of one CSS-style colour string (hsl/hsla/rgb/rgba/#hex)
// so a light map style becomes a dark one; any other string is returned as is.
static inline QString invertColorString(const QString &in)
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

static inline QJsonValue invertColors(const QJsonValue &v)
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
    Q_PROPERTY(bool android READ isAndroid CONSTANT)
    Q_PROPERTY(qreal insetTop READ insetTop NOTIFY insetsChanged)
    Q_PROPERTY(qreal insetBottom READ insetBottom NOTIFY insetsChanged)
    Q_PROPERTY(qreal insetLeft READ insetLeft NOTIFY insetsChanged)
    Q_PROPERTY(qreal insetRight READ insetRight NOTIFY insetsChanged)

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
#ifdef Q_OS_ANDROID
    bool isAndroid() const { return true; }
#else
    bool isAndroid() const { return false; }
#endif
    // System-bar / cut-out insets in logical pixels (always 0 off Android).
    qreal insetTop() const { return m_insets[0]; }
    qreal insetBottom() const { return m_insets[1]; }
    qreal insetLeft() const { return m_insets[2]; }
    qreal insetRight() const { return m_insets[3]; }

    // Platform services (AppTools.cpp). Android-only; no-ops elsewhere, where
    // QML talks to Nemo directly.
    Q_INVOKABLE void setKeepScreenOn(bool on);
    Q_INVOKABLE void vibrate(int ms);
    Q_INVOKABLE void notify(const QString &title, const QString &body);
    Q_INVOKABLE void refreshInsets();

    Q_INVOKABLE QString cacheDir() const
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(dir);
        return dir;
    }

    // Writes a style document into the cache dir and returns its file:// URL
    // (MapLibre's QtLocation plugin only loads styles by URL). Named by content
    // so a changed style never hits a stale copy; older copies are removed.
    Q_INVOKABLE QString writeStyleFile(const QString &json) const
    {
        const QString dir = cacheDir();
        const QString name = QStringLiteral("style-%1.json")
                .arg(QString::fromLatin1(QCryptographicHash::hash(json.toUtf8(), QCryptographicHash::Md5).toHex()));
        const QStringList old = QDir(dir).entryList({QStringLiteral("style-*.json")}, QDir::Files);
        for (const QString &f : old) {
            if (f != name)
                QFile::remove(dir + QLatin1Char('/') + f);
        }
        QFile f(dir + QLatin1Char('/') + name);
        if (!f.exists() && f.open(QIODevice::WriteOnly)) {
            f.write(json.toUtf8());
            f.close();
        }
        return QUrl::fromLocalFile(f.fileName()).toString();
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
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
    void insetsChanged();

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
    qreal m_insets[4] = {0, 0, 0, 0}; // top, bottom, left, right
};

#endif // APPTOOLS_H
