cmake_minimum_required(VERSION 3.10)

project(compressor)

add_compile_options(-std=c++17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_BUILD_TYPE "Debug")
set(PYBIND11_CPP_STANDARD -std=c++17)

find_package(pybind11 REQUIRED)
find_package(SDL2 REQUIRED)


pybind11_add_module(compressor
            src/py_pushwork.cpp
            src/utils.cpp
            src/encoder.cpp
            src/pushwork.cpp
)


target_include_directories(compressor PRIVATE 
    ./_include
	/home/wanghf/ffmpeg_build/include
    /home/wanghf/anaconda3/lib/python3.12/site-packages/numpy/core/include
    /usr/local/include/opencv4
    ${pybind11_INCLUDE_DIRS}
	${SDL2_INCLUDE_DIRS}
)


target_link_directories(compressor PRIVATE
    /usr/local/lib
	/home/wanghf/ffmpeg_build/lib
	${SDL2_LIBRARY_DIRS}
)


target_link_libraries(compressor PRIVATE
    pybind11::module
    avformat
    avcodec
    avdevice
    avfilter
    swscale
    swresample
    avutil
    SDL2::SDL2  # 使用 SDL2 的目标
    
    # opencv 
    opencv_core
    opencv_highgui
    opencv_imgproc
    opencv_imgcodecs

    # basic libs
    bz2
    asound
    z
    m
    pthread
    dl
)