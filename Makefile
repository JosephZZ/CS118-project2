CC=gcc
CFLAGS = -g

all: router

router: router.o
	$(CC) -o router router.o $(LIBS)

router.o: router.c

clean:
	rm -f router router.o
