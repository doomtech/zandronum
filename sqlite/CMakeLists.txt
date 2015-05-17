cmake_minimum_required( VERSION 2.4 )

# [BB] Download SQLite if necessary.
IF ( NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.h )
	# [BB] Specifies the desired SQLite version 
	SET ( SQLITE_VERSION 3081001 )
	SET ( SQLITE_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/sqlite.tar.gz )
	FILE ( DOWNLOAD https://www.sqlite.org/2015/sqlite-autoconf-${SQLITE_VERSION}.tar.gz ${SQLITE_ARCHIVE} SHOW_PROGRESS )
	EXECUTE_PROCESS ( COMMAND ${CMAKE_COMMAND} -E tar xzf ${SQLITE_ARCHIVE} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )

	FILE ( COPY ${CMAKE_CURRENT_BINARY_DIR}/sqlite-autoconf-${SQLITE_VERSION}/sqlite3.c DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} )
	FILE ( COPY ${CMAKE_CURRENT_BINARY_DIR}/sqlite-autoconf-${SQLITE_VERSION}/sqlite3.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} )
	FILE ( COPY ${CMAKE_CURRENT_BINARY_DIR}/sqlite-autoconf-${SQLITE_VERSION}/sqlite3ext.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR} )
ENDIF ( )

add_library( sqlite3 sqlite3.c )
target_link_libraries( sqlite3 ${CMAKE_DL_LIBS} )

