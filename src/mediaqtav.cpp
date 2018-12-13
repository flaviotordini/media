#include "mediaqtav.h"

namespace {

#ifndef MEDIA_AUDIOONLY
QtAV::VideoRendererId rendererIdFor(const QString &name) {
    static const struct {
        const char *name;
        QtAV::VideoRendererId id;
    } renderers[] = {{"opengl", QtAV::VideoRendererId_OpenGLWidget},
                     {"gl", QtAV::VideoRendererId_GLWidget2},
                     {"d2d", QtAV::VideoRendererId_Direct2D},
                     {"gdi", QtAV::VideoRendererId_GDI},
                     {"xv", QtAV::VideoRendererId_XV},
                     {"x11", QtAV::VideoRendererId_X11},
                     {"qt", QtAV::VideoRendererId_Widget}};

    for (int i = 0; renderers[i].name; ++i) {
        if (name == QLatin1String(renderers[i].name)) return renderers[i].id;
    }
#ifndef QT_NO_OPENGL
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return QtAV::VideoRendererId_OpenGLWidget;
#endif
    return QtAV::VideoRendererId_GLWidget2;
#endif
}
#endif

Media::State stateFor(QtAV::AVPlayer::State state) {
    static const QMap<QtAV::AVPlayer::State, Media::State> map{
            {QtAV::AVPlayer::StoppedState, Media::StoppedState},
            {QtAV::AVPlayer::PlayingState, Media::PlayingState},
            {QtAV::AVPlayer::PausedState, Media::PausedState}};
    return map[state];
}

} // namespace

MediaQtAV::MediaQtAV(QObject *parent)
    : Media(parent), player1(nullptr), player2(nullptr), currentPlayer(nullptr) {}

#ifndef MEDIA_AUDIOONLY
void MediaQtAV::setRenderer(const QString &name) {
    rendererId = rendererIdFor(name);
}
#endif

void MediaQtAV::setAudioOnly(bool value) {
    audioOnly = value;
}

void MediaQtAV::init() {
    player1 = createPlayer(audioOnly);
    setCurrentPlayer(player1);
}

#ifndef MEDIA_AUDIOONLY
QWidget *MediaQtAV::videoWidget() const {
    return currentPlayer->renderer()->widget();
}
#endif

Media::State MediaQtAV::state() const {
    if (currentPlayer->mediaStatus() == QtAV::LoadingMedia) return LoadingState;
    if (currentPlayer->bufferProgress() < 1.) return BufferingState;
    return stateFor(currentPlayer->state());
}

void MediaQtAV::play(const QString &file) {
    currentPlayer->play(file);
    aboutToFinishEmitted = false;
    lastErrorString.clear();
    clearQueue();
}

#ifndef MEDIA_AUDIOONLY
void MediaQtAV::playSeparateAudioAndVideo(const QString &video, const QString &audio) {
    currentPlayer->play(video);
    currentPlayer->setExternalAudio(audio);
    aboutToFinishEmitted = false;
    lastErrorString.clear();
    clearQueue();
}
#endif

void MediaQtAV::play() {
    if (currentPlayer->isPaused())
        currentPlayer->togglePause();
    else
        currentPlayer->play();
}

QString MediaQtAV::file() const {
    return currentPlayer->file();
}

void MediaQtAV::setBufferMilliseconds(qint64 value) {
    currentPlayer->setBufferValue(value);
}

void MediaQtAV::enqueue(const QString &file) {
    queue << file;
    if (queue.size() == 1) {
        qDebug() << "Preloading" << file;
        auto nextPlayer = player1;
        if (currentPlayer == player1) {
            if (player2 == nullptr) player2 = createPlayer(audioOnly);
            nextPlayer = player2;
        }
        nextPlayer->setFile(file);
        nextPlayer->load();
    }
}

void MediaQtAV::clearQueue() {
    queue.clear();
}

bool MediaQtAV::hasQueue() const {
    return !queue.isEmpty();
}

qint64 MediaQtAV::position() const {
    return currentPlayer->position();
}

qint64 MediaQtAV::duration() const {
    return currentPlayer->duration();
}

qint64 MediaQtAV::remainingTime() const {
    return currentPlayer->duration() - currentPlayer->position();
}

qreal MediaQtAV::volume() const {
    return currentPlayer->audio()->volume();
}

void MediaQtAV::setVolume(qreal value) {
    auto audio = currentPlayer->audio();
    if (!audio->isOpen()) audio->open();
    audio->setVolume(value);
}

