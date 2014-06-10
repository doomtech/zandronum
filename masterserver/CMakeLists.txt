cmake_minimum_required( VERSION 2.4 )
set( ZAN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src )
include_directories( ${ZAN_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

add_executable( master-97
	main.cpp
	network.cpp
	${ZAN_DIR}/networkshared.cpp
	${ZAN_DIR}/platform.cpp
	${ZAN_DIR}/huffman/bitreader.cpp 
	${ZAN_DIR}/huffman/bitwriter.cpp 
	${ZAN_DIR}/huffman/huffcodec.cpp 
	${ZAN_DIR}/huffman/huffman.cpp
)
