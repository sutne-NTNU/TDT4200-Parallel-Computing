BUILD_DIR = build
create_folder := $(shell mkdir -p $(BUILD_DIR))

CC  = nvcc
C	= cu
H	= h

MAKEFILE_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ROOT_DIR := $(shell dirname $(MAKEFILE_DIR))

CFLAGS 	= -g -I${ROOT_DIR}/includes -I${MAKEFILE_DIR}/libs
LFLAGS  = -g


ifeq ("$(shell uname)", "Darwin")
    LDFLAGS     = -framework Foundation -framework GLUT -framework OpenGL -lm
else
  ifeq ("$(shell uname)", "Linux")
    LDFLAGS     = -L /usr/lib64/ -lglut -lGL -lm -lGLU
  endif
endif


PROJECT = morph

SRC_FILES = $(wildcard src/*.${C})
OBJ_FILES = $(patsubst src/%.${C}, build/%.o, ${SRC_FILES})

${PROJECT}: build/${PROJECT}.o $(OBJ_FILES)
		$(CC) -o ${PROJECT} ${CFLAGS} ${LDFLAGS} ${OBJ_FILES}

build/%.o: src/%.${C}
		$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f *.o *~ core.* *.h.gch output/images/*.png morph
	rm -rf build

run:
	./scripts/run.sh 2

gif:
	./scripts/generate_gif.sh