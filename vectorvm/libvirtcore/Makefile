CC = gcc
AR = ar
CFLAGS = -O2 -Wall -Wextra -Wpedantic -std=c2x

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)
LIB := libvirtcore.a

all: $(LIB)

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB)
