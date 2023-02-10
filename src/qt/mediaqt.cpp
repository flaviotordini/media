#include "mediaqt.h"

MediaQt::MediaQt(QObject *parent) : Media(parent) {
    player = new QMediaPlayer(this);
}

#ifndef MEDIA_AUDIOONLY

void MediaQt::setRenderer(const QString &name) {
    // Not implemented
}

QWidget *MediaQt::videoWidget() {
    return widget;
}

void MediaQt::playSeparateAudioAndVideo(const QString &video, const QString &audio) {
    // Not implemented
    qWarning() << "Not supported by Qt";
}

void MediaQt::snapshot() {
    // Not implemented
}

#endif

void MediaQt::setAudioOnly(bool value) {
    audioOnly = value;
}

void MediaQt::init() {
#ifndef MEDIA_AUDIOONLY
    widget = new QVideoWidget;
    player->setVideoOutput(widget);
#endif

    const auto ao = new QAudioOutput(this);
    connect(ao, &QAudioOutput::volumeChanged, this, &Media::volumeChanged);
    connect(ao, &QAudioOutput::mutedChanged, this, &Media::volumeMutedChanged);
    player->setAudioOutput(ao);

    connect(player, &QMediaPlayer::sourceChanged, this, &Media::sourceChanged);
    connect(player, &QMediaPlayer::bufferProgressChanged, this, &Media::bufferStatus);
    connect(player, &QMediaPlayer::positionChanged, this, &Media::positionChanged);
    connect(player, &QMediaPlayer::positionChanged, this, [this](auto position) {
        if (!aboutToFinishEmitted && player->playbackState() == QMediaPlayer::PlayingState &&
            duration() - position < 5000) {
            aboutToFinishEmitted = true;
            emit aboutToFinish();
        }
    });

    connect(player, &QMediaPlayer::playbackStateChanged, this, [this](auto playbackState) {
        qDebug() << playbackState;
        if (playbackState == QMediaPlayer::PlaybackState::PlayingState) {
            emit started();
            emitStateChanged(Media::PlayingState);
        } else if (playbackState == QMediaPlayer::PlaybackState::PausedState) {
            emit paused(true);
            emitStateChanged(Media::PausedState);
        } else if (playbackState == QMediaPlayer::PlaybackState::StoppedState) {
            emit stopped();
            emitStateChanged(Media::StoppedState);
        }
    });

    connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](auto status) {
        qDebug() << status;
        if (status == QMediaPlayer::MediaStatus::BufferingMedia) {
            emitStateChanged(Media::State::BufferingState);
        } else if (status == QMediaPlayer::MediaStatus::LoadingMedia) {
            emitStateChanged(Media::State::LoadingState);
        } else if (status == QMediaPlayer::MediaStatus::EndOfMedia) {
            emit finished();
            if (!queue.isEmpty()) {
                play(queue.dequeue());
            }
        }
    });

    connect(player, &QMediaPlayer::errorOccurred, this,
            [this] { emitStateChanged(Media::State::ErrorState); });
}

void MediaQt::emitStateChanged(State state) {
    currentState = state;
    emit stateChanged(state);
}

Media::State MediaQt::state() const {
    return currentState;
}

void MediaQt::play(const QString &file) {
    aboutToFinishEmitted = false;
    player->setSource(file);
    player->play();
}

void MediaQt::play() {
    player->play();
}

void MediaQt::pause() {
    player->pause();
}

void MediaQt::stop() {
    player->stop();
}

void MediaQt::seek(qint64 ms) {
    player->setPosition(ms);
}

void MediaQt::relativeSeek(qint64 ms) {
    player->setPosition(player->position() + ms);
}

QString MediaQt::file() const {
    return player->source().toString();
}

void MediaQt::setBufferMilliseconds(qint64 value) {
    Q_UNUSED(value)
}

void MediaQt::setUserAgent(const QString &value) {
    Q_UNUSED(value)
}

void MediaQt::enqueue(const QString &file) {
    queue << file;
}

void MediaQt::clearQueue() {
    queue.clear();
}

bool MediaQt::hasQueue() const {
    return !queue.isEmpty();
}

qint64 MediaQt::position() const {
    return player->position();
}

qint64 MediaQt::duration() const {
    return player->duration();
}

qint64 MediaQt::remainingTime() const {
    return player->duration() - player->position();
}

qreal MediaQt::volume() const {
    return player->audioOutput()->volume();
}

void MediaQt::setVolume(qreal value) {
    player->audioOutput()->setVolume(value);
}

bool MediaQt::volumeMuted() const {
    return player->audioOutput()->isMuted();
}

void MediaQt::setVolumeMuted(bool value) {
    player->audioOutput()->setMuted(value);
}

QString MediaQt::errorString() const {
    return player->errorString();
}