bool MediaQtAV::volumeMuted() const {
    return currentPlayer->audio()->isMute();
}

void MediaQtAV::setVolumeMuted(bool value) {
    currentPlayer->audio()->setMute(value);
}

QString MediaQtAV::errorString() const {
    return lastErrorString;
}

void MediaQtAV::checkAboutToFinish(qint64 position) {
    if (!aboutToFinishEmitted && currentPlayer->isPlaying() &&
        duration() - position < currentPlayer->bufferValue()) {
        aboutToFinishEmitted = true;
        emit aboutToFinish();
    }
}

void MediaQtAV::onMediaStatusChange(QtAV::MediaStatus status) {
    qDebug() << QVariant(status).toString();

    switch (status) {
    case QtAV::LoadingMedia:
        emit stateChanged(LoadingState);
        break;
    case QtAV::EndOfMedia:
        if (queue.isEmpty())
            emit finished();
        else {
            auto nextPlayer = currentPlayer == player1 ? player2 : player1;
            if (nextPlayer->isLoaded()) {
                nextPlayer->play();
                setCurrentPlayer(nextPlayer);
                aboutToFinishEmitted = false;
                lastErrorString.clear();
                emit sourceChanged();
                queue.dequeue();
            } else {
                qDebug() << "Not preloaded";
                currentPlayer->play(queue.dequeue());
            }
        }
        break;
    default:
        qDebug() << "Unhandled" << QVariant(status).toString();
    }
}

void MediaQtAV::onAVError(const QtAV::AVError &e) {
    lastErrorString = e.string();
    emit error(lastErrorString);
}

void MediaQtAV::pause() {
    currentPlayer->pause();
}

void MediaQtAV::stop() {
    currentPlayer->stop();
}

void MediaQtAV::seek(qint64 ms) {
    currentPlayer->setPosition(ms);
}

QtAV::AVPlayer *MediaQtAV::createPlayer(bool audioOnly) {
    QtAV::AVPlayer *p = new QtAV::AVPlayer(this);

#ifndef MEDIA_AUDIOONLY
    if (!audioOnly) {
        QtAV::VideoRenderer *renderer = QtAV::VideoRenderer::create(rendererId);
        if (!renderer || !renderer->isAvailable() || !renderer->widget()) {
            qFatal("No QtAV video renderer");
        }
        p->setRenderer(renderer);
    }
#endif

    p->setBufferMode(QtAV::BufferTime);
    if (currentPlayer) p->setBufferValue(currentPlayer->bufferValue());

    return p;
}

void MediaQtAV::connectPlayer(QtAV::AVPlayer *player) {
    connect(player, &QtAV::AVPlayer::error, this, &MediaQtAV::onAVError);

    connect(player, &QtAV::AVPlayer::sourceChanged, this, &Media::sourceChanged);
    connect(player, &QtAV::AVPlayer::sourceChanged, this, [this] { aboutToFinishEmitted = false; });

    connect(player, &QtAV::AVPlayer::bufferProgressChanged, this, &Media::bufferStatus);
    connect(player, &QtAV::AVPlayer::started, this, &Media::started);
    connect(player, &QtAV::AVPlayer::stopped, this, &Media::stopped);
    connect(player, &QtAV::AVPlayer::paused, this, &Media::paused);

    connect(player, &QtAV::AVPlayer::positionChanged, this, &Media::positionChanged);
    connect(player, &QtAV::AVPlayer::positionChanged, this, &MediaQtAV::checkAboutToFinish);

    connect(player, &QtAV::AVPlayer::stateChanged, this,
            [this](QtAV::AVPlayer::State state) { emit stateChanged(stateFor(state)); });
    connect(player, &QtAV::AVPlayer::mediaStatusChanged, this, &MediaQtAV::onMediaStatusChange);

    connect(player->audio(), &QtAV::AudioOutput::volumeChanged, this, &Media::volumeChanged);
    connect(player->audio(), &QtAV::AudioOutput::muteChanged, this, &Media::volumeMutedChanged);
}

void MediaQtAV::setCurrentPlayer(QtAV::AVPlayer *player) {
    if (currentPlayer) {
        currentPlayer->disconnect(this);
        player->audio()->setVolume(currentPlayer->audio()->volume());
        player->audio()->setMute(currentPlayer->audio()->isMute());
        player->setBufferValue(currentPlayer->bufferValue());
    }
    currentPlayer = player;
    connectPlayer(currentPlayer);
}
