# Makefile — mini-synchronizer-alpha-beta-gamma
# Reference: Awerbuch (1985) "Complexity of Network Synchronization" JACM 32(4)
#
# Targets:
#   make          Build library and all programs
#   make test     Build and run test suite
#   make examples Build example programs
#   make clean    Remove build artifacts
#   make all      Build everything

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99 -pedantic -O2 -g
LDFLAGS  = -lm

INCDIR   = include
SRCDIR   = src
TESTDIR  = tests
EXAMDIR  = examples
BUILDDIR = build

# Core source files (library)
SRCS = $(SRCDIR)/network.c \
       $(SRCDIR)/alpha_synchronizer.c \
       $(SRCDIR)/beta_synchronizer.c \
       $(SRCDIR)/gamma_synchronizer.c \
       $(SRCDIR)/distributed_algorithms.c

OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Test files
TEST_SRC = $(TESTDIR)/test_synchronizers.c
TEST_BIN = $(BUILDDIR)/test_synchronizers

# Example files
EXAMPLE_SRCS = $(EXAMDIR)/example_bfs.c \
               $(EXAMDIR)/example_leader_election.c \
               $(EXAMDIR)/example_sensor_network.c \
               $(EXAMDIR)/example_complexity_comparison.c

EXAMPLE_BINS = $(EXAMPLE_SRCS:$(EXAMDIR)/%.c=$(BUILDDIR)/%)

# Library
LIB = $(BUILDDIR)/libsynchro.a

.PHONY: all clean test examples

all: $(LIB) test examples

#------------------------------------------------------------------------------
# Static library
#------------------------------------------------------------------------------
$(LIB): $(OBJS)
	@mkdir -p $(BUILDDIR)
	ar rcs $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

#------------------------------------------------------------------------------
# Test binary
#------------------------------------------------------------------------------
$(TEST_BIN): $(TEST_SRC) $(LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) $< -L$(BUILDDIR) -lsynchro $(LDFLAGS) -o $@

test: $(TEST_BIN)
	@echo "Running test suite..."
	./$(TEST_BIN)

#------------------------------------------------------------------------------
# Example binaries
#------------------------------------------------------------------------------
$(BUILDDIR)/example_bfs: $(EXAMDIR)/example_bfs.c $(LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) $< -L$(BUILDDIR) -lsynchro $(LDFLAGS) -o $@

$(BUILDDIR)/example_leader_election: $(EXAMDIR)/example_leader_election.c $(LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) $< -L$(BUILDDIR) -lsynchro $(LDFLAGS) -o $@

$(BUILDDIR)/example_sensor_network: $(EXAMDIR)/example_sensor_network.c $(LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) $< -L$(BUILDDIR) -lsynchro $(LDFLAGS) -o $@

$(BUILDDIR)/example_complexity_comparison: $(EXAMDIR)/example_complexity_comparison.c $(LIB)
	$(CC) $(CFLAGS) -I$(INCDIR) $< -L$(BUILDDIR) -lsynchro $(LDFLAGS) -o $@

examples: $(EXAMPLE_BINS)

#------------------------------------------------------------------------------
# Run examples
#------------------------------------------------------------------------------
run-examples: examples
	@echo ""
	@echo "========== Example: BFS =========="
	./$(BUILDDIR)/example_bfs
	@echo ""
	@echo "========== Example: Leader Election =========="
	./$(BUILDDIR)/example_leader_election
	@echo ""
	@echo "========== Example: Sensor Network =========="
	./$(BUILDDIR)/example_sensor_network
	@echo ""
	@echo "========== Example: Complexity Comparison =========="
	./$(BUILDDIR)/example_complexity_comparison

#------------------------------------------------------------------------------
# Clean
#------------------------------------------------------------------------------
clean:
	rm -rf $(BUILDDIR)

#------------------------------------------------------------------------------
# Help
#------------------------------------------------------------------------------
help:
	@echo "Targets:"
	@echo "  make all          Build library, tests, and examples"
	@echo "  make test         Build and run test suite"
	@echo "  make examples     Build all example programs"
	@echo "  make run-examples Build and run all examples"
	@echo "  make clean        Remove build artifacts"
	@echo "  make help         Show this help"
