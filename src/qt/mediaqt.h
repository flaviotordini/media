#ifndef MEDIAQT_H
#define MEDIAQT_H

#include "media.h"

#include <QtMultimedia>
#ifndef MEDIA_AUDIOONLY
#include <QVideoWidget>
#endif

class MediaQt : public Media {
    Q_OBJECT

public:
    MediaQt(QObject *parent = nullptr);

    // Media interface
#ifndef MEDIA_AUDIOONLY
    void setRenderer(const QString &name);
    QWidget *videoWidget();
    void playSeparateAudioAndVideo(const QString &video, const QString &audio);
    void snapshot();
#endif
    void setAudioOnly(bool value);
    void init();
    Media::State state() const;
    void play(const QString &file);
    void play();
    void pause();
    void stop();
    void seek(qint64 ms);
    void relativeSeek(qint64 ms);
    QString file() const;
    void setBufferMilliseconds(qint64 value);
    void setUserAgent(const QString &value);
    void enqueue(const QString &file);
    void clearQueue();
    bool hasQueue() const;
    qint64 position() const;
    qint64 duration() const;
    qint64 remainingTime() const;
    qreal volume() const;
    void setVolume(qreal value);
    bool volumeMuted() const;
    void setVolumeMuted(bool value);
    QString errorString() const;

private:
    void emitStateChanged(Media::State state);

#ifndef MEDIA_AUDIOONLY
    QVideoWidget *widget;
#endif
    bool audioOnly = false;
    QMediaPlayer *player;
    Media::State currentState = StoppedState;
    QQueue<QString> queue;
    bool aboutToFinishEmitted = false;
};

#endif // MEDIAQT_H
