# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall -fcommon -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS = -lpthread

# Directories
SHARED_DIR = shared
SERVER_DIR = server_side
CLIENT_DIR = client_side

# Object files for server and client
SERVER_OBJS = $(SERVER_DIR)/server.o $(SERVER_DIR)/game.o $(SERVER_DIR)/timer.o $(SHARED_DIR)/utils.o $(SHARED_DIR)/console.o
CLIENT_OBJS = $(CLIENT_DIR)/client.o $(SHARED_DIR)/utils.o $(SHARED_DIR)/console.o

# Targets
all: server client

# Server executable
server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS) $(LDFLAGS)

# Client executable
client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS) $(LDFLAGS)

# Compile server sources
$(SERVER_DIR)/server.o: $(SERVER_DIR)/server.c $(SERVER_DIR)/server.h $(SHARED_DIR)/constants.h $(SHARED_DIR)/utils.h $(SHARED_DIR)/console.h $(SERVER_DIR)/timer.h
	$(CC) $(CFLAGS) -c $(SERVER_DIR)/server.c -o $(SERVER_DIR)/server.o

$(SERVER_DIR)/game.o: $(SERVER_DIR)/game.c $(SERVER_DIR)/game.h $(SHARED_DIR)/constants.h
	$(CC) $(CFLAGS) -c $(SERVER_DIR)/game.c -o $(SERVER_DIR)/game.o

$(SERVER_DIR)/timer.o: $(SERVER_DIR)/timer.c $(SERVER_DIR)/timer.h $(SERVER_DIR)/server.h
	$(CC) $(CFLAGS) -c $(SERVER_DIR)/timer.c -o $(SERVER_DIR)/timer.o

# Compile client sources
$(CLIENT_DIR)/client.o: $(CLIENT_DIR)/client.c $(CLIENT_DIR)/client.h $(SHARED_DIR)/constants.h $(SHARED_DIR)/utils.h $(SHARED_DIR)/console.h
	$(CC) $(CFLAGS) -c $(CLIENT_DIR)/client.c -o $(CLIENT_DIR)/client.o

# Compile shared sources
$(SHARED_DIR)/utils.o: $(SHARED_DIR)/utils.c $(SHARED_DIR)/utils.h
	$(CC) $(CFLAGS) -c $(SHARED_DIR)/utils.c -o $(SHARED_DIR)/utils.o

$(SHARED_DIR)/console.o: $(SHARED_DIR)/console.c $(SHARED_DIR)/console.h
	$(CC) $(CFLAGS) -c $(SHARED_DIR)/console.c -o $(SHARED_DIR)/console.o

# Clean build
clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) server client