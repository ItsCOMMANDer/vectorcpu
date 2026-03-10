CC := gcc
CFLAGS := -I libvirtcore -Wall -Wextra -O2
SRC_DIR := src
OBJ_DIR := build
TARGET := vectorvm
LIB := libvirtcore/libvirtcore.a

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS) $(LIB)
	$(CC) $(OBJS) $(LIB) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
