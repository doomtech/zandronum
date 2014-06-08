cmake_minimum_required( VERSION 2.4 )

add_library( sqlite3 sqlite3.c )
target_link_libraries( sqlite3 ${CMAKE_DL_LIBS} )

