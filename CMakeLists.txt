cmake_minimum_required(VERSION 3.16)
project(QtFileManager LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets QUIET)
if (Qt6_FOUND)
    set(QT_LIB Qt6::Widgets)
    qt_add_executable(QtFileManager
        main.cpp
        MainWindow.h
        MainWindow.cpp
        resources.qrc
    )
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    set(QT_LIB Qt5::Widgets)
    add_executable(QtFileManager
        main.cpp
        MainWindow.h
        MainWindow.cpp
        resources.qrc
    )
endif()

if (WIN32)
    target_sources(QtFileManager PRIVATE app.rc)
endif()

target_link_libraries(QtFileManager PRIVATE ${QT_LIB})

if (WIN32)
    set_target_properties(QtFileManager PROPERTIES WIN32_EXECUTABLE YES)
endif()
