# ---------------------------------------------------------------------------
# What is a Makefile?
#   It is a list of rules. Each rule says: "to make this file, run this
#   command". Make reads the file, works out what needs rebuilding, and
#   runs the commands. That saves you from re-typing long compile lines
#   and from re-compiling files that have not changed.
#
#   NOTE: the recipe lines below MUST start with a real TAB character,
#   not spaces. Make treats a TAB as "this line is a command to run".
#   Check with:  cat -A Makefile    (a TAB shows up as ^I)
# ---------------------------------------------------------------------------

# The compiler to use, and the flags to compile with.
#   -std=c++17 : use the C++17 language standard
#   -O2        : optimise for speed
#   -Wall -Wextra : turn on all the usual warnings (the code should build
#                   with zero warnings)
#   -MMD -MP   : let the compiler write out the .d dependency files that
#                the last line of this file includes
CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -MMD -MP

# Where the source files live, where the compiled files go, and the names
# of the two programs this project produces.
SRC_DIR  := src
OBJ_DIR  := build
TARGET   := resqtree
TEST_BIN := test_basic

# Collect every .cpp file in src/ into SRCS, then work out the matching
# .o file names in build/. The wildcard is expanded by make itself.
SRCS     := $(wildcard $(SRC_DIR)/*.cpp)
OBJS     := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))
LIB_OBJS := $(filter-out $(OBJ_DIR)/main.o,$(OBJS))

# The targets below are not real files, they are just names of recipes,
# so tell make not to look for files with these names.
.PHONY: all run test clean

# "make" with no arguments builds the main program.
all: $(TARGET)

# Link all the object files into one executable.
# $^ is a shortcut for "all the prerequisites of this rule", which here
# are exactly the object files.
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# The rule for compiling one file. The % stands for any file name, so this
# rule turns src/kdtree.cpp into build/kdtree.o. The | $(OBJ_DIR) part is
# an "order only" prerequisite: the build folder must exist first, but
# changing it does not force a recompile.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# The build folder does not exist yet the first time we run make, so we
# create it here.
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# "make run" builds the program and then starts it.
run: $(TARGET)
	./$(TARGET)

# "make test" builds the test program and runs it.
# The test file has its own main(), so main.o is left out (LIB_OBJS) and
# a second main would clash. -I$(SRC_DIR) lets the test include the
# project headers with #include "kdtree.h".
test: $(LIB_OBJS)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) tests/test_basic.cpp $(LIB_OBJS) -o $(TEST_BIN)
	./$(TEST_BIN)

# "make clean" removes everything that was generated, so you can start
# from a clean slate. The .d file for the test build is listed as well,
# because the test target compiles the test source directly and that
# leaves a test_basic.d behind.
clean:
	rm -rf $(OBJ_DIR) $(TARGET) $(TEST_BIN) $(TEST_BIN).d

# The compiler wrote one .d file per .o file listing the headers that were
# used. Including them here means make will rebuild a .o file if one of
# its headers changes. The leading "-" tells make to carry on if the
# files do not exist yet.
-include $(OBJS:.o=.d)
