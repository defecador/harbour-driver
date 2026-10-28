#include "AppTools.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>

namespace {
QJniObject activityContext()
{
    return QJniObject(QNativeInterface::QAndroidApplication::context());
}
}

void AppTools::setKeepScreenOn(bool on)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on]() {
        QJniObject activity = activityContext();
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        if (!window.isValid())
            return;
        const jint flag = 0x00000080; // FLAG_KEEP_SCREEN_ON
        if (on)
            window.callMethod<void>("addFlags", "(I)V", flag);
        else
            window.callMethod<void>("clearFlags", "(I)V", flag);
    });
}

void AppTools::vibrate(int ms)
{
    QJniObject ctx = activityContext();
    const QJniObject name = QJniObject::fromString(QStringLiteral("vibrator"));
    QJniObject vib = ctx.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", name.object());
    if (!vib.isValid())
        return;
    QJniObject effect = QJniObject::callStaticObjectMethod("android/os/VibrationEffect", "createOneShot",
                                                           "(JI)Landroid/os/VibrationEffect;", jlong(ms), jint(-1));
    vib.callMethod<void>("vibrate", "(Landroid/os/VibrationEffect;)V", effect.object());
}

void AppTools::notify(const QString &title, const QString &body)
{
    QJniObject ctx = activityContext();
    const QJniObject name = QJniObject::fromString(QStringLiteral("notification"));
    QJniObject nm = ctx.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", name.object());
    if (!nm.isValid())
        return;
    const QJniObject channelId = QJniObject::fromString(QStringLiteral("driver_nav"));
    QJniObject channel("android/app/NotificationChannel", "(Ljava/lang/String;Ljava/lang/CharSequence;I)V",
                       channelId.object(), QJniObject::fromString(QStringLiteral("Navigation")).object(), jint(4));
    nm.callMethod<void>("createNotificationChannel", "(Landroid/app/NotificationChannel;)V", channel.object());
    QJniObject builder("android/app/Notification$Builder", "(Landroid/content/Context;Ljava/lang/String;)V",
                       ctx.object(), channelId.object());
    const jint icon = 17301651; // android.R.drawable.ic_dialog_info
    builder.callObjectMethod("setSmallIcon", "(I)Landroid/app/Notification$Builder;", icon);
    builder.callObjectMethod("setContentTitle", "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;",
                             QJniObject::fromString(title).object());
    builder.callObjectMethod("setContentText", "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;",
                             QJniObject::fromString(body).object());
    builder.callObjectMethod("setAutoCancel", "(Z)Landroid/app/Notification$Builder;", jboolean(true));
    QJniObject notification = builder.callObjectMethod("build", "()Landroid/app/Notification;");
    nm.callMethod<void>("notify", "(ILandroid/app/Notification;)V", jint(1), notification.object());
}

void AppTools::refreshInsets()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([this]() {
        QJniObject activity = activityContext();
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        QJniObject decor = window.callObjectMethod("getDecorView", "()Landroid/view/View;");
        QJniObject insets = decor.callObjectMethod("getRootWindowInsets", "()Landroid/view/WindowInsets;");
        if (!insets.isValid())
            return;
        const qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
        const qreal v[4] = {
            insets.callMethod<jint>("getSystemWindowInsetTop") / dpr,
            insets.callMethod<jint>("getSystemWindowInsetBottom") / dpr,
            insets.callMethod<jint>("getSystemWindowInsetLeft") / dpr,
            insets.callMethod<jint>("getSystemWindowInsetRight") / dpr,
        };
        bool changed = false;
        for (int i = 0; i < 4; ++i) {
            if (!qFuzzyCompare(1 + m_insets[i], 1 + v[i])) {
                m_insets[i] = v[i];
                changed = true;
            }
        }
        if (changed)
            emit insetsChanged();
    });
}
#else
void AppTools::setKeepScreenOn(bool) {}
void AppTools::vibrate(int) {}
void AppTools::notify(const QString &, const QString &) {}
void AppTools::refreshInsets() {}
#endif
