SOURCE = array_sum.cpp
PROGRAM_NAME = array_sum
TEST_GENERATOR = input_generator.cpp
CC = g++
CFLAGS = -std=c++14 -lpthread

${PROGRAM_NAME} : ${SOURCE}
	${CC} -o ${PROGRAM_NAME} ${SOURCE} ${CFLAGS}
	@echo "======================================================================================="
	
run : ${PROGRAM_NAME}
	./${PROGRAM_NAME}
	python3 plot.py < data.txt
	\rm data.txt
	@echo "======================================================================================="

testgen :
	g++ {TEST_GENERATOR}
	./a.out
	\rm a.out

test : ${PROGRAM_NAME}
	./${PROGRAM_NAME} input1.txt
	python3 plot.py < data.txt
	@echo "======================================================================================="
	./${PROGRAM_NAME} input2.txt
	python3 plot.py < data.txt
	@echo "======================================================================================="
	./${PROGRAM_NAME} input3.txt
	python3 plot.py < data.txt
	@echo "======================================================================================="
	./${PROGRAM_NAME} input4.txt
	python3 plot.py < data.txt
	@echo "======================================================================================="
	./${PROGRAM_NAME} input5.txt
	python3 plot.py < data.txt
	@echo "======================================================================================="
	\rm data.txt
	@echo "======================================================================================="

test1 : ${PROGRAM_NAME}
	./${PROGRAM_NAME} input1.txt
	python3 plot.py < data.txt
	\rm data.txt
	@echo "======================================================================================="

test2 : ${PROGRAM_NAME}
	./${PROGRAM_NAME} input2.txt
	python3 plot.py < data.txt
	\rm data.txt
	@echo "======================================================================================="

test3 : ${PROGRAM_NAME}
	./${PROGRAM_NAME} input3.txt
	python3 plot.py < data.txt
	\rm data.txt
	@echo "======================================================================================="

test4 : ${PROGRAM_NAME}
	./${PROGRAM_NAME} input4.txt
	python3 plot.py < data.txt
	\rm data.txt
	@echo "======================================================================================="

test5 : ${PROGRAM_NAME}
	./${PROGRAM_NAME} input5.txt
	python3 plot.py < data.txt
	\rm data.txt
	@echo "======================================================================================="

clean :
	\rm ${PROGRAM_NAME} *.png *.out