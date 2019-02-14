INCLUDEPATH += $$PWD/src
DEPENDPATH += $$PWD/src

HEADERS += $$PWD/src/media.h

contains(DEFINES, MEDIA_QTAV) {
    QT += avwidgets
    INCLUDEPATH += $$PWD/src/qtav
    DEPENDPATH += $$PWD/src/qtav
    HEADERS += $$PWD/src/mediaqtav.h
    SOURCES += $$PWD/src/mediaqtav.cpp
}

contains(DEFINES, MEDIA_MPV) {
    QT *= gui widgets
    unix:!mac {
        QT *= x11extras
    }

    LIBS += -lmpv
mac {
    # useful for homebrew: brew install mpv
    # LIBS += -L/usr/local/lib
    # INCLUDEPATH += /usr/local/include
}

    INCLUDEPATH += $$PWD/src/mpv
    DEPENDPATH += $$PWD/src/mpv
    HEADERS += $$PWD/src/mpv/mediampv.h
    SOURCES += $$PWD/src/mpv/mediampv.cpp

    !contains(DEFINES, MEDIA_AUDIOONLY) {
        HEADERS += $$PWD/src/mpv/mpvwidget.h
        SOURCES += $$PWD/src/mpv/mpvwidget.cpp
    }
}
