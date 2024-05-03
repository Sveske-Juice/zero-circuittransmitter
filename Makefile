SOURCE:=$(shell find $(pwd) -name *.c)

# Include all source files
all: $(SOURCE)
	mkdir -p build/ && \
	bear -- gcc -o ./build/zeroct -Wall $(SOURCE) -lgpiod


# Run this to deploy on RPi
deploy:
	git pull
	$(MAKE) all
