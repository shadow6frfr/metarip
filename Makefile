CC = gcc
CFLAGS = -std=c17 -O3 -Wall -Wextra -pthread
LDFLAGS = -pthread
SRC = src/main.c src/scanner.c src/worker.c src/queue.c
OBJ = $(SRC:.c=.o)
TARGET = climetadata
TEST_SRC = tests/test_runner.c
TEST_BIN = tests/test_runner

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(TEST_BIN): $(TEST_SRC)
	$(CC) $(CFLAGS) -Iinclude -o $@ $^

test: all $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(OBJ) $(TARGET) $(TEST_BIN)

.PHONY: all test clean
