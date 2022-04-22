CFLAGS = -Wall -Wshadow -O3 -g 
LDLIBS = -lm -fopenmp -lopenblas -I/usr/local/opt/openblas/include -L/usr/local/opt/openblas/lib

# CC=gcc
CC=gcc-11

all: main

clean:
	$(RM) main

run:
	make && ./main

.PHONY: clean
