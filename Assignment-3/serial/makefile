SOURCE = kilo.c
TARGET = kilo
CC = gcc
FLAGS = -Wall -Wextra -pedantic -std=c11

$(TARGET): $(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(FLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	\rm ${TARGET}