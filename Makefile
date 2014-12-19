CXX = gcc
FLAGS = -ggdb -Wall

main: main.c
	${CXX} ${FLAGS} -w -DDEBUG=1 -o file_cache file_cache.h file_cache.c main.c -lpthread

clean:
	rm -f file_cache
