#include "mediampv.h"

#include <clocale>
#include <mpv/qthelper.hpp>

#ifndef MEDIA_AUDIOONLY
#include "mpvwidget.h"
#endif

// TODO: screenshot

namespace {
void wakeup(void *ctx) {
    // This callback is invoked from any mpv thread (but possibly also
    // recursively from a thread that is calling the mpv API). Just notify
    // the Qt GUI thread to wake up (so that it can process events with
    // mpv_wait_event()), and return as quickly as possible.
    MediaMPV *mediaMPV = (MediaMPV *)ctx;
    emit mediaMPV->mpvEvents();
}

} // namespace

MediaMPV::MediaMPV(QObject *parent) : Media(parent), widget(nullptr) {
    QThread *thread = new QThread(this);
    thread->start();
    moveToThread(thread);
    connect(this, &QObject::destroyed, thread, &QThread::quit);

#ifndef Q_OS_WIN
    // Qt sets the locale in the QApplication constructor, but libmpv requires
    // the LC_NUMERIC category to be set to "C", so change it back.
    std::setlocale(LC_NUMERIC, "C");
#endif

    mpv = mpv_create();
    if (!mpv) qFatal("Cannot create MPV instance");

    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "audio-display", "no");
    mpv_set_option_string(mpv, "gapless-audio", "weak");
    mpv_set_option_string(mpv, "idle", "yes");
    mpv_set_option_string(mpv, "input-default-bindings", "no");
    mpv_set_option_string(mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(mpv, "input-cursor", "no");
    mpv_set_option_string(mpv, "input-media-keys", "no");
    mpv_set_option_string(mpv, "ytdl", "no");
    mpv_set_option_string(mpv, "fs", "no");
    mpv_set_option_string(mpv, "osd-level", "0");
    mpv_set_option_string(mpv, "quiet", "yes");
    mpv_set_option_string(mpv, "softvol", "yes");
    mpv_set_option_string(mpv, "load-scripts", "no");
    mpv_set_option_string(mpv, "audio-client-name",
                          QCoreApplication::applicationName().toUtf8().data());
    mpv_set_option_string(mpv, "hwdec", "auto");

#if defined Q_OS_UNIX && !defined Q_OS_DARWIN
    // mpv_set_option_string(mpv, "gpu-context", "x11egl");
    // mpv_set_option_string(mpv, "gpu-hwdec-interop", "vaapi-egl");
#else
    // mpv_set_option_string(mpv, "opengl-early-flush", "no");
    // mpv_set_option_string(mpv, "cocoa-force-dedicated-gpu", "yes");
    // mpv_set_option_string(mpv, "native-fs", "no");
#endif

#ifndef MEDIA_AUDIOONLY
    mpv_set_option_string(mpv, "vo", "libmpv");
#endif

#ifdef MEDIA_MPV_WID
    widget = new QWidget();
    // widget->setAttribute(Qt::WA_DontCreateNativeAncestors);
    // widget->setAttribute(Qt::WA_NativeWindow);
    // If you have a HWND, use: int64_t wid = (intptr_t)hwnd;
    int64_t wid = (intptr_t)widget->winId();
    mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);
#endif

    // Let us receive property change events with MPV_EVENT_PROPERTY_CHANGE if
    // this property changes.
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "mute", MPV_FORMAT_FLAG);

#ifndef QT_NO_DEBUG_OUTPUT
    // Request log messages
    // They are received as MPV_EVENT_LOG_MESSAGE.
    mpv_request_log_messages(mpv, "info");
#endif

    // From this point on, the wakeup function will be called. The callback
    // can come from any thread, so we use the QueuedConnection mechanism to
    // relay the wakeup in a thread-safe way.
    connect(this, &MediaMPV::mpvEvents, this, &MediaMPV::onMpvEvents, Qt::QueuedConnection);
    mpv_set_wakeup_callback(mpv, wakeup, this);

    if (mpv_initialize(mpv) < 0) qFatal("mpv failed to initialize");
}

