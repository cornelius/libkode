add_executable(tst_xmlelement tst_xmlelement.cpp)
target_link_libraries(tst_xmlelement xmlschema Qt5::Test)
add_test(tst_xmlelement tst_xmlelement)
