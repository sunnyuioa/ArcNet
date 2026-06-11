CXX = g++
CXXFLAGS = -std=c++14 -Wall -O2 -g -pthread
LDFLAGS = -lstdc++fs -lmysqlclient -lcurl
INCLUDES = -I.

BUILD_DIR = build
BIN_DIR = bin

# 各个模块的源文件
BUFFER_SRCS    = buffer/ring_buffer.cpp
NET_SRCS       = net/socket_ops.cpp net/tcp_socket.cpp
PROTOCOL_SRCS  = protocol/message_codec.cpp
STORAGE_SRCS   = storage_manager/storage_manager.cpp
MYSQL_SRCS     = mysql/DBWorker.cpp
MATCH_SRCS     = MatchManager/MatchManager.cpp
SUBREACTOR_SRCS = concurrency/SubReactor.cpp
ROOM_SRCS      = Room/Room.cpp Room/RoomManager.cpp Room/deepseek_api.cpp

# 所有公共源文件
COMMON_SRCS = \
    $(BUFFER_SRCS) \
    $(NET_SRCS) \
    $(PROTOCOL_SRCS) \
    $(STORAGE_SRCS) \
    $(MYSQL_SRCS) \
    $(MATCH_SRCS) \
    $(SUBREACTOR_SRCS) \
    $(ROOM_SRCS)

# 为每个源文件生成唯一的 .o 文件名（带模块前缀）
BUFFER_OBJS    = $(patsubst buffer/%.cpp, $(BUILD_DIR)/buffer_%.o, $(BUFFER_SRCS))
NET_OBJS       = $(patsubst net/%.cpp, $(BUILD_DIR)/net_%.o, $(NET_SRCS))
PROTOCOL_OBJS  = $(patsubst protocol/%.cpp, $(BUILD_DIR)/protocol_%.o, $(PROTOCOL_SRCS))
STORAGE_OBJS   = $(patsubst storage_manager/%.cpp, $(BUILD_DIR)/storage_manager_%.o, $(STORAGE_SRCS))
MYSQL_OBJS     = $(patsubst mysql/%.cpp, $(BUILD_DIR)/mysql_%.o, $(MYSQL_SRCS))
MATCH_OBJS     = $(patsubst MatchManager/%.cpp, $(BUILD_DIR)/MatchManager_%.o, $(MATCH_SRCS))
SUBREACTOR_OBJS = $(patsubst concurrency/%.cpp, $(BUILD_DIR)/concurrency_%.o, $(SUBREACTOR_SRCS))
ROOM_OBJS      = $(patsubst Room/%.cpp, $(BUILD_DIR)/Room_%.o, $(ROOM_SRCS))

# 所有公共目标文件
COMMON_OBJS = \
    $(BUFFER_OBJS) \
    $(NET_OBJS) \
    $(PROTOCOL_OBJS) \
    $(STORAGE_OBJS) \
    $(MYSQL_OBJS) \
    $(MATCH_OBJS) \
    $(SUBREACTOR_OBJS) \
    $(ROOM_OBJS)

# 各可执行文件所需的目标文件
SERVER_OBJS    = $(BUILD_DIR)/main.o $(COMMON_OBJS)
CLIENT_OBJS    = $(BUILD_DIR)/client_main.o $(COMMON_OBJS)
BENCHMARK_OBJS = $(BUILD_DIR)/benchmark.o $(COMMON_OBJS)

all: $(BIN_DIR)/arcnet_server $(BIN_DIR)/arcnet_client $(BIN_DIR)/benchmark

$(BIN_DIR)/arcnet_server: $(SERVER_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES) $(LDFLAGS)

$(BIN_DIR)/arcnet_client: $(CLIENT_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES) $(LDFLAGS)

$(BIN_DIR)/benchmark: $(BENCHMARK_OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(INCLUDES) $(LDFLAGS)

# 各模块编译规则
$(BUILD_DIR)/buffer_%.o: buffer/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/net_%.o: net/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/protocol_%.o: protocol/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/storage_manager_%.o: storage_manager/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/mysql_%.o: mysql/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/MatchManager_%.o: MatchManager/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/concurrency_%.o: concurrency/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

$(BUILD_DIR)/Room_%.o: Room/%.cpp
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