// This slot is invoked by wakeup() (through the mpvEvents signal).
void MediaMPV::onMpvEvents() {
    // Process all events, until the event queue is empty.
    while (mpv) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        handleMpvEvent(event);
    }
}

void MediaMPV::checkAboutToFinish(qint64 position) {
    if (!aboutToFinishEmitted && currentState == Media::PlayingState) {
        const qint64 dur = duration();
        if (dur > 0 && dur - position < 5000) {
            aboutToFinishEmitted = true;
            qDebug() << "aboutToFinish" << position << dur;
            emit aboutToFinish();
        }
    }
}

void MediaMPV::handleMpvEvent(mpv_event *event) {
    // qDebug() << event->data;
    switch (event->event_id) {
    case MPV_EVENT_START_FILE:
        setState(Media::LoadingState);
        break;

    case MPV_EVENT_SEEK:
        setState(Media::BufferingState);
        break;

    case MPV_EVENT_FILE_LOADED:
        emit sourceChanged();
        setState(Media::PlayingState);
        break;

    case MPV_EVENT_PLAYBACK_RESTART:
    case MPV_EVENT_UNPAUSE:
        setState(Media::PlayingState);
        break;

    case MPV_EVENT_END_FILE: {
        struct mpv_event_end_file *eof_event = (struct mpv_event_end_file *)event->data;
        if (eof_event->reason == MPV_END_FILE_REASON_EOF ||
            eof_event->reason == MPV_END_FILE_REASON_ERROR) {
            qDebug() << "Finished";
            setState(Media::StoppedState);
            emit finished();
        }
        break;
    }

    case MPV_EVENT_PAUSE:
        setState(Media::PausedState);
        break;

    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = (mpv_event_property *)event->data;
        // qDebug() << prop->name << prop->data;

        if (strcmp(prop->name, "time-pos") == 0) {
            if (prop->format == MPV_FORMAT_DOUBLE) {
                double seconds = *(double *)prop->data;
                qint64 ms = seconds * 1000.;
                emit positionChanged(ms);
                checkAboutToFinish(ms);
            }
        }

        else if (strcmp(prop->name, "volume") == 0) {
            if (prop->format == MPV_FORMAT_DOUBLE) {
                double vol = *(double *)prop->data;
                emit volumeChanged(vol / 100.);
            }
        }

        else if (strcmp(prop->name, "mute") == 0) {
            if (prop->format == MPV_FORMAT_FLAG) {
                int mute = *(int *)prop->data;
                emit volumeMutedChanged(mute == 0);
            }
        }

        break;
    }

    case MPV_EVENT_LOG_MESSAGE: {
        struct mpv_event_log_message *msg = (struct mpv_event_log_message *)event->data;
        qDebug() << "[" << msg->prefix << "] " << msg->level << ": " << msg->text;

        if (msg->log_level == MPV_LOG_LEVEL_ERROR) {
            lastErrorString = QString::fromUtf8(msg->text);
            emit error(lastErrorString);
        }

        break;
    }

    case MPV_EVENT_SHUTDOWN: {
        mpv_terminate_destroy(mpv);
        mpv = nullptr;
        break;
    }

    default:;
        // Unhandled events
    }
}

void MediaMPV::sendCommand(const char *args[]) {
    // mpv_command_async(mpv, 0, args);
    mpv_command(mpv, args);
}

void MediaMPV::setState(Media::State value) {
    if (value != currentState) {
        currentState = value;
        emit stateChanged(currentState);
    }
}

void MediaMPV::clearTrackState() {
    lastErrorString.clear();
    aboutToFinishEmitted = false;

    if (currentState == Media::PausedState) play();
}

void MediaMPV::setAudioOnly(bool value) {}

#ifndef MEDIA_AUDIOONLY

void MediaMPV::setRenderer(const QString &name) {}

QWidget *MediaMPV::videoWidget() {
    if (!widget) {
        widget = new MpvWidget(mpv);
    }
    return widget;
}

