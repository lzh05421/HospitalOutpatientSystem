cmake_minimum_required(VERSION 3.16)

project(HospitalOutpatientSystem
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(CTest)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Sql Widgets Test PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Sql Widgets Test PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS WebEngineWidgets)

add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(launcher)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
