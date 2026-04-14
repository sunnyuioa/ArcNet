
### 20. 粘贴 `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.10)
project(ArcNet VERSION 1.0.0)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -O2 -g")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找需要的库
find_package(Threads REQUIRED)

# 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 源文件列表
set(BUFFER_SOURCES
    buffer/ring_buffer.cpp
)

set(NET_SOURCES
    net/socket_ops.cpp
    net/tcp_socket.cpp
)

set(PROTOCOL_SOURCES
    protocol/message_codec.cpp
)

set(COMMON_SOURCES
)

# 服务器可执行文件
add_executable(arcnet_server
    main.cpp
    ${BUFFER_SOURCES}
    ${NET_SOURCES}
    ${PROTOCOL_SOURCES}
)

# 客户端可执行文件
add_executable(arcnet_client
    client_main.cpp
    ${BUFFER_SOURCES}
    ${NET_SOURCES}
    ${PROTOCOL_SOURCES}
)

# 压测工具
add_executable(benchmark
    benchmark.cpp
    ${BUFFER_SOURCES}
    ${NET_SOURCES}
    ${PROTOCOL_SOURCES}
)

# 链接线程库
target_link_libraries(arcnet_server Threads::Threads)
target_link_libraries(arcnet_client Threads::Threads)
target_link_libraries(benchmark Threads::Threads)

# 设置编译选项
target_compile_options(arcnet_server PRIVATE -Wall -Wextra)
target_compile_options(arcnet_client PRIVATE -Wall -Wextra)
target_compile_options(benchmark PRIVATE -Wall -Wextra)

# 安装规则
install(TARGETS arcnet_server arcnet_client
    RUNTIME DESTINATION bin
)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")