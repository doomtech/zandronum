cmake_minimum_required( VERSION 2.4 )

get_target_property(ZIPDIR_EXE zipdir LOCATION)

add_custom_command( OUTPUT ${ZDOOM_OUTPUT_DIR}/zandronum.pk3
	COMMAND ${ZIPDIR_EXE} -udf ${ZDOOM_OUTPUT_DIR}/zandronum.pk3 ${CMAKE_CURRENT_SOURCE_DIR}/static ${CMAKE_CURRENT_BINARY_DIR}/generated
	DEPENDS zipdir ${CMAKE_CURRENT_SOURCE_DIR}/static )

add_custom_target( pk3 ALL
	DEPENDS ${ZDOOM_OUTPUT_DIR}/zandronum.pk3 )

