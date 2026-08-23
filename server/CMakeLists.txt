cmake_minimum_required(VERSION 3.20)
project(runescape_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ixwebsocket CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(runescape_server
    src/main.cpp
    src/world.cpp
)

target_link_libraries(runescape_server PRIVATE ixwebsocket::ixwebsocket nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(runescape_server PRIVATE bcrypt)
endif()
