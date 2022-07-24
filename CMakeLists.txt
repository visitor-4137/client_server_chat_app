cmake_minimum_required(VERSION 3.10)

project(client_server_chat_app)

set (CMAKE_CXX_FLAGS "-O3")

FIND_PACKAGE( Boost 1.40 COMPONENTS system thread REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

add_executable(server server.cpp)
add_executable(client client.cpp)

TARGET_LINK_LIBRARIES( server LINK_PUBLIC ${Boost_LIBRARIES} )

TARGET_LINK_LIBRARIES( client LINK_PUBLIC ${Boost_LIBRARIES} )