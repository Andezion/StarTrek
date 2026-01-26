CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

CXXFLAGS += $(shell pkg-config --cflags ixwebsocket 2>/dev/null)
LDFLAGS += $(shell pkg-config --libs ixwebsocket 2>/dev/null || echo "-lixwebsocket -lz")

TARGET = cosmodrom-visualizer

SOURCES = \
	src/main.cpp \
	src/app.cpp \
	src/network/ws_client.cpp \
	src/network/protocol.cpp \
	src/data/simulation_state.cpp \
	src/data/rocket_data.cpp \
	src/scene/scene.cpp \
	src/scene/camera.cpp \
	src/ui/ui_manager.cpp

OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

src/main.o: include/app.hpp
src/app.o: include/app.hpp include/scene/scene.hpp include/network/ws_client.hpp include/data/simulation_state.hpp include/ui/ui_manager.hpp
src/network/ws_client.o: include/network/ws_client.hpp include/network/protocol.hpp
src/network/protocol.o: include/network/protocol.hpp
src/data/simulation_state.o: include/data/simulation_state.hpp include/data/rocket_data.hpp
src/data/rocket_data.o: include/data/rocket_data.hpp include/network/protocol.hpp
src/scene/scene.o: include/scene/scene.hpp include/scene/camera.hpp include/data/simulation_state.hpp
src/scene/camera.o: include/scene/camera.hpp
src/ui/ui_manager.o: include/ui/ui_manager.hpp include/data/simulation_state.hpp include/raygui.h
