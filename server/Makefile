CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -O2 -g
LDFLAGS = -lpthread

all: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o aesdsocket $(LDFLAGS)

.PHONY: clean

clean:
	rm -rf aesdsocket
