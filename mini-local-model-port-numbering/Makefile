# Makefile for mini-local-model-port-numbering
# LOCAL Model of Distributed Computing with Port Numbering
#
# Target: make test — compiles and runs the test suite
# Target: make all  — compiles all examples and test

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2 -g
INCLUDES = -Iinclude
LDFLAGS  = -lm

SRC_DIR  = src
INC_DIR  = include
TST_DIR  = tests
EXA_DIR  = examples

# Source files
SRCS     = $(SRC_DIR)/local_model.c \
           $(SRC_DIR)/port_numbering.c \
           $(SRC_DIR)/graph_problems.c \
           $(SRC_DIR)/algorithms.c \
           $(SRC_DIR)/lower_bounds.c

OBJS     = $(SRCS:.c=.o)

# Test
TEST_SRC = $(TST_DIR)/test_local_model.c
TEST_BIN = build/test_local_model

# Examples
EX1_SRC  = $(EXA_DIR)/example_ring_coloring.c
EX1_BIN  = build/example_ring_coloring
EX2_SRC  = $(EXA_DIR)/example_mis_random.c
EX2_BIN  = build/example_mis_random
EX3_SRC  = $(EXA_DIR)/example_leader_bfs.c
EX3_BIN  = build/example_leader_bfs
EX4_SRC  = $(EXA_DIR)/example_lower_bounds.c
EX4_BIN  = build/example_lower_bounds

.PHONY: all test clean examples

all: build $(TEST_BIN) $(EX1_BIN) $(EX2_BIN) $(EX3_BIN) $(EX4_BIN)

build:
	mkdir -p build

# Object files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/*.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test binary
$(TEST_BIN): $(OBJS) $(TEST_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(TEST_SRC) $(OBJS) $(LDFLAGS)

# Example binaries
$(EX1_BIN): $(OBJS) $(EX1_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(EX1_SRC) $(OBJS) $(LDFLAGS)

$(EX2_BIN): $(OBJS) $(EX2_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(EX2_SRC) $(OBJS) $(LDFLAGS)

$(EX3_BIN): $(OBJS) $(EX3_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(EX3_SRC) $(OBJS) $(LDFLAGS)

$(EX4_BIN): $(OBJS) $(EX4_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(EX4_SRC) $(OBJS) $(LDFLAGS)

# Test target
test: $(TEST_BIN)
	@echo "Running test suite..."
	./$(TEST_BIN)

# Run all examples
examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN) $(EX4_BIN)
	@echo "=== Example 1: Ring Coloring ==="
	./$(EX1_BIN)
	@echo ""
	@echo "=== Example 2: MIS on Random Graph ==="
	./$(EX2_BIN)
	@echo ""
	@echo "=== Example 3: Leader Election + BFS ==="
	./$(EX3_BIN)
	@echo ""
	@echo "=== Example 4: Lower Bounds ==="
	./$(EX4_BIN)

# Clean
clean:
	rm -rf build $(SRC_DIR)/*.o

# Help
help:
	@echo "Targets:"
	@echo "  make all      - Build test and all examples"
	@echo "  make test     - Build and run test suite"
	@echo "  make examples - Build and run all examples"
	@echo "  make clean    - Remove build artifacts"
