TARGET = hyphenator

QT += core gui
CONFIG += silent
TEMPLATE = lib

DEFINES += QT_NO_DEPRECATED_WARNINGS

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

DESTDIR = $$PWD/../bin

DEFINES += HYPHENATOR_DLL_EXPORTS

SOURCES += $$files("$$PWD/*.cpp", false)
HEADERS += $$files("$$PWD/*.h", false)
