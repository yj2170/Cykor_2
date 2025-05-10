CC = gcc
CFLAGS = -Wall -g

SRC = main.c prompt.c parser.c executor.c builtin.c utils.c

TARGET = linux_2025350202_전유정

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)