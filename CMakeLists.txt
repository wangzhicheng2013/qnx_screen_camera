cmake_minimum_required(VERSION 3.5)
project(qnx_screen_camera)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 11)
set(CURRENT_ROOT_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_SYSTEM_NAME LINUX)
set(QNX_HOST "$ENV{QNX_HOST}")
set(QNX_TARGET "$ENV{QNX_TARGET}")

message("QNX_HOST = ${QNX_HOST}")
message("QNX_TARGET = ${QNX_TARGET}")

set(CMAKE_C_COMPILER ${QNX_HOST}/usr/bin/aarch64-unknown-nto-qnx7.0.0-gcc)
set(CMAKE_CXX_COMPILER ${QNX_HOST}/usr/bin/aarch64-unknown-nto-qnx7.0.0-g++)

set(TARGET                  ${QNX_TARGET})
set(QNX_SOURCE_DIR          ${CURRENT_ROOT_PATH}/target)

include_directories(${TARGET}/usr/include)
include_directories(${QNX_SOURCE_DIR}/usr/include)
message("target path:" ${TARGET}/usr/include)
message("qnx source dir:" ${QNX_SOURCE_DIR}/usr/include)

include_directories(utils)
include_directories(screen)
include_directories(camera)

set(LIB_DIR 
   ${TARGET}/aarch64le/usr/lib
   ${TARGET}/aarch64le/lib
   ${QNX_SOURCE_DIR}/usr/lib)

link_directories(${LIB_DIR})

set(LINK_ALL_LIBRARIES
        screen
        ais_client)

add_executable(qnx_screen_camera main/main.cpp)
target_link_libraries(qnx_screen_camera ${LINK_ALL_LIBRARIES})

