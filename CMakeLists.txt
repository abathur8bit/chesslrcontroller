cmake_minimum_required(VERSION 3.0.2)
project(chesslrcontroller)

set(CMAKE_CXX_STANDARD 14)

SET(CURSES_USE_NCURSES TRUE)

#set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_FLAGS "-std=c++11")


include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_executable(chesslrcontroller src/main/cpp/controller.cpp)
#add_executable(actiontest src/main/cpp/actiontest.cpp )
#add_executable(boardtest src/main/cpp/boardtest.cpp)
add_executable(test src/main/cpp/test.cpp)

target_link_libraries(chesslrcontroller PRIVATE ssobjects wiringPi pthread)
#target_link_libraries(actiontest PRIVATE ssobjects wiringPi pthread)
#target_link_libraries(boardtest PRIVATE ssobjects wiringPi pthread)
