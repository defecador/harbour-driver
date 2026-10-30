#include "VoiceClips.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QPermissions>
#include <QtCore/qnativeinterface.h>
#endif

namespace {
const int kMaxClipMs = 5000;
const qint64 kMinClipBytes = 1024;
}

struct VoiceClips::Private {
#ifdef Q_OS_ANDROID
    QJniObject recorder;
    QJniObject player;
    QJniObject focusRequest;
    QJniObject audioManager;
#endif
};

VoiceClips::VoiceClips(const QString &dir, QObject *parent)
    : QObject(parent), d(new Private), m_dir(dir)
{
}

VoiceClips::~VoiceClips()
{
    stopPlayback();
    delete d;
}

bool VoiceClips::available() const
{
#ifdef Q_OS_ANDROID
    return !m_dir.isEmpty();
#else
    return false;
#endif
}

bool VoiceClips::validKind(const QString &kind)
{
    return kind == QLatin1String("stop") || kind == QLatin1String("giveWay") || kind == QLatin1String("camera");
}

QString VoiceClips::pathFor(const QString &kind) const
{
    if (m_dir.isEmpty() || !validKind(kind))
        return QString();
    return m_dir + QLatin1Char('/') + kind + QStringLiteral(".m4a");
}

bool VoiceClips::has(const QString &kind) const
{
    const QString p = pathFor(kind);
    return !p.isEmpty() && QFileInfo(p).size() >= kMinClipBytes;
}

void VoiceClips::remove(const QString &kind)
{
    const QString p = pathFor(kind);
    if (p.isEmpty() || !QFile::exists(p))
        return;
    QFile::remove(p);
    ++m_revision;
    emit changed();
}

#ifdef Q_OS_ANDROID

namespace {
QJniObject activityContext()
{
    return QJniObject(QNativeInterface::QAndroidApplication::context());
}
}

void VoiceClips::startRecording(const QString &kind)
{
    if (!available() || !validKind(kind) || !m_recording.isEmpty())
        return;
    QMicrophonePermission mic;
    switch (qApp->checkPermission(mic)) {
    case Qt::PermissionStatus::Granted:
        beginRecording(kind);
        break;
    case Qt::PermissionStatus::Undetermined:
        qApp->requestPermission(mic, this, [this, kind](const QPermission &p) {
            if (p.status() == Qt::PermissionStatus::Granted)
                beginRecording(kind);
            else
                emit error(tr("Microphone access was denied."));
        });
        break;
    default:
        emit error(tr("Microphone access is off. Turn it on for Driver in the system settings."));
        break;
    }
}

void VoiceClips::beginRecording(const QString &kind)
{
    QDir().mkpath(m_dir);
    const QString tmp = pathFor(kind) + QStringLiteral(".rec");
    QFile::remove(tmp);

    d->recorder = QJniObject("android/media/MediaRecorder");
    if (!d->recorder.isValid()) {
        emit error(tr("Couldn't start recording."));
        return;
    }
    d->recorder.callMethod<void>("setAudioSource", "(I)V", jint(1));         // MIC
    d->recorder.callMethod<void>("setOutputFormat", "(I)V", jint(2));        // MPEG_4
    d->recorder.callMethod<void>("setAudioEncoder", "(I)V", jint(3));        // AAC
    d->recorder.callMethod<void>("setAudioChannels", "(I)V", jint(1));
    d->recorder.callMethod<void>("setAudioSamplingRate", "(I)V", jint(44100));
    d->recorder.callMethod<void>("setAudioEncodingBitRate", "(I)V", jint(64000));
    d->recorder.callMethod<void>("setMaxDuration", "(I)V", jint(kMaxClipMs));
    d->recorder.callMethod<void>("setOutputFile", "(Ljava/lang/String;)V",
                                 QJniObject::fromString(tmp).object<jstring>());
    d->recorder.callMethod<void>("prepare", "()V");
    d->recorder.callMethod<void>("start", "()V");

    m_recording = kind;
    emit recordingChanged();
    const int gen = ++m_recGen;
    QTimer::singleShot(kMaxClipMs + 200, this, [this, gen]() {
        if (gen == m_recGen)
            stopRecording();
    });
}

