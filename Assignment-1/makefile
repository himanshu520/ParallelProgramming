SOURCE = hare_turtle_thread.cpp
PROGRAM_NAME = hare_turtle_thread
CC = g++
CFLAGS = -std=c++11 -lpthread
OBJS = hare_turtle_thread.o

run : ${PROGRAM_NAME}
	./${PROGRAM_NAME}
	
${PROGRAM_NAME} : ${OBJS}
	${CC} -o ${PROGRAM_NAME} ${OBJS} ${CFLAGS}

${OBJS} : ${SOURCE}
	${CC} -c ${SOURCE} ${CFLAGS}

clean :
	\rm ${PROGRAM_NAME} *.o