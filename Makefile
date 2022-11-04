HEADERS=include
SRC=src
TEST=tests/parser_test

CFLAGS= -g -I./include --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L
LDFLAGS = -lpthread -pthread

CLIENT_C_FILES = src/client
SERVER_C_FILES =  src/netutils.o src/selector.o src/stm.o src/buffer.o src/args.o src/main.o

selector.o:			selector.h
stm.o:				stm.h
buffer.o:			buffer.h
args.o:				args.h
main.o:				main.h
netutils.o:			netutils.h

all: server client

server: $(CLIENT_C_FILES)
	mkdir -p bin
	$(CC) $(CFLAGS)  $(CLIENT_C_FILES) -o bin/client

client:$(SERVER_C_FILES)
	mkdir -p bin
	$(CC) $(CFLAGS) $(LDFLAGS) $(SERVER_C_FILES) -o bin/server

clean:
	rm -fr ./bin/*
	rm -f ./src/*.o
	rm -f ./src/client/*.o
