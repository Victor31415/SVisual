cmake_minimum_required(VERSION 3.0.2)

include(../cmake/qt.cmake)

project(SVDocker)

FILE(GLOB_RECURSE SRC_SVDock "*.cpp" "*.h")

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif(UNIX)

add_executable(SVDocker ${SRC_SVDock})

if(MSVC) 
   set_target_properties (SVDocker PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT") 
endif(MSVC)

target_link_libraries(SVDocker SVConfig SVAuxFunc SVServer SVWebServer)

qt5_use_modules(SVDocker Core)