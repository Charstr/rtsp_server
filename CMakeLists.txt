cmake_minimum_required(VERSION 3.10)
project(rtspServer VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(TOR_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SRC_DIR ${TOR_DIR}/src)

include_directories(${SRC_DIR})

set(CXX_FLAGS -O0 -g)
add_definitions(${CXX_FLAGS})

set(LD_FLAGS -lpthread -lrt)

file(GLOB_RECURSE SRC_NET ${SRC_DIR}/rtp/*.cpp)

file(GLOB_RECURSE SRC_NET_LEARNING ${TOR_DIR}/networkLearning/*.cpp)


# RTSP ser
# add_executable(parseRTSP test/parseRTSP.cpp ${SRC_BASE})
# target_link_libraries(parseRTSP ${LD_FLAGS})

# add_executable(h264_rtsp_server rtspExample/rtsp_aac_server.cpp  ${SRC_NET})
# target_link_libraries(h264_rtsp_server ${LD_FLAGS})

# socket 
add_executable(p2pcli networkExample/p2pcli.cpp)
target_link_libraries(p2pcli ${LD_FLAGS})

add_executable(p2psrv networkExample/p2psrv.cpp)
target_link_libraries(p2psrv ${LD_FLAGS})

# leetcode
# add_executable(leetcode main.cpp)