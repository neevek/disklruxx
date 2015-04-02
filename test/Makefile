CC=g++
CFLAGS=-I.. -std=c++11 -Wall -DLOG_DEBUG -c
BIN=testlruxx

all: ${BIN}

${BIN}: test.o disk_cache.o file_util.o sha1.o
	${CC} test.o disk_cache.o file_util.o sha1.o -o ${BIN}

test.o: test.cc
	${CC} ${CFLAGS} -o test.o test.cc

disk_cache.o: ../lru/disk_cache.cc
	${CC} ${CFLAGS} -o disk_cache.o ../lru/disk_cache.cc

file_util.o: ../common/file_util.cc
	${CC} ${CFLAGS} -o file_util.o ../common/file_util.cc

sha1.o: ../common/sha1/sha1.cpp
	${CC} ${CFLAGS} -o sha1.o ../common/sha1/sha1.cpp

clean: 
	rm -f *.o ${BIN}