void MediaMPV::playSeparateAudioAndVideo(const QString &video, const QString &audio) {
    const QByteArray fileUtf8 = video.toUtf8();
    const char *args[] = {"loadfile", fileUtf8.constData(), nullptr};
    sendCommand(args);

    const QByteArray audioUtf8 = audio.toUtf8();
    const char *args2[] = {"audio-add", audioUtf8.constData(), nullptr};
    sendCommand(args2);

    clearTrackState();
}

void MediaMPV::snapshot() {}

#endif

void MediaMPV::init() {}

Media::State MediaMPV::state() const {
    return currentState;
}

void MediaMPV::play(const QString &file) {
    const QByteArray fileUtf8 = file.toUtf8();
    const char *args[] = {"loadfile", fileUtf8.constData(), nullptr};
    sendCommand(args);

    clearTrackState();
}

void MediaMPV::play() {
    int flag = 0;
    mpv_set_property_async(mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MediaMPV::pause() {
    int flag = 1;
    mpv_set_property_async(mpv, 0, "pause", MPV_FORMAT_FLAG, &flag);
}

void MediaMPV::stop() {
    const char *args[] = {"stop", nullptr};
    sendCommand(args);
}

void MediaMPV::seek(qint64 ms) {
    double seconds = ms / 1000.;
    QByteArray ba = QString::number(seconds).toUtf8();
    const char *args[] = {"seek", ba.constData(), "absolute", nullptr};
    sendCommand(args);
}

QString MediaMPV::file() const {
    char *path;
    mpv_get_property(mpv, "path", MPV_FORMAT_STRING, &path);
    return QString::fromUtf8(path);
}

void MediaMPV::setBufferMilliseconds(qint64 value) {
    Q_UNUSED(value);
    // Not implemented
}

void MediaMPV::setUserAgent(const QString &value) {
    mpv_set_option_string(mpv, "user-agent", value.toUtf8());
}

void MediaMPV::enqueue(const QString &file) {
    const QByteArray fileUtf8 = file.toUtf8();
    const char *args[] = {"loadfile", fileUtf8.constData(), "append", nullptr};
    sendCommand(args);
}

void MediaMPV::clearQueue() {
    const char *args[] = {"playlist-clear", nullptr};
    sendCommand(args);
}

bool MediaMPV::hasQueue() const {
    mpv_node node;
    int r = mpv_get_property(mpv, "playlist", MPV_FORMAT_NODE, &node);
    if (r < 0) return false;
    QVariant v = mpv::qt::node_to_variant(&node);
    mpv_free_node_contents(&node);
    QVariantList list = v.toList();
    return list.count() > 1;
}

qint64 MediaMPV::position() const {
    double seconds;
    mpv_get_property(mpv, "time-pos", MPV_FORMAT_DOUBLE, &seconds);
    return seconds * 1000.;
}

qint64 MediaMPV::duration() const {
    double seconds;
    mpv_get_property(mpv, "duration", MPV_FORMAT_DOUBLE, &seconds);
    return seconds * 1000.;
}

qint64 MediaMPV::remainingTime() const {
    double seconds;
    mpv_get_property(mpv, "time-remaining", MPV_FORMAT_DOUBLE, &seconds);
    return seconds * 1000.;
}

qreal MediaMPV::volume() const {
    double vol;
    mpv_get_property(mpv, "ao-volume", MPV_FORMAT_DOUBLE, &vol);
    return vol / 100.;
}

void MediaMPV::setVolume(qreal value) {
    double percent = value * 100.;
    mpv_set_property_async(mpv, 0, "ao-volume", MPV_FORMAT_DOUBLE, &percent);
}

bool MediaMPV::volumeMuted() const {
    int mute;
    mpv_get_property(mpv, "mute", MPV_FORMAT_FLAG, &mute);
    qDebug() << "mute is" << mute;
    return mute != 0;
}

void MediaMPV::setVolumeMuted(bool value) {
    int mute = value ? 1 : 0;
    mpv_set_property(mpv, "mute", MPV_FORMAT_FLAG, &mute);
}

QString MediaMPV::errorString() const {
    return lastErrorString;
}
