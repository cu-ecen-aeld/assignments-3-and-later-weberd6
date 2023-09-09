CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -O2 -g

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket

.PHONY: clean

clean:
	rm -rf aesdsocket
