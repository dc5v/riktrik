CC = gcc
CFLAGS =  -g
LDFLAGS = -luuid -ljson-c -lpthread -lm -lz -lyaml

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

TARGET = $(BIN_DIR)/tictacdb

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(BIN_DIR) $(OBJ_DIR) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

all: $(TARGET)

.PHONY: all clean
