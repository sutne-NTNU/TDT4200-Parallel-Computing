CFLAGS = -Wall -Wshadow -O3 -g #-march=native
LDLIBS = -lm -lopenblas -L/usr/local/opt/openblas/lib
CPPFLAGS = -I/usr/local/opt/openblas/include
CC=gcc

all: check example4

sigmoid: CFLAGS += -Dgenann_act=genann_act_sigmoid_cached
sigmoid: all

threshold: CFLAGS += -Dgenann_act=genann_act_threshold
threshold: all

linear: CFLAGS += -Dgenann_act=genann_act_linear
linear: all

test: test.o genann.o

check: test
	./$^

example4: example4.o genann.o

clean:
	$(RM) *.o
	$(RM) test example4 *.exe
	$(RM) persist.txt

.PHONY: sigmoid threshold linear clean