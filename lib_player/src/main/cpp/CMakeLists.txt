cmake_minimum_required(VERSION 3.4.1)

project("player")

file(GLOB SOURCE ${CMAKE_SOURCE_DIR}/*.cpp)
#设置so库文件路径
set(LIB_PATH ${CMAKE_SOURCE_DIR}/../../../libs/${ANDROID_ABI})
#配置库文件寻找路径
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${LIB_PATH}")
#设置头文件路径
include_directories("${CMAKE_SOURCE_DIR}/includes")

message(${LIB_PATH})

add_library(player SHARED ${SOURCE})

target_link_libraries(
        player
        avcodec avfilter avformat avutil swresample swscale
        android log z OpenSLES
)