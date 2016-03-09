MAKEFLAGS += --silent
CC = gcc
CFLAGS = -std=gnu99
LDFLAGS = 

CFLAGS += `pkg-config --cflags 'libprotobuf-c >= 1.0.0'`
LDFLAGS += `pkg-config --libs 'libprotobuf-c >= 1.0.0'`

PROTOS_SRC = protos
PROTOS = $(shell find $(PROTOS_SRC) -name '*.proto')

# Server-Specific Functions #
SERVER_SRC = server
SERVER_EXECUTABLE = $(SERVER_SRC)/pktgen_server
SERVER_SOURCES = $(shell find $(SERVER_SRC) -name '*.c')
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)

SERVER_PROTOC = protoc-c
SERVER_PFLAGS = --c_out=$(SERVER_SRC) --proto_path=$(PROTOS_SRC)

server: $(SERVER_SOURCES) $(SERVER_EXECUTABLE)

$(SERVER_EXECUTABLE): $(SERVER_SOURCES) server_protos
	$(CC) -o $@ $(SERVER_SOURCES) $(CFLAGS) $(LDFLAGS)

server_protos:
	$(SERVER_PROTOC) $(SERVER_PFLAGS) $(PROTOS)

server_clean:
	rm -f $(SERVER_OBJECTS) $(SERVER_EXECUTABLE)
	rm -f $(shell find $(SERVER_SRC) -name '*.pb-c*')

# Scheduler-Specific Functions #
SCHEDULER_SRC = scheduler
SCHEDULER_PROTOC = protoc
SCHEDULER_PFLAGS = --python_out=$(SCHEDULER_SRC) --proto_path=$(PROTOS_SRC)

scheduler: scheduler_protos

scheduler_protos:
	$(SCHEDULER_PROTOC) $(SCHEDULER_PFLAGS) $(PROTOS)

scheduler_clean:
	rm -f $(shell find $(SCHEDULER_SRC) -name '*_pb2*')

# General Functions #
all: server scheduler

clean: server_clean scheduler_clean

