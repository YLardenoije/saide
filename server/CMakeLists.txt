cmake_minimum_required(VERSION 3.20)

# Static triplet on Windows so saide_server.exe is self-contained: no side-by-side
# DLLs to lose when launched through the repo-root convenience symlink.
if(CMAKE_HOST_WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet")
endif()

project(saide_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(ixwebsocket CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_executable(saide_server
    src/main.cpp
    src/world.cpp
)

target_link_libraries(saide_server PRIVATE ixwebsocket::ixwebsocket nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(saide_server PRIVATE bcrypt)
endif()

# Convenience symlink at the repo root so the built server can be launched
# without navigating into server/build. Local/gitignored, recreated each build.
add_custom_command(TARGET saide_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_SOURCE_DIR}/../saide_server${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            $<TARGET_FILE:saide_server>
            ${CMAKE_SOURCE_DIR}/../saide_server${CMAKE_EXECUTABLE_SUFFIX}
    COMMENT "Linking saide_server executable into the repository root"
)
