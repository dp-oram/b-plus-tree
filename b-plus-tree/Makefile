# definitions

SDIR=src
TDIR=test
HDIR=benchmark
IDIR=include
ODIR=obj
LDIR=lib
BDIR=bin

LDFLAGS=-L $(LDIR)
LDLIBS=-l boost_system # libs for main code
LDTESTLIBS=-l gtest -l pthread -l benchmark # libs for tests and benchmarks
INCLUDES=-I $(IDIR)
CPPFLAGS= --std=c++17 -Wall -Wno-unknown-pragmas -fPIC
CC=g++
RM=rm -rf

LIBNAME=bplustree

# if you follow the convention that for each class CLASS you have a header in
# $(IDIR)/CLASS.hpp, a code in $(SDIR)/CLASS.cpp and a test in $(TDIR)/test-CLASS.cpp,
# then the rest will magically work - it will compile each class and test and will run the tests.
# CLASS does not even have to be a class in C++.
ENTITIES = storage-adapter utility tree

# dependencies - definitions plus header files
_DEPS = definitions.h $(addsuffix .hpp, $(ENTITIES))
DEPS = $(patsubst %, $(IDIR)/%, $(_DEPS))

_OBJ = $(addsuffix .o, $(ENTITIES))
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

TESTS = $(ENTITIES)
TESTBIN = $(addprefix $(BDIR)/test-, $(TESTS))
JUNITS= $(foreach test, $(TESTS), bin/test-$(test)?--gtest_output=xml:junit-$(test).xml)

BENCHMARKS = tree
BENCHMARKSBIN = $(addprefix $(BDIR)/benchmark-, $(BENCHMARKS))

INTEGRATION = tree-big
INTEGRATIONBIN = $(addprefix $(BDIR)/test-, $(INTEGRATION))

ENTRYPOINTCC = $(CC) -o $@ $^ $(CPPFLAGS) $(INCLUDES) $(LDLIBS) $(LDTESTLIBS) $(LDFLAGS)

# flags-setting commands

all: shared docs

binaries: $(TESTBIN) $(INTEGRATIONBIN) $(BENCHMARKSBIN)
cleandebug: clean debug

debug: CPPFLAGS += -g -DTESTING
debug: binaries

profile: CPPFLAGS += -fprofile-arcs -ftest-coverage -fPIC -O0
profile: clean run-tests-junit

# executables

.SECONDEXPANSION:
$(TESTBIN): $(OBJ) $$(subst $$(BDIR), $(TDIR), $$@).cpp
	$(ENTRYPOINTCC)

.SECONDEXPANSION:
$(BENCHMARKSBIN): $(OBJ) $$(subst $$(BDIR), $(HDIR), $$@).cpp
	$(ENTRYPOINTCC)

.SECONDEXPANSION:
$(INTEGRATIONBIN): $(OBJ) $$(subst $$(BDIR), $(TDIR), $$@).cpp
	$(ENTRYPOINTCC)

shared-debug: CPPFLAGS += -g
shared-debug: shared

shared: CPPFLAGS += -DSHARED -O3
shared: $(OBJ)
	$(CC) -shared $(LDLIBS) $(LDFLAGS) -o $(BDIR)/lib$(LIBNAME).so $(OBJ)

# objects

$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CPPFLAGS) $(INCLUDES)

# commands

# some Linuxes do not respect -L flag during execution, so we can modify the system LD path
ldconfig:
	@echo $(shell pwd)/bin > /etc/ld.so.conf.d/$(LIBNAME)-dev.conf
	@ldconfig

# will compile the test against the library (not objects and sources) and will run it
run-shared-lib: shared
	$(CC) -o $(BDIR)/test-shared-lib $(TDIR)/test-shared-lib.cpp -I $(IDIR) -L $(BDIR) -l $(LIBNAME) $(CPPFLAGS)
	$(BDIR)/test-shared-lib

run-tests-junit: CPPFLAGS += -DTESTING
run-tests-junit: $(TESTBIN)
	$(subst ?, ,$(addsuffix &&, $(JUNITS))) echo Tests passed!

run-tests: CPPFLAGS += -DTESTING
run-tests: $(TESTBIN)
	$(addsuffix &&, $(TESTBIN)) echo Tests passed!

run-benchmarks: $(BENCHMARKSBIN)
	$(addsuffix &&, $(BENCHMARKSBIN)) echo Benchmarks completed!

run-integration: CPPFLAGS += -DTESTING
run-integration: $(INTEGRATIONBIN)
	$(addsuffix &&, $(INTEGRATIONBIN)) echo Tests passed!

coverage: profile
	gcovr -r . $(addprefix -f $(SDIR)/, $(addsuffix .cpp, $(ENTITIES))) --exclude-unreachable-branches
	mkdir -p coverage-html/
	gcovr -r . --html --html-details -o coverage-html/index.html
	gcovr -r . -x -o cobertura.xml

docs: clean-docs
	doxygen ../Doxyfile

clean: clean-docs clean-binaries
	$(RM) $(ODIR)/* *~ *.dSYM *.gcov *.gcda *.gcno *.bin coverage-html junit-*.xml cobertura.xml

clean-docs:
	$(RM) ../docs

clean-binaries:
	$(RM) $(BDIR)/*

# phony

.PHONY: docs clean clean-docs clean-binaries coverage
.PHONY: profile debug cleandebug
.PHONY: binaries all shared
.PHONY: run-tests run-integration run-benchmarks run-shared-lib run-tests-junit
