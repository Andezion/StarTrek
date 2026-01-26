cmake_minimum_required(VERSION 3.16)
project(cosmodrom-visualizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(raylib 4.5 QUIET)
if(NOT raylib_FOUND)
    message(STATUS "raylib not found, fetching from source...")
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

find_package(ixwebsocket QUIET)
if(NOT ixwebsocket_FOUND)
    message(STATUS "IXWebSocket not found, fetching from source...")
    include(FetchContent)
    FetchContent_Declare(
        ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG v11.4.5
        GIT_SHALLOW TRUE
    )
    set(USE_TLS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ixwebsocket)
endif()

find_package(nlohmann_json 3.9 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from source...")
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

set(SOURCES
    src/main.cpp
    src/app.cpp
    src/network/ws_client.cpp
    src/network/protocol.cpp
    src/data/simulation_state.cpp
    src/data/rocket_data.cpp
    src/scene/scene.cpp
    src/scene/camera.cpp
    src/ui/ui_manager.cpp
)

set(HEADERS
    include/app.hpp
    include/network/ws_client.hpp
    include/network/protocol.hpp
    include/data/simulation_state.hpp
    include/data/rocket_data.hpp
    include/scene/scene.hpp
    include/scene/camera.hpp
    include/ui/ui_manager.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    ixwebsocket
    nlohmann_json::nlohmann_json
)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/include/raygui.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/raysan5/raygui/master/src/raygui.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/raygui.h
        STATUS DOWNLOAD_STATUS
    )
endif()
