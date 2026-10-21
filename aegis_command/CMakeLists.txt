cmake_minimum_required(VERSION 3.28)

project(FitWingCommand LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
)

set(SOURCE_DIR ${PROJECT_SOURCE_DIR}/src)
set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES
    "${SOURCE_DIR}/*.cpp"
    "${INCLUDE_DIR}/*.h"
)

set(QRC_RESOURCES
    ${PROJECT_SOURCE_DIR}/resources/resources.qrc
)

add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
    ${QRC_RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(
    ${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)