CFLAGS= -g -I./include --std=c11 -pedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L
# CFLAGS= -g -I./include --std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L

CLIENT_C_FILES = src/client.o src/tcpClientUtil.o src/util.o src/logger.o

LDFLAGS = -lpthread -pthread
SERVER_C_FILES =   src/selector.o src/stm.o src/new_parser.o src/authentication.o src/socks5nio.o src/management.o src/management_request.o src/connecting.o src/hello.o src/request_parser.o src/resolv.o src/request.o src/copy.o src/debug.o src/address_utils.o src/socket_utils.o src/buffer.o src/args.o src/main.o

management.o: management.h
management_request.o: management_request.h
authentication.o:	authentication.h
new_parser.o:		new_parser.h
request_parser.o:	request_parser.h
resolv.o:			resolv.h
client.o:			client.h
tcpClientUtil.o:	tcpClientUtil.h
util.o:				util.h
logger.o:			logger.h
selector.o:			selector.h
stm.o:				stm.h
socks5nio.o:		socks5nio.h
connecting.o:		connecting.h
hello.o:			hello.h
request.o:			request.h
copy.o:				copy.h
debug.o:			debug.h
address_utils.o:	address_utils.h
socket_utils.o:		socket_utils.h
buffer.o:			buffer.h
args.o:				args.h
main.o:				main.h


all: server  client

client: $(CLIENT_C_FILES)
	mkdir -p bin
	$(CC) $(CFLAGS)  $(CLIENT_C_FILES) -o bin/client

server: $(SERVER_C_FILES)
	mkdir -p bin
	$(CC) $(CFLAGS) $(LDFLAGS) $(SERVER_C_FILES) -o bin/server

clean:
	rm -fr ./bin/*
	rm -f ./src/*.o


.PHONY: all clean client server