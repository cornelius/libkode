cmake_minimum_required(VERSION 3.11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(libkode)

# Qt 5.9 required for QNetworkRequest::NoLessSafeRedirectPolicy
find_package(Qt5 5.9.0 CONFIG REQUIRED COMPONENTS Core Network Xml)

set(CMAKE_AUTOMOC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include_directories(SYSTEM
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/code_generation
)

add_subdirectory(code_generation)
add_subdirectory(common)
add_subdirectory(schema)
