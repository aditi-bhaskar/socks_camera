CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

BUILD_DIR = build

TARGET = $(BUILD_DIR)/image_processor

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/image.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/main.o: main.cpp image.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c main.cpp -o $@

$(BUILD_DIR)/image.o: image.cpp image.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c image.cpp -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

