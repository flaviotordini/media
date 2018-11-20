INCLUDEPATH += $$PWD/src
DEPENDPATH += $$PWD/src

HEADERS += $$PWD/src/media.h

contains(DEFINES, MEDIA_QTAV) {
    QT += avwidgets
    HEADERS += $$PWD/src/mediaqtav.h
    SOURCES += $$PWD/src/mediaqtav.cpp
}
