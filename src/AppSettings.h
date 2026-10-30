#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QObject>
#include <QSettings>
#include <QVariantMap>

class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString hereApiKey READ hereApiKey WRITE setHereApiKey NOTIFY hereApiKeyChanged)
    Q_PROPERTY(bool hasHereKey READ hasHereKey NOTIFY hereApiKeyChanged)
    Q_PROPERTY(QString mapStyle READ mapStyle WRITE setMapStyle NOTIFY mapStyleChanged)
    Q_PROPERTY(bool traffic READ traffic WRITE setTraffic NOTIFY trafficChanged)
    Q_PROPERTY(bool vibration READ vibration WRITE setVibration NOTIFY vibrationChanged)
    Q_PROPERTY(bool warnSigns READ warnSigns WRITE setWarnSigns NOTIFY warnSignsChanged)
    Q_PROPERTY(bool speedLimitSign READ speedLimitSign WRITE setSpeedLimitSign NOTIFY speedLimitSignChanged)
    Q_PROPERTY(bool speedCameras READ speedCameras WRITE setSpeedCameras NOTIFY speedCamerasChanged)
    Q_PROPERTY(QString uiStyle READ uiStyle WRITE setUiStyle NOTIFY uiStyleChanged)
    Q_PROPERTY(bool view3d READ view3d WRITE setView3d NOTIFY view3dChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString units READ units WRITE setUnits NOTIFY unitsChanged)
    Q_PROPERTY(QVariantMap home READ home WRITE setHome NOTIFY homeChanged)
    Q_PROPERTY(QVariantMap work READ work WRITE setWork NOTIFY workChanged)
    Q_PROPERTY(QVariantList recents READ recents NOTIFY recentsChanged)
    // Places the user saved with "Add place", newest last.
    Q_PROPERTY(QVariantList savedPlaces READ savedPlaces NOTIFY savedPlacesChanged)
    Q_PROPERTY(bool termsAccepted READ termsAccepted WRITE setTermsAccepted NOTIFY termsAcceptedChanged)
    Q_PROPERTY(QString lang READ lang CONSTANT)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString hereApiKey() const;
    void setHereApiKey(const QString &key);
    bool hasHereKey() const { return !hereApiKey().isEmpty(); }
    QString mapStyle() const;
    void setMapStyle(const QString &s);
    bool traffic() const;
    void setTraffic(bool on);
    bool vibration() const;
    void setVibration(bool on);
    bool warnSigns() const;
    void setWarnSigns(bool on);
    bool speedLimitSign() const;
    void setSpeedLimitSign(bool on);
    bool speedCameras() const;
    void setSpeedCameras(bool on);
    QString uiStyle() const;
    void setUiStyle(const QString &s);
    bool view3d() const;
    void setView3d(bool on);
    QString theme() const;
    void setTheme(const QString &t);
    QString units() const;
    void setUnits(const QString &u);
    QVariantMap home() const;
    void setHome(const QVariantMap &p);
    QVariantMap work() const;
    void setWork(const QVariantMap &p);
    QVariantList recents() const;
    QVariantList savedPlaces() const;
    bool termsAccepted() const;
    void setTermsAccepted(bool a);
    QString lang() const;

    Q_INVOKABLE void addRecent(const QVariantMap &place);
    Q_INVOKABLE void clearRecents();
    Q_INVOKABLE void addSavedPlace(const QVariantMap &place);
    Q_INVOKABLE void removeSavedPlace(int index);

signals:
    void hereApiKeyChanged();
    void mapStyleChanged();
    void trafficChanged();
    void view3dChanged();
    void uiStyleChanged();
    void vibrationChanged();
    void warnSignsChanged();
    void speedCamerasChanged();
    void speedLimitSignChanged();
    void themeChanged();
    void unitsChanged();
    void homeChanged();
    void workChanged();
    void recentsChanged();
    void savedPlacesChanged();
    void termsAcceptedChanged();

private:
    QSettings m_s;
};

#endif
