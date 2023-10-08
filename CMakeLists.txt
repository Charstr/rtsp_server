cmake_minimum_required(VERSION 3.10)
project(rtspServer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(TOR_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SRC_DIR ${TOR_DIR}/src)

set(CXX_FLAGS -O0 -g)
add_definitions(${CXX_FLAGS})

set(LD_FLAGS -lpthread -lrt)

include_directories(${SRC_DIR})

file(GLOB_RECURSE SRC_BASE ${SRC_DIR}/base/*.cpp)

file(GLOB_RECURSE SRC_MEDIA ${SRC_DIR}/media/*.cpp)

file(GLOB_RECURSE SRC_SCHEDULE ${SRC_DIR}/schedule/*.cpp)


file(GLOB_RECURSE SRC_SERVER ${SRC_DIR}/server/*.cpp)

add_executable(h264_rtsp_server 
        rtspExample/01_h264_rtsp_server.cpp 
        ${SRC_BASE} 
        ${SRC_MEDIA}
        ${SRC_SCHEDULE}
        ${SRC_SERVER})
        
target_link_libraries(h264_rtsp_server ${LD_FLAGS})

# add_executable(rtsp_h264_server rtspExample/rtsp_h264_server.cpp  ${SRC_NET})
# target_link_libraries(rtsp_h264_server ${LD_FLAGS})


# socket 
# add_executable(client1 networkExample/client1.cpp)
# target_link_libraries(client1 ${LD_FLAGS})

# add_executable(server1 networkExample/server1.cpp)
# target_link_libraries(server1 ${LD_FLAGS})

#leetcode
add_executable(leetcode main.cpp)