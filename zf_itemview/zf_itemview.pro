TARGET = zf_itemview

QT += core gui widgets
QT += core-private widgets-private gui-private

CONFIG += silent
TEMPLATE = lib

DEFINES += QT_NO_DEPRECATED_WARNINGS

DESTDIR = $$PWD/../bin

DEFINES += ZF_ITEMVIEW_DLL_EXPORTS

unix {
    linux-clang {
        QMAKE_CXXFLAGS += -Wno-gnu-inline-cpp-without-extern
        # Иначе clang генерит кучу предупреждений в заголовках Qt
        QMAKE_CXXFLAGS += -Wno-deprecated-copy

    } else {
        # Иначе gcc 9 генерит кучу предупреждений в заголовках Qt
        QMAKE_CXXFLAGS += -Wno-deprecated-copy
    }
}

SOURCES += $$files("$$PWD/*.cpp", true)
HEADERS += $$files("$$PWD/*.h", true)

INCLUDEPATH += ../hyphenator

LIBS += -L$$absolute_path($$DESTDIR) -lhyphenator
