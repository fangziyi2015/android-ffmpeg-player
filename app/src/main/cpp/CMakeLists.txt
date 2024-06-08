cmake_minimum_required(VERSION 3.6.4111459)

project("fmplayer")

set(FFMPEG "${CMAKE_SOURCE_DIR}/ffmpeg")
set(RTMP "${CMAKE_SOURCE_DIR}/rtmp")

include_directories("${FFMPEG}/include")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${FFMPEG}/libs/${CMAKE_ANDROID_ARCH_ABI}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${RTMP}/libs/${CMAKE_ANDROID_ARCH_ABI}")

file(GLOB src_file *.cpp)

add_library(${CMAKE_PROJECT_NAME} SHARED
        ${src_file})

target_link_libraries(${CMAKE_PROJECT_NAME}
        # 需要放在前面，否则会编译不通过
        -Wl,--start-group
        avcodec avfilter avformat avutil swresample swscale
        -Wl,--end-group
        android
        log
        z
        rtmp
        OpenSLES
        )