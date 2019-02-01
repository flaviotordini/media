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
