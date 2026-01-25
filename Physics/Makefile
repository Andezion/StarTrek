CC = gcc
CFLAGS = -Wall -Wextra -O3 -fPIC -std=c11
LDFLAGS = -shared -lm

TARGET = librocket_physics.so
SOURCES = rocket_physics.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = rocket_physics.h

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

test: $(TARGET)
	@echo "Building test program..."
	$(CC) $(CFLAGS) -L. -lrocket_physics test_physics.c -o test_physics -lm
	@echo "Running test..."
	LD_LIBRARY_PATH=. ./test_physics

install: $(TARGET)
	cp $(TARGET) /usr/local/lib/
	cp $(HEADERS) /usr/local/include/
	ldconfig

.SUFFIXES: .c .o
