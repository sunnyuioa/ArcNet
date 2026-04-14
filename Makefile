CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2 -g -pthread
INCLUDES = -I.

SRC_DIR = .
BUILD_DIR = build
BIN_DIR = bin

# 公共源文件
COMMON_SRCS = \
    buffer/ring_buffer.cpp \
    net/socket_ops.cpp \
    net/tcp_socket.cpp \
    protocol/message_codec.cpp

COMMON_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(COMMON_SRCS:.cpp=.o)))

# 服务器
SERVER_SRCS = main.cpp $(COMMON_SRCS)
SERVER_OBJS = $(BUILD_DIR)/main.o $(COMMON_OBJS)

# 客户端
CLIENT_SRCS = client_main.cpp $(COMMON_SRCS)
CLIENT_OBJS = $(BUILD_DIR)/client_main.o $(COMMON_OBJS)

# 压测工具
BENCHMARK_SRCS = benchmark.cpp $(COMMON_SRCS)
BENCHMARK_OBJS = $(BUILD_DIR)/benchmark.o $(COMMON_OBJS)

all: $(BIN_DIR)/arcnet_server $(BIN_DIR)/arcnet_client $(BIN_DIR)/benchmark

$(BIN_DIR)/arcnet_server: $(SERVER_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES)

$(BIN_DIR)/arcnet_client: $(CLIENT_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES)

$(BIN_DIR)/benchmark: $(BENCHMARK_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES)

$(BUILD_DIR)/%.o: buffer/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/%.o: net/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/%.o: protocol/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/main.o: main.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/client_main.o: client_main.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/benchmark.o: benchmark.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: $(BIN_DIR)/arcnet_server
	$(BIN_DIR)/arcnet_server

run-client: $(BIN_DIR)/arcnet_client
	$(BIN_DIR)/arcnet_client

run-bench: $(BIN_DIR)/benchmark
	$(BIN_DIR)/benchmark 500 100

.PHONY: all clean run run-client run-bench