cmake_minimum_required(VERSION 3.10)
project(ArcNet VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -O2 -g")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Threads REQUIRED)

# 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ----- 查找 MySQL -----
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)
include_directories(${MYSQL_INCLUDE_DIRS})
link_directories(${MYSQL_LIBRARY_DIRS})

# ----- 查找/下载 nlohmann/json -----
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found, will use FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
    set(NLOHMANN_JSON_INCLUDE_DIR ${json_SOURCE_DIR}/include)
endif()
include_directories(${NLOHMANN_JSON_INCLUDE_DIR})

# ----- 查找 CURL（用于 DeepSeek API 调用）-----
find_package(CURL REQUIRED)

# 源文件
set(BUFFER_SOURCES      buffer/ring_buffer.cpp)
set(NET_SOURCES         net/socket_ops.cpp net/tcp_socket.cpp)
set(PROTOCOL_SOURCES    protocol/message_codec.cpp)
set(STORAGE_SOURCES     storage_manager/storage_manager.cpp)
set(MYSQL_SOURCES       mysql/DBWorker.cpp)
set(MATCH_SOURCES       MatchManager/MatchManager.cpp)
set(SUBREACTOR_SOURCES  concurrency/SubReactor.cpp)
set(ROOM_SOURCES        Room/Room.cpp Room/RoomManager.cpp Room/deepseek_api.cpp)   # 包含 DeepSeek API 文件

add_executable(arcnet_server
    main.cpp
    ${BUFFER_SOURCES}
    ${NET_SOURCES}
    ${PROTOCOL_SOURCES}
    ${STORAGE_SOURCES}
    ${MYSQL_SOURCES}
    ${MATCH_SOURCES}
    ${SUBREACTOR_SOURCES}
    ${ROOM_SOURCES}
)

add_executable(arcnet_client
    client_main.cpp
    ${BUFFER_SOURCES}
    ${NET_SOURCES}
    ${PROTOCOL_SOURCES}
    ${STORAGE_SOURCES}
    ${MYSQL_SOURCES}
    ${MATCH_SOURCES}
    ${SUBREACTOR_SOURCES}
    ${ROOM_SOURCES}
)

add_executable(benchmark
    benchmark.cpp
    ${BUFFER_SOURCES}
    ${NET_SOURCES}
    ${PROTOCOL_SOURCES}
    ${STORAGE_SOURCES}
    ${MYSQL_SOURCES}
    ${MATCH_SOURCES}
    ${SUBREACTOR_SOURCES}
    ${ROOM_SOURCES}
)

# 链接库
target_link_libraries(arcnet_server
    Threads::Threads
    stdc++fs
    ${MYSQL_LIBRARIES}
    CURL::libcurl
)
target_link_libraries(arcnet_client
    Threads::Threads
    stdc++fs
    ${MYSQL_LIBRARIES}
    CURL::libcurl
)
target_link_libraries(benchmark
    Threads::Threads
    stdc++fs
    ${MYSQL_LIBRARIES}
    CURL::libcurl
)

target_compile_options(arcnet_server PRIVATE -Wall -Wextra)
target_compile_options(arcnet_client PRIVATE -Wall -Wextra)
target_compile_options(benchmark PRIVATE -Wall -Wextra)

install(TARGETS arcnet_server arcnet_client RUNTIME DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")