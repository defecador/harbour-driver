#ifndef VOICECLIPS_H
#define VOICECLIPS_H

#include <QObject>
#include <QString>

// The user's own recorded voice warnings ("stop sign", "give way", "speed
// camera"), one short clip per kind. Recording and playback are Android only
// (MediaRecorder / MediaPlayer through JNI); elsewhere `available` is false
// and everything is a no-op. Playback asks for transient audio focus so music
// from other apps (Spotify) is lowered while the clip plays.
class VoiceClips : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    // The kind currently being recorded, or "".
    Q_PROPERTY(QString recording READ recording NOTIFY recordingChanged)
    // Bumped whenever a clip is added or removed, so QML bindings on has() update.
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    explicit VoiceClips(const QString &dir = QString(), QObject *parent = nullptr);
    ~VoiceClips() override;

    bool available() const;
    QString recording() const { return m_recording; }
    int revision() const { return m_revision; }

    // kind is one of "stop", "giveWay", "camera".
    Q_INVOKABLE bool has(const QString &kind) const;
    Q_INVOKABLE void startRecording(const QString &kind);
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE bool play(const QString &kind);
    Q_INVOKABLE void remove(const QString &kind);

    static bool validKind(const QString &kind);

signals:
    void changed();
    void recordingChanged();
    void error(const QString &message);

private:
    QString pathFor(const QString &kind) const;
    void beginRecording(const QString &kind);
    void stopPlayback();

    struct Private;
    Private *d;
    QString m_dir;
    QString m_recording;
    int m_revision = 0;
    int m_recGen = 0;
    int m_playGen = 0;
};

#endif
