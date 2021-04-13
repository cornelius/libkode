macro(xmlschema_add_test _target)
   add_executable(${_target} ${_global_add_executable_param} ${ARGN})
   target_link_libraries(${_target} xmlschema Qt${QT_MAJOR_VERSION}::Test)
   add_test(${_target} ${_target})
endmacro()

xmlschema_add_test(tst_xmlelement tst_xmlelement.cpp)
xmlschema_add_test(tst_element tst_element.cpp)
xmlschema_add_test(tst_group tst_group.cpp)
