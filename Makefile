CC=g++
CFLAGS = -g

all: my-router

router: my-router.o
	$(CC) -o my-router my-router.o $(LIBS)

my-router.o: my-router.cpp

clean:
	rm -f my-router my-router.o
