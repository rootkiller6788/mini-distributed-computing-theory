# Makefile — mini-paxos-raft-formal
# Paxos/Raft Consensus Protocols & Formal Verification
#
# Targets:
#   make all        — build all tests and examples
#   make test       — run all tests
#   make examples   — run all examples
#   make clean      — remove built artifacts

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2
LDFLAGS  =

INCDIR   = include
SRCDIR   = src
TESTDIR  = tests
EXAMDIR  = examples
BUILDDIR = build

# ─── Source Files ────────────────────────────────────────────────────

SRC_FILES = $(SRCDIR)/quorum_system.c \
            $(SRCDIR)/paxos_basic.c \
            $(SRCDIR)/paxos_multi.c \
            $(SRCDIR)/raft_leader_election.c \
            $(SRCDIR)/raft_log_replication.c \
            $(SRCDIR)/safety_proof.c \
            $(SRCDIR)/invariant_checker.c \
            $(SRCDIR)/tla_model.c \
            $(SRCDIR)/flp_impossibility.c \
            $(SRCDIR)/byzantine_paxos.c \
            $(SRCDIR)/raft_simulator.c

SRC_OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC_FILES))

# ─── Test Targets ────────────────────────────────────────────────────

TEST_SRCS = $(TESTDIR)/test_quorum.c \
            $(TESTDIR)/test_paxos.c \
            $(TESTDIR)/test_raft.c \
            $(TESTDIR)/test_safety.c

TEST_BINS = $(patsubst $(TESTDIR)/%.c,$(BUILDDIR)/%,$(TEST_SRCS))

# ─── Example Targets ─────────────────────────────────────────────────

EXAMPLE_SRCS = $(EXAMDIR)/synod_example.c \
               $(EXAMDIR)/raft_cluster_example.c \
               $(EXAMDIR)/kv_store_example.c

EXAMPLE_BINS = $(patsubst $(EXAMDIR)/%.c,$(BUILDDIR)/%,$(EXAMPLE_SRCS))

# ─── Default Target ──────────────────────────────────────────────────

.PHONY: all
all: $(TEST_BINS) $(EXAMPLE_BINS)

# ─── Build Directory ─────────────────────────────────────────────────

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# ─── Object Files ────────────────────────────────────────────────────

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# ─── Test Binaries ───────────────────────────────────────────────────

$(BUILDDIR)/test_quorum: $(TESTDIR)/test_quorum.c $(SRC_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/test_paxos: $(TESTDIR)/test_paxos.c $(SRC_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/test_raft: $(TESTDIR)/test_raft.c $(SRC_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/test_safety: $(TESTDIR)/test_safety.c $(SRC_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

# ─── Example Binaries ────────────────────────────────────────────────

$(BUILDDIR)/synod_example: $(EXAMDIR)/synod_example.c $(SRC_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/raft_cluster_example: $(EXAMDIR)/raft_cluster_example.c $(SRC_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

$(BUILDDIR)/kv_store_example: $(EXAMDIR)/kv_store_example.c $(SRC_OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -I$(INCDIR) $< $(SRC_OBJS) -o $@ $(LDFLAGS)

# ─── Test Runner ─────────────────────────────────────────────────────

.PHONY: test
test: $(TEST_BINS)
	@echo "═══════════════════════════════════"
	@echo "  Running All Tests"
	@echo "═══════════════════════════════════"
	@echo ""
	@./$(BUILDDIR)/test_quorum$(EXE)
	@echo ""
	@./$(BUILDDIR)/test_paxos$(EXE)
	@echo ""
	@./$(BUILDDIR)/test_raft$(EXE)
	@echo ""
	@./$(BUILDDIR)/test_safety$(EXE)
	@echo ""
	@echo "═══════════════════════════════════"
	@echo "  All Tests Passed!"
	@echo "═══════════════════════════════════"

# ─── Example Runner ──────────────────────────────────────────────────

.PHONY: examples
examples: $(EXAMPLE_BINS)
	@echo "═══════════════════════════════════"
	@echo "  Running Examples"
	@echo "═══════════════════════════════════"
	@echo ""
	@echo "--- Synod (Basic Paxos) ---"
	@./$(BUILDDIR)/synod_example
	@echo ""
	@echo "--- Raft Cluster ---"
	@./$(BUILDDIR)/raft_cluster_example
	@echo ""
	@echo "--- KV Store ---"
	@./$(BUILDDIR)/kv_store_example
	@echo ""
	@echo "═══════════════════════════════════"
	@echo "  All Examples Complete"
	@echo "═══════════════════════════════════"

# ─── Line Count ──────────────────────────────────────────────────────

.PHONY: lines
lines:
	@echo "include/ + src/ total lines:"
	@find include src -name "*.h" -o -name "*.c" | xargs wc -l | tail -1

# ─── Clean ───────────────────────────────────────────────────────────

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)

# ─── Help ────────────────────────────────────────────────────────────

.PHONY: help
help:
	@echo "mini-paxos-raft-formal Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make all       — build all tests and examples"
	@echo "  make test      — run all tests"
	@echo "  make examples  — run all examples"
	@echo "  make lines     — count lines in include/ + src/"
	@echo "  make clean     — remove build artifacts"
	@echo "  make help      — show this help"
