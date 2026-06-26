# Makefile for mini-consensus-fault-tolerance
# Consensus and Fault Tolerance in Distributed Computing
#
# Reference: Lamport (1982, 1998, 2001), Fischer, Lynch, Paterson (1985),
#            Ongaro & Ousterhout (2014), Castro & Liskov (1999),
#            Nakamoto (2008), Malkhi & Reiter (1998)

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2
INCLUDES = -Iinclude
LDFLAGS  = -lm

# Source files
SRCS = src/consensus_types.c \
       src/consensus_model.c \
       src/paxos.c \
       src/raft.c \
       src/pbft.c \
       src/quorum.c \
       src/nakamoto.c \
       src/byzantine_agreement.c

# Object files
OBJS = $(SRCS:.c=.o)

# Test
TEST_SRC = tests/test_consensus.c
TEST_BIN = test_consensus

# Examples
EXAMPLES = examples/example_paxos \
           examples/example_raft \
           examples/example_nakamoto

.PHONY: all clean test examples

all: test examples

# Build library objects
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build and run test
$(TEST_BIN): $(OBJS) $(TEST_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) $(TEST_SRC) $(OBJS) -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

# Build examples
examples/example_paxos: $(OBJS) examples/example_paxos.c
	$(CC) $(CFLAGS) $(INCLUDES) examples/example_paxos.c $(OBJS) -o $@ $(LDFLAGS)

examples/example_raft: $(OBJS) examples/example_raft.c
	$(CC) $(CFLAGS) $(INCLUDES) examples/example_raft.c $(OBJS) -o $@ $(LDFLAGS)

examples/example_nakamoto: $(OBJS) examples/example_nakamoto.c
	$(CC) $(CFLAGS) $(INCLUDES) examples/example_nakamoto.c $(OBJS) -o $@ $(LDFLAGS)

examples: $(EXAMPLES)

# Run all examples
run-examples: examples
	@echo "=== Running example_paxos ==="
	./examples/example_paxos
	@echo ""
	@echo "=== Running example_raft ==="
	./examples/example_raft
	@echo ""
	@echo "=== Running example_nakamoto ==="
	./examples/example_nakamoto

clean:
	rm -f $(OBJS) $(TEST_BIN) $(EXAMPLES)
	rm -f src/*.o
