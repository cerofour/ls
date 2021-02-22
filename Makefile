CC = cc
# CFLAGS = -O3 -march=native -Wall -Wextra -D _POSIX_C_SOURCE=200809L -std=c11 -pedantic
CFLAGS = -g -Wall -Wextra -D _POSIX_C_SOURCE=200809L -std=c11 -pedantic

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)

cls: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm $(OBJ) cls
