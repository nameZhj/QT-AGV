cmake_minimum_required(VERSION 3.16)
project(QT-AGV VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    WebSockets
    Mqtt
    Svg
)

# Collect source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/*.h
    main.cpp
)

# Generate app.rc with an absolute path to the icon, so windres reliably
# locates the .ico regardless of its working directory.
set(APP_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/app-icon.ico)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resources/app.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
    @ONLY
)

add_executable(${PROJECT_NAME} WIN32
    ${SOURCES}
    src/resources.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/app.rc
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
    Qt6::Mqtt
    Qt6::Svg
)
