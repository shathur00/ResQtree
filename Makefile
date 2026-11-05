# ---------------------------------------------------------------------------
# Makefile for ResQTree.
# A Makefile is a list of rules: "to make this file, run this command".
# Run "make" to build, "make run" to build and start, "make clean" to
# wipe the generated files.
# Recipe lines MUST start with a real TAB (not spaces); check with cat -A.
# ---------------------------------------------------------------------------

# Compiler + flags.
#   -std=c++17 : C++17 language
#   -O2        : optimise
#   -Wall -Wextra : warnings (build must be warning-free)
#   -MMD -MP   : auto-generate header dependency files
CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -MMD -MP

SRC_DIR  := src
OBJ_DIR  := build
TARGET   := resqtree

# Every .cpp in src/ becomes an object in build/.
SRCS     := $(wildcard $(SRC_DIR)/*.cpp)
OBJS     := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

.PHONY: all run clean

# Default target.
all: $(TARGET)

# Link objects into the executable.
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Compile one .cpp into a .o. The build/ folder must exist first.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create build/ folder if needed.
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Build then run.
run: $(TARGET)
	./$(TARGET)

# Remove all generated files.
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

-include $(OBJS:.o=.d)
