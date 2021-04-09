# Pass the following variables to cmake to control the build:
#
# -Dlibkode_QT6=[true|false]
#  Build against Qt6 rather than Qt5
#  Default=false (Qt5 will be used even if Qt6 is available)

cmake_minimum_required(VERSION 3.11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(libkode)

option(${PROJECT_NAME}_QT6 "Build against Qt 6" OFF)

if(${PROJECT_NAME}_QT6)
  find_package(Qt6 6.0.0 CONFIG REQUIRED COMPONENTS Core Network Xml Test)
  set(QT_MAJOR_VERSION 6)
  set(libkode_LIBRARY_QTID "-qt6")
else()
  # Qt 5.9 required for QNetworkRequest::NoLessSafeRedirectPolicy
  find_package(Qt5 5.9.0 CONFIG REQUIRED COMPONENTS Core Network Xml Test)
  set(QT_MAJOR_VERSION 5)
  set(libkode_LIBRARY_QTID "")
endif()

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(SYSTEM
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/code_generation
)

enable_testing()

add_subdirectory(code_generation)
add_subdirectory(common)
add_subdirectory(schema)
add_subdirectory(autotests)
