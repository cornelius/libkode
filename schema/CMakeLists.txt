set(SCHEMA_SOURCES
	annotation.cpp
	attribute.cpp
	attributegroup.cpp
	complextype.cpp
	compositor.cpp
	element.cpp
	group.cpp
	parser.cpp
	#schematest.cpp
	simpletype.cpp
	types.cpp
	xmlelement.cpp
	xsdtype.cpp
)

set(SCHEMA_HEADERS
	annotation.h
	attribute.h
	attributegroup.h
	complextype.h
	compositor.h
	element.h
	group.h
	parser.h
	simpletype.h
	types.h
	xmlelement.h
	xsdtype.h
)

add_library(xmlschema STATIC
	${SCHEMA_SOURCES} ${SCHEMA_HEADERS}
)

target_link_libraries(xmlschema
	Qt5::Xml
)

target_include_directories(xmlschema PUBLIC
	Qt5::Xml
)
