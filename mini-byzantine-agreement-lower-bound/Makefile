# Makefile for mini-byzantine-agreement-lower-bound
# Part of mini-distributed-computing-theory

CC      := gcc
LDFLAGS := -lm

SRC_DIR   := src
INC_DIR   := include
TEST_DIR  := tests
EX_DIR    := examples

INCLUDES  := -I$(INC_DIR)
CFLAGS    := -std=c11 -Wall -Wextra -pedantic -O2 $(INCLUDES)

# Source files
SRCS := $(SRC_DIR)/byzantine_agreement_core.c \
        $(SRC_DIR)/eig_implementation.c \
        $(SRC_DIR)/msg_automaton.c \
        $(SRC_DIR)/consensus_types.c \
        $(SRC_DIR)/phase_protocols.c \
        $(SRC_DIR)/blockchain_consensus.c

OBJS := $(SRCS:.c=.o)

# Test
TEST_SRC  := $(TEST_DIR)/test_byzantine_agreement.c
TEST_BIN  := build/test_runner

# Examples
EX1_SRC := $(EX_DIR)/example_n3_f1_impossible.c
EX2_SRC := $(EX_DIR)/example_eig_n4_f1.c
EX3_SRC := $(EX_DIR)/example_blockchain_consensus.c
EX1_BIN := build/example_n3_f1_impossible
EX2_BIN := build/example_eig_n4_f1
EX3_BIN := build/example_blockchain_consensus

.PHONY: all clean test examples

all: test examples

build:
	mkdir -p build

$(OBJS): build

# Compile object files
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test binary
$(TEST_BIN): $(OBJS) $(TEST_SRC) | build
	$(CC) $(CFLAGS) $(TEST_SRC) $(OBJS) -o $@ $(LDFLAGS)

# Example binaries
$(EX1_BIN): $(OBJS) $(EX1_SRC) | build
	$(CC) $(CFLAGS) $(EX1_SRC) $(OBJS) -o $@ $(LDFLAGS)

$(EX2_BIN): $(OBJS) $(EX2_SRC) | build
	$(CC) $(CFLAGS) $(EX2_SRC) $(OBJS) -o $@ $(LDFLAGS)

$(EX3_BIN): $(OBJS) $(EX3_SRC) | build
	$(CC) $(CFLAGS) $(EX3_SRC) $(OBJS) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

examples: $(EX1_BIN) $(EX2_BIN) $(EX3_BIN)

run-examples: examples
	@echo "=== Example 1: N=3, f=1 Impossibility ==="
	./$(EX1_BIN)
	@echo ""
	@echo "=== Example 2: EIG with N=4, f=1 ==="
	./$(EX2_BIN)
	@echo ""
	@echo "=== Example 3: Blockchain Consensus ==="
	./$(EX3_BIN)

clean:
	rm -rf build $(OBJS)

# Lean 4 formalization (check only, doesn't block C build)
lean-check:
	@echo "Lean 4 formalization present in src/byzantine_agreement.lean"
	@echo "To verify: lake build (requires Lean 4 toolchain)"