void VoiceClips::stopRecording()
{
    if (m_recording.isEmpty())
        return;
    const QString kind = m_recording;
    ++m_recGen;
    if (d->recorder.isValid()) {
        d->recorder.callMethod<void>("stop", "()V");      // throws if nothing was recorded; cleared by Qt
        d->recorder.callMethod<void>("release", "()V");
        d->recorder = QJniObject();
    }
    m_recording.clear();
    emit recordingChanged();

    const QString tmp = pathFor(kind) + QStringLiteral(".rec");
    if (QFileInfo(tmp).size() >= kMinClipBytes) {
        // A failed take must never destroy the previous clip, hence the temp file.
        QFile::remove(pathFor(kind));
        QFile::rename(tmp, pathFor(kind));
        ++m_revision;
        emit changed();
    } else {
        QFile::remove(tmp);
        emit error(tr("Nothing was recorded."));
    }
}

bool VoiceClips::play(const QString &kind)
{
    if (!has(kind) || !m_recording.isEmpty())
        return false;
    stopPlayback();

    QJniObject ctx = activityContext();
    d->audioManager = ctx.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                           QJniObject::fromString(QStringLiteral("audio")).object<jstring>());

    // Navigation-guidance speech, with transient focus that lets other apps
    // keep playing at a lower volume.
    QJniObject attrBuilder("android/media/AudioAttributes$Builder");
    attrBuilder.callObjectMethod("setUsage", "(I)Landroid/media/AudioAttributes$Builder;", jint(12));
    attrBuilder.callObjectMethod("setContentType", "(I)Landroid/media/AudioAttributes$Builder;", jint(1));
    QJniObject attrs = attrBuilder.callObjectMethod("build", "()Landroid/media/AudioAttributes;");

    if (d->audioManager.isValid()) {
        QJniObject focusBuilder("android/media/AudioFocusRequest$Builder", "(I)V", jint(3));
        focusBuilder.callObjectMethod("setAudioAttributes",
                                      "(Landroid/media/AudioAttributes;)Landroid/media/AudioFocusRequest$Builder;",
                                      attrs.object());
        d->focusRequest = focusBuilder.callObjectMethod("build", "()Landroid/media/AudioFocusRequest;");
        d->audioManager.callMethod<jint>("requestAudioFocus", "(Landroid/media/AudioFocusRequest;)I",
                                         d->focusRequest.object());
    }

    d->player = QJniObject("android/media/MediaPlayer");
    if (!d->player.isValid())
        return false;
    d->player.callMethod<void>("setAudioAttributes", "(Landroid/media/AudioAttributes;)V", attrs.object());
    d->player.callMethod<void>("setDataSource", "(Ljava/lang/String;)V",
                               QJniObject::fromString(pathFor(kind)).object<jstring>());
    d->player.callMethod<void>("prepare", "()V");
    d->player.callMethod<void>("start", "()V");
    const int duration = d->player.callMethod<jint>("getDuration", "()I");

    const int gen = ++m_playGen;
    QTimer::singleShot(qBound(500, duration, 8000) + 300, this, [this, gen]() {
        if (gen == m_playGen)
            stopPlayback();
    });
    return true;
}

void VoiceClips::stopPlayback()
{
    ++m_playGen;
    if (d->player.isValid()) {
        d->player.callMethod<void>("stop", "()V");
        d->player.callMethod<void>("release", "()V");
        d->player = QJniObject();
    }
    if (d->audioManager.isValid() && d->focusRequest.isValid()) {
        d->audioManager.callMethod<jint>("abandonAudioFocusRequest", "(Landroid/media/AudioFocusRequest;)I",
                                         d->focusRequest.object());
    }
    d->focusRequest = QJniObject();
    d->audioManager = QJniObject();
}

#else   // not Android: recording and playback are not available

void VoiceClips::startRecording(const QString &) {}
void VoiceClips::beginRecording(const QString &) {}
void VoiceClips::stopRecording() {}
bool VoiceClips::play(const QString &) { return false; }
void VoiceClips::stopPlayback() {}

#endif
