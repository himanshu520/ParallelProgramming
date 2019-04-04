SOURCE = kilo.c
TARGET = kilo
CC = gcc
FLAGS = -Wall -Wextra -pedantic -std=c11

$(TARGET): $(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(FLAGS)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	./$(TARGET) test1.c
	./$(TARGET) test2.c
	./$(TARGET) test3.c
	./$(TARGET) test4.c
	./$(TARGET) test5.txt
	./$(TARGET) test6.txt
	./$(TARGET) test7.txt
	./$(TARGET) test8.txt

test1: $(TARGET)
	./$(TARGET) test1.c

test2: $(TARGET)
	./$(TARGET) test2.c

test3: $(TARGET)
	./$(TARGET) test3.c

test4: $(TARGET)
	./$(TARGET) test4.c

test5: $(TARGET)
	./$(TARGET) test5.txt

test6: $(TARGET)
	./$(TARGET) test6.txt

test7: $(TARGET)
	./$(TARGET) test7.txt

test8: $(TARGET)
	./$(TARGET) test8.txt

testgen: testgen.cpp
	g++ testgen.cpp -o testgen
	./testgen 
	\rm testgen
	
clean:
	\rm ${TARGET} test1.c test2.c test3.c test4.c test5.txt test6.txt test7.txt test8.txt

cleanall:
	\rm ${TARGET} test1.c test2.c test3.c test4.c test5.txt test6.txt test7.txt test8.txt log.txt