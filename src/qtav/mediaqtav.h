#ifndef MEDIAQTAV_H
#define MEDIAQTAV_H

#include "media.h"

#include <QtAV>
#ifndef MEDIA_AUDIOONLY
#include <QtAVWidgets>
#include <QtWidgets>
#endif

class MediaQtAV : public Media {
    Q_OBJECT

public:
    MediaQtAV(QObject *parent = nullptr);
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

private slots:
    void checkAboutToFinish(qint64 position);
    void onMediaStatusChange(QtAV::MediaStatus status);
    void onAVError(const QtAV::AVError &e);

private:
    QtAV::AVPlayer *createPlayer(bool audioOnly);
    void connectPlayer(QtAV::AVPlayer *player);
    void setCurrentPlayer(QtAV::AVPlayer *player);
    void smoothSourceChange(const QString &file, const QString &externalAudio);

    QtAV::AVPlayer *player1;
    QtAV::AVPlayer *player2;
    QtAV::AVPlayer *currentPlayer;

    QQueue<QString> queue;
    bool aboutToFinishEmitted = false;
    QString lastErrorString;

#ifndef MEDIA_AUDIOONLY
    QtAV::VideoRendererId rendererId;
#endif
    bool audioOnly = false;
};

#endif // MEDIAQTAV_H
