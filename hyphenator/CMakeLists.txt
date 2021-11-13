set(ID hyphenator)
project(${ID} C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui REQUIRED)

add_definitions(-DQT_NO_DEPRECATED_WARNINGS -DHYPHENATOR_DLL_EXPORTS)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
)

add_library(${ID} SHARED
    ${SOURCES}
)

target_include_directories(${ID} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${ID}
        Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui
)
