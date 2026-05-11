
CXX = g++
CXXFLAGS = -O3 -Wall -Iinclude -std=c++17
CXXFLAGS = -O3 -Wall -Iinclude `pkg-config --cflags rtaudio rtmidi`
LIBS = `pkg-config --libs rtaudio rtmidi` -lpthread
LIBS = -lrtmidi -lrtaudio -lpthread -lasound

SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = $(BIN_DIR)/synth_exec


all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@


clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean
