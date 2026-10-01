#include "AppSettings.h"

#include <QLocale>
#include <QStandardPaths>

// Stored under ~/.config/org.matka/Matka/ which the Sailjail sandbox allows.
AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_s(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/settings.conf"),
          QSettings::IniFormat)
{
}

QString AppSettings::hereApiKey() const { return m_s.value(QStringLiteral("here/apiKey")).toString().trimmed(); }
void AppSettings::setHereApiKey(const QString &key)
{
    if (key.trimmed() == hereApiKey()) return;
    m_s.setValue(QStringLiteral("here/apiKey"), key.trimmed());
    m_s.sync();
    emit hereApiKeyChanged();
}

QString AppSettings::mapStyle() const { return m_s.value(QStringLiteral("map/style"), QStringLiteral("explore")).toString(); }
void AppSettings::setMapStyle(const QString &s)
{
    if (s == mapStyle()) return;
    m_s.setValue(QStringLiteral("map/style"), s);
    m_s.sync();
    emit mapStyleChanged();
}

bool AppSettings::traffic() const { return m_s.value(QStringLiteral("map/traffic"), true).toBool(); }
void AppSettings::setTraffic(bool on)
{
    if (on == traffic()) return;
    m_s.setValue(QStringLiteral("map/traffic"), on);
    m_s.sync();
    emit trafficChanged();
}

bool AppSettings::vibration() const { return m_s.value(QStringLiteral("nav/vibration"), false).toBool(); }
void AppSettings::setVibration(bool on)
{
    if (on == vibration()) return;
    m_s.setValue(QStringLiteral("nav/vibration"), on);
    m_s.sync();
    emit vibrationChanged();
}

bool AppSettings::view3d() const { return m_s.value(QStringLiteral("map/view3d"), true).toBool(); }
void AppSettings::setView3d(bool on)
{
    if (on == view3d()) return;
    m_s.setValue(QStringLiteral("map/view3d"), on);
    m_s.sync();
    emit view3dChanged();
}

QString AppSettings::theme() const { return m_s.value(QStringLiteral("ui/theme"), QStringLiteral("auto")).toString(); }
void AppSettings::setTheme(const QString &t)
{
    if (t == theme()) return;
    m_s.setValue(QStringLiteral("ui/theme"), t);
    m_s.sync();
    emit themeChanged();
}

QString AppSettings::units() const
{
    const QString def = QLocale::system().measurementSystem() == QLocale::MetricSystem
            ? QStringLiteral("metric") : QStringLiteral("imperial");
    return m_s.value(QStringLiteral("ui/units"), def).toString();
}
void AppSettings::setUnits(const QString &u)
{
    if (u == units()) return;
    m_s.setValue(QStringLiteral("ui/units"), u);
    m_s.sync();
    emit unitsChanged();
}

QVariantMap AppSettings::home() const { return m_s.value(QStringLiteral("places/home")).toMap(); }
void AppSettings::setHome(const QVariantMap &p) { m_s.setValue(QStringLiteral("places/home"), p); m_s.sync(); emit homeChanged(); }
QVariantMap AppSettings::work() const { return m_s.value(QStringLiteral("places/work")).toMap(); }
void AppSettings::setWork(const QVariantMap &p) { m_s.setValue(QStringLiteral("places/work"), p); m_s.sync(); emit workChanged(); }

QVariantList AppSettings::recents() const { return m_s.value(QStringLiteral("places/recents")).toList(); }

void AppSettings::addRecent(const QVariantMap &place)
{
    QVariantList list = recents();
    const QString title = place.value(QStringLiteral("title")).toString();
    for (int i = list.size() - 1; i >= 0; --i)
        if (list.at(i).toMap().value(QStringLiteral("title")).toString() == title)
            list.removeAt(i);
    list.prepend(place);
    while (list.size() > 10)
        list.removeLast();
    m_s.setValue(QStringLiteral("places/recents"), list);
    m_s.sync();
    emit recentsChanged();
}

void AppSettings::clearRecents()
{
    m_s.remove(QStringLiteral("places/recents"));
    m_s.sync();
    emit recentsChanged();
}

bool AppSettings::termsAccepted() const { return m_s.value(QStringLiteral("here/termsAccepted"), false).toBool(); }
void AppSettings::setTermsAccepted(bool a)
{
    m_s.setValue(QStringLiteral("here/termsAccepted"), a);
    m_s.sync();
    emit termsAcceptedChanged();
}

QString AppSettings::lang() const
{
    // HERE's tile API requires lang to match ^[a-z]{2}(-[A-Za-z]{1,4})?$ exactly.
    // QLocale::system().name() can return locales with a script subtag
    // (e.g. "zh_Hans_CN"), which has too many parts to match that pattern,
    // so only the language and the first following subtag are kept.
    QString l = QLocale::system().name();
    l.replace(QLatin1Char('_'), QLatin1Char('-'));
    const QStringList parts = l.split(QLatin1Char('-'), QString::SkipEmptyParts);
    const QString language = parts.value(0).toLower();
    if (language.length() != 2)
        return QStringLiteral("en-GB");
    if (parts.size() > 1 && parts.at(1).length() >= 1 && parts.at(1).length() <= 4)
        return language + QLatin1Char('-') + parts.at(1);
    return language;
}
