CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -pthread
LDFLAGS = -pthread
TARGET  = vm
SRC     = vm.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o correct.txt
