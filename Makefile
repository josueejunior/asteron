CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
TARGET = asteron
SRCDIR = src
OBJDIR = obj

# Directories
LEXER_DIR = $(SRCDIR)/lexer
PARSER_DIR = $(SRCDIR)/parser
AST_DIR = $(SRCDIR)/ast
UTILS_DIR = $(SRCDIR)/utils

# Directories
INTERPRETER_DIR = $(SRCDIR)/interpreter
GRAPH_DIR = $(SRCDIR)/graph
TYPECHECKER_DIR = $(SRCDIR)/typechecker
SSA_DIR = $(SRCDIR)/optimizer
ESCAPE_DIR = $(SRCDIR)/optimizer
INLINE_DIR = $(SRCDIR)/optimizer
REG_ALLOC_DIR = $(SRCDIR)/optimizer
VM_DIR = $(SRCDIR)/vm
SCHEDULER_DIR = $(SRCDIR)/scheduler
GRAPH_DECLARATIVE_DIR = $(SRCDIR)/graph_declarative

# Source files
SOURCES = $(SRCDIR)/main.c \
          $(LEXER_DIR)/lexer.c \
          $(PARSER_DIR)/parser.c \
          $(AST_DIR)/ast.c \
          $(UTILS_DIR)/utils.c \
          $(TYPECHECKER_DIR)/typechecker.c \
          $(SSA_DIR)/ssa.c \
          $(SSA_DIR)/escape.c \
          $(INLINE_DIR)/inline.c \
          $(REG_ALLOC_DIR)/reg_alloc.c \
          $(INTERPRETER_DIR)/interpreter.c \
          $(VM_DIR)/compiler.c \
          $(VM_DIR)/vm.c \
          $(VM_DIR)/jit.c \
          $(SCHEDULER_DIR)/scheduler.c \
          $(GRAPH_DECLARATIVE_DIR)/graph_declarative.c \
          $(GRAPH_DIR)/graph.c

# Object files (manual mapping to subdirectories)
OBJECTS = $(OBJDIR)/main.o \
          $(OBJDIR)/lexer/lexer.o \
          $(OBJDIR)/parser/parser.o \
          $(OBJDIR)/ast/ast.o \
          $(OBJDIR)/utils/utils.o \
          $(OBJDIR)/typechecker/typechecker.o \
          $(OBJDIR)/optimizer/ssa.o \
          $(OBJDIR)/optimizer/escape.o \
          $(OBJDIR)/optimizer/inline.o \
          $(OBJDIR)/optimizer/reg_alloc.o \
          $(OBJDIR)/interpreter/interpreter.o \
          $(OBJDIR)/vm/compiler.o \
          $(OBJDIR)/vm/vm.o \
          $(OBJDIR)/vm/jit.o \
          $(OBJDIR)/scheduler/scheduler.o \
          $(OBJDIR)/graph_declarative/graph_declarative.o \
          $(OBJDIR)/graph/graph.o

# Create necessary directories
$(OBJDIR):
	mkdir -p $(OBJDIR)/lexer
	mkdir -p $(OBJDIR)/parser
	mkdir -p $(OBJDIR)/ast
	mkdir -p $(OBJDIR)/utils
	mkdir -p $(OBJDIR)/typechecker
	mkdir -p $(OBJDIR)/optimizer
	mkdir -p $(OBJDIR)/interpreter
	mkdir -p $(OBJDIR)/vm
	mkdir -p $(OBJDIR)/scheduler
	mkdir -p $(OBJDIR)/graph_declarative
	mkdir -p $(OBJDIR)/graph

# Default rule
all: $(TARGET)

# Compile executable
$(TARGET): $(OBJDIR) $(OBJECTS)
	$(CC) $(CFLAGS) -pthread -o $(TARGET) $(OBJECTS)

# Compile object files
$(OBJDIR)/lexer/lexer.o: $(LEXER_DIR)/lexer.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/parser/parser.o: $(PARSER_DIR)/parser.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ast/ast.o: $(AST_DIR)/ast.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/utils/utils.o: $(UTILS_DIR)/utils.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/typechecker/typechecker.o: $(TYPECHECKER_DIR)/typechecker.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/optimizer/ssa.o: $(SSA_DIR)/ssa.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/optimizer/escape.o: $(SSA_DIR)/escape.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/optimizer/inline.o: $(INLINE_DIR)/inline.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/optimizer/reg_alloc.o: $(REG_ALLOC_DIR)/reg_alloc.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/interpreter/interpreter.o: $(INTERPRETER_DIR)/interpreter.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/vm/compiler.o: $(VM_DIR)/compiler.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/vm/vm.o: $(VM_DIR)/vm.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/vm/jit.o: $(VM_DIR)/jit.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/scheduler/scheduler.o: $(SCHEDULER_DIR)/scheduler.c
	$(CC) $(CFLAGS) -pthread -c $< -o $@

$(OBJDIR)/graph_declarative/graph_declarative.o: $(GRAPH_DECLARATIVE_DIR)/graph_declarative.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/graph/graph.o: $(GRAPH_DIR)/graph.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/main.o: $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean compiled files
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Test with test files
test: $(TARGET)
	@echo "=== Test 1 ==="
	./$(TARGET) tests/test1.ast
	@echo ""
	@echo "=== Test 2 ==="
	./$(TARGET) tests/test2.ast

.PHONY: all clean test

