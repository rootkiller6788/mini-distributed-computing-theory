CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
AR = ar

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,build/%.o,$(SRC))
LIB = build/libflp.a

EXAMPLES_SRC = $(wildcard examples/*.c)
EXAMPLES_BIN = $(patsubst examples/%.c,build/example_%,$(EXAMPLES_SRC))

BENCH_SRC = $(wildcard benches/*.c)
BENCH_BIN = $(patsubst benches/%.c,build/bench_%,$(BENCH_SRC))

DEMOS_SRC = $(wildcard demos/*.c)
DEMOS_BIN = $(patsubst demos/%.c,build/demo_%,$(DEMOS_SRC))

all: $(LIB) tests/run_tests

$(LIB): $(OBJ)
	@mkdir -p build
	$(AR) rcs $@ $^

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

tests/run_tests: tests/test_flp.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lflp -lm

test: tests/run_tests
	./tests/run_tests

examples: $(EXAMPLES_BIN)

build/example_%: examples/%.c $(LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lflp -lm

benches: $(BENCH_BIN)

build/bench_%: benches/%.c $(LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lflp -lm

demos: $(DEMOS_BIN)

build/demo_%: demos/%.c $(LIB)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< -Lbuild -lflp -lm

clean:
	rm -rf build/ tests/run_tests tests/run_tests.exe

distclean: clean
	rm -f tests/*.exe examples/*.exe

.PHONY: all test clean distclean examples benches demos
