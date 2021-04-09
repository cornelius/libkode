set(CODE_GENERATION_SOURCES
	class.cpp
	code.cpp
	enum.cpp
	file.cpp
	function.cpp
	include.cpp
	license.cpp
	membervariable.cpp
	namer.cpp
	printer.cpp
	statemachine.cpp
	style.cpp
	typedef.cpp
	variable.cpp
)

set(CODE_GENERATION_HEADERS
	class.h
	code.h
	enum.h
	file.h
	function.h
	include.h
	kode_export.h
	license.h
	membervariable.h
	namer.h
	printer.h
	statemachine.h
	style.h
	typedef.h
	variable.h
)

add_library(kode STATIC
	${CODE_GENERATION_SOURCES} ${CODE_GENERATION_HEADERS}
)

target_link_libraries(kode
        Qt${QT_MAJOR_VERSION}::Core
)

target_include_directories(kode PUBLIC
        Qt${QT_MAJOR_VERSION}::Core
)

set_target_properties(kode PROPERTIES OUTPUT_NAME "kode${kode_LIBRARY_QTID}")
