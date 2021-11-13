set(ID test_item_view_project)
project(${ID} C CXX)

set(TOP_SRS_DIR ${PROJECT_SOURCE_DIR})
set(BIN_DIR ${PROJECT_SOURCE_DIR}/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR})
link_directories(${BIN_DIR})

add_subdirectory(hyphenator)
add_subdirectory(zf_itemview)
add_subdirectory(demo)


