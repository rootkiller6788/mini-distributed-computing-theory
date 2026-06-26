CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include -Wno-unused-function -Wno-unused-parameter
AR = ar

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
LIB = build/libcoloringmis.a

EXAMPLES_SRC = $(wildcard examples/*.c)
EXAMPLES_BIN = $(patsubst examples/%.c,build/example_%,$(EXAMPLES_SRC))

TESTS_SRC = $(wildcard tests/*.c)
TESTS_BIN = $(patsubst tests/%.c,build/test_%,$(TESTS_SRC))

.PHONY: all clean test examples

all: $(LIB) test examples

$(LIB): $(OBJ)
	@mkdir -p build
	$(AR) rcs $@ $^

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/test_%: tests/%.c $(LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lcoloringmis -lm

build/example_%: examples/%.c $(LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lcoloringmis -lm

test: $(TESTS_BIN)
	@for t in $(TESTS_BIN); do echo "Running $$t..."; ./$$t || exit 1; done
	@echo "All tests passed."

examples: $(EXAMPLES_BIN)

clean:
	rm -rf build
