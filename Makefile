CC = gcc
CFLAGS = -std=c17 -O3 -Wall -Wextra -pthread
LDFLAGS = -pthread
SRC = src/main.c src/scanner.c src/worker.c src/queue.c
OBJ = $(SRC:.c=.o)

# --- Cross-Platform Platform Detection ---
ifdef OS
   # Windows environment detector
   TARGET = metarip.exe
   TEST_BIN = tests/test_runner.exe
   RM = del /Q /F
   FIX_PATH = $(subst /,\,$1)
else
   # Linux / macOS environment detector
   TARGET = metarip
   TEST_BIN = tests/test_runner
   RM = rm -f
   FIX_PATH = $1
endif

TEST_SRC = tests/test_runner.c

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
ifdef OS
	$(RM) src\*.o 2>NUL || exit 0
	$(RM) $(TARGET) 2>NUL || exit 0
	$(RM) tests\test_runner.exe 2>NUL || exit 0
else
	$(RM) $(OBJ) $(TARGET) $(TEST_BIN)
endif

.PHONY: all test clean