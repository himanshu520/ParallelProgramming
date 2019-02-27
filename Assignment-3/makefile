SOURCE = kilo.cpp
TARGET = kilo
CC = g++
FLAGS = -Wall -Wextra -pedantic -std=c++14

$(TARGET): $(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(FLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	\rm kilo