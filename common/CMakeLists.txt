set(COMMON_SOURCES
	fileprovider.cpp
	messagehandler.cpp
	nsmanager.cpp
	parsercontext.cpp
	qname.cpp
)

set(COMMON_HEADERS
	fileprovider.h
	messagehandler.h
	nsmanager.h
	parsercontext.h
	qname.h
)

add_library(xmlcommon STATIC
	${COMMON_SOURCES} ${COMMON_HEADERS}
)

target_link_libraries(xmlcommon
        Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::Xml
)

target_include_directories(xmlcommon PUBLIC
   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

set_target_properties(xmlcommon PROPERTIES OUTPUT_NAME "xmlcommon${kode_LIBRARY_QTID}")
