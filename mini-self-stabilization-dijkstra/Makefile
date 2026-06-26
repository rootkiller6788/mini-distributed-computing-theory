# Makefile for mini-self-stabilization-dijkstra
# Dijkstra's Self-Stabilizing Token Ring
#
# Reference:
#   Dijkstra, E.W. "Self-stabilizing Systems in Spite of Distributed Control."
#   Communications of the ACM, 17(11):643-644, November 1974.

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -I./include
LDFLAGS = -lm

# Source files
SRC = src/dijkstra_ring.c \
      src/state_machine.c \
      src/scheduler.c \
      src/convergence.c \
      src/self_stabilization.c \
      src/fault_injection.c \
      src/applications.c

# Object files
OBJ = $(SRC:.c=.o)

# Test files
TEST_SRC = tests/test_dijkstra.c
TEST_BIN = test_dijkstra

# Example files
EXAMPLE_SRC = examples/token_ring_demo.c \
              examples/mutual_exclusion.c \
              examples/fault_recovery.c
EXAMPLE_BIN = $(EXAMPLE_SRC:.c=.exe)

# Demo files
DEMO_SRC = demos/visualizer.c
DEMO_BIN = $(DEMO_SRC:.c=.exe)

# Bench files
BENCH_SRC = benches/convergence_bench.c
BENCH_BIN = $(BENCH_SRC:.c=.exe)

.PHONY: all clean test examples demos benches

all: test examples demos benches

# Compile shared objects (used by tests, examples, demos, benches)
libdijkstra.a: $(OBJ)
	ar rcs $@ $(OBJ)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test binary
$(TEST_BIN): $(OBJ) $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) $(OBJ) $(LDFLAGS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

# Example binaries
examples/%.exe: examples/%.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

examples: $(EXAMPLE_BIN)
	@echo "=== Running token_ring_demo ==="
	./examples/token_ring_demo.exe
	@echo ""
	@echo "=== Running mutual_exclusion ==="
	./examples/mutual_exclusion.exe
	@echo ""
	@echo "=== Running fault_recovery ==="
	./examples/fault_recovery.exe

# Demo binary
demos/%.exe: demos/%.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

demos: $(DEMO_BIN)
	./demos/visualizer.exe

# Bench binary
benches/%.exe: benches/%.c $(OBJ)
	$(CC) $(CFLAGS) $< $(OBJ) $(LDFLAGS) -o $@

benches: $(BENCH_BIN)
	./benches/convergence_bench.exe

clean:
	rm -f src/*.o
	rm -f libdijkstra.a
	rm -f $(TEST_BIN)
	rm -f $(EXAMPLE_BIN)
	rm -f $(DEMO_BIN)
	rm -f $(BENCH_BIN)

# Line count statistics (for SKILL.md compliance: include/ + src/ ≥ 3000)
.PHONY: count
count:
	@echo "include/ lines:"
	@find include -name "*.h" -exec cat {} + | wc -l
	@echo "src/ lines (C only):"
	@find src -name "*.c" -exec cat {} + | wc -l
	@echo "src/ lines (Lean 4):"
	@find src -name "*.lean" -exec cat {} + | wc -l
	@echo "include/ + src/ (C) total:"
	@bash -c 'cat include/*.h src/*.c 2>/dev/null | wc -l'
