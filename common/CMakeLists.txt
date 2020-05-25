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
	Qt5::Network Qt5::Xml
)

target_include_directories(xmlcommon PUBLIC
	Qt5::Network Qt5::Xml
)
