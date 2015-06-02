CC=g++
CFLAGS = -g

all: router

router: router.o
	$(CC) -o router router.o $(LIBS)

router.o: router.cpp

clean:
	rm -f router router.o
