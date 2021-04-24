QT       += core gui

QT += widgets

DEFINES += QT_NO_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

DESTDIR = $$PWD/../bin
INCLUDEPATH += ../zf_itemview

LIBS += -L$$absolute_path($$DESTDIR) -lhyphenator -lzf_itemview
