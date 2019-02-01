#include "mpvwidget.h"
#include <stdexcept>

#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
#include <QtX11Extras/QX11Info>
#endif

static void *get_proc_address(void *ctx, const char *name) {
    Q_UNUSED(ctx);
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx) return nullptr;
    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

MpvWidget::MpvWidget(mpv_handle *mpv, QWidget *parent, Qt::WindowFlags f)
    : QOpenGLWidget(parent, f), mpv(mpv), mpv_gl(nullptr) {
    // moveToThread(qApp->thread());
    // setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    // setAttribute(Qt::WA_NativeWindow);
    // hide();
}

MpvWidget::~MpvWidget() {
    makeCurrent();
    if (mpv_gl) mpv_render_context_free(mpv_gl);
    mpv_terminate_destroy(mpv);
}

void MpvWidget::initializeGL() {
    if (mpv_gl) qFatal("Already initialized");

    QWidget *nativeParent = nativeParentWidget();
    qDebug() << "initializeGL" << nativeParent;
    if (nativeParent == nullptr) qFatal("No native parent");

    mpv_opengl_init_params gl_init_params{get_proc_address, this, nullptr};
    mpv_render_param params[]{{MPV_RENDER_PARAM_API_TYPE, (void *)MPV_RENDER_API_TYPE_OPENGL},
                              {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
                              {MPV_RENDER_PARAM_INVALID, nullptr},
                              {MPV_RENDER_PARAM_INVALID, nullptr}};

#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
    const QString platformName = QGuiApplication::platformName();
    if (platformName.contains("xcb")) {
        params[2].type = MPV_RENDER_PARAM_X11_DISPLAY;
        params[2].data = (void *)QX11Info::display();
        qDebug() << platformName << params[2].data;
    } else if (platformName.contains("wayland")) {
        qWarning() << "Wayland not supported";
    }
#endif

    if (mpv_render_context_create(&mpv_gl, mpv, params) < 0)
        qFatal("failed to initialize mpv GL context");
    mpv_render_context_set_update_callback(mpv_gl, MpvWidget::onUpdate, (void *)this);

    connect(this, &QOpenGLWidget::frameSwapped, this, &MpvWidget::onFrameSwapped);

    /*
    connect(this, &QOpenGLWidget::aboutToResize, this, [this] {
        qDebug() << "aboutToResize";
        setUpdatesEnabled(false);
    });
    connect(this, &QOpenGLWidget::resized, this, [this] {
        qDebug() << "resized";
        setUpdatesEnabled(true);
    });
    */
}

void MpvWidget::resizeGL(int w, int h) {
    // makeCurrent();
    qreal r = devicePixelRatioF();
    glWidth = int(w * r);
    glHeight = int(h * r);
}

void MpvWidget::paintGL() {
    mpv_opengl_fbo fbo{static_cast<int>(defaultFramebufferObject()), glWidth, glHeight, 0};

    static bool yes = true;
    mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
                                 {MPV_RENDER_PARAM_FLIP_Y, &yes},
                                 {MPV_RENDER_PARAM_INVALID, nullptr}};
    // See render_gl.h on what OpenGL environment mpv expects, and
    // other API details.
    mpv_render_context_render(mpv_gl, params);
}

// Make Qt invoke mpv_opengl_cb_draw() to draw a new/updated video frame.
void MpvWidget::maybeUpdate() {
    // If the Qt window is not visible, Qt's update() will just skip rendering.
    // This confuses mpv's opengl-cb API, and may lead to small occasional
    // freezes due to video rendering timing out.
    // Handle this by manually redrawing.
    // Note: Qt doesn't seem to provide a way to query whether update() will
    //       be skipped, and the following code still fails when e.g. switching
    //       to a different workspace with a reparenting window manager.
    QWidget *w = window();
    if (!w->updatesEnabled() || w->isHidden() || w->isMinimized() || isHidden() ||
        !updatesEnabled()) {
        makeCurrent();
        paintGL();
        QOpenGLContext *c = context();
        c->swapBuffers(c->surface());
        doneCurrent();
        mpv_render_context_report_swap(mpv_gl);
    } else {
        update();
    }
}

void MpvWidget::onFrameSwapped() {
    mpv_render_context_report_swap(mpv_gl);
}

void MpvWidget::onUpdate(void *ctx) {
    QMetaObject::invokeMethod((MpvWidget *)ctx, "maybeUpdate");
}
