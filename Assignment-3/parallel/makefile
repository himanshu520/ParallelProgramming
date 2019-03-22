SOURCE = kilo.c
TARGET = kilo
CC = gcc
FLAGS = -Wall -Wextra -pedantic -std=c11 -lpthread

$(TARGET): $(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(FLAGS)

run: $(TARGET)
	./$(TARGET)

test1: $(TARGET)
	./$(TARGET) test1.c

test2: $(TARGET)
	./$(TARGET) test2.c

test3: $(TARGET)
	./$(TARGET) test3.c

test4: $(TARGET)
	./$(TARGET) test4.txt

test5: $(TARGET)
	./$(TARGET) test5.txt

test6: $(TARGET)
	./$(TARGET) test6.txt

testgen: testgen.cpp
	g++ testgen.cpp -o testgen
	./testgen 
	\rm testgen
	
clean:
	\rm ${TARGET} test1.c test2.c test3.c test4.txt test5.txt test6.txt