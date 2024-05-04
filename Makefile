CFLAGS=-Wall -I/usr/include/gpiod
LDFLAGS=-lwiringPi -lpaho-mqtt3c

all: zeroct

zeroct: main.o
	mkdir -p build
	$(CC) -o build/zeroct main.o $(LDFLAGS)

main.o: src/main.c
	bear -- $(CC) -c $(CFLAGS) src/main.c -o main.o

deploy:
	git pull
	$(MAKE) zeroct

clean:
	rm -rf *.o
