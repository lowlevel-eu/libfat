#  Konvertier *.c in *.o
OBJECT_FILES = $(addsuffix .o,$(basename $(1)))

LL_OBJS= $(LIBFAT_OBJ)

TESTS= $(notdir $(wildcard tests/*))

CFLAGS += -O2 -Wall -Werror -g
CFLAGS_LIBFAT += -Wextra -Wno-unused-parameter


##############################################################################
# Targets fuer Tests
##############################################################################
# Quelldateien der testlib
TESTLIB_SRC= $(wildcard testlib/*.c)

# Objektdateien der testlib
TESTLIB_OBJ= $(call OBJECT_FILES,$(TESTLIB_SRC))
ALL_OBJS+= $(TESTLIB_OBJ)

libtest.a: CFLAGS+= -I libfat/include
libtest.a: $(TESTLIB_OBJ)
	@ar rs $@ $^

define TEST_template
$(1)_SRC := $(wildcard tests/$(1)/*.c)
$(1)_OBJ := $$(call OBJECT_FILES,$$($(1)_SRC))
ALL_OBJS+= $$($(1)_OBJ)
tests/$(1)/test: CFLAGS+= -I libfat/include -I testlib -L .
tests/$(1)/test: libfat.a libtest.a $$($(1)_OBJ)
	@$(CC) $(CFLAGS) -L . -o tests/$(1)/test $$($(1)_OBJ) -lfat -ltest

run_valgrind_tests/$(1)/test: tests/$(1)/test
	@bzcat tests/$(1)/image.bz2 > tests/$(1)/img.run
	@(valgrind -q --leak-check=full --show-reachable=yes tests/$(1)/test tests/$(1)/img.run && rm tests/$(1)/img.run) || true

run_tests/$(1)/test: tests/$(1)/test
	@bzcat tests/$(1)/image.bz2 > tests/$(1)/img.run
	@(tests/$(1)/test tests/$(1)/img.run && rm tests/$(1)/img.run) || true
endef
$(foreach test,$(TESTS),$(eval $(call TEST_template,$(test))))

clean_tests:
	@rm -f $(TESTS:%=tests/%/img.run)
build_tests: $(TESTS:%=tests/%/test)
run_tests: $(TESTS:%=run_tests/%/test)
run_valgrind_tests: $(TESTS:%=run_valgrind_tests/%/test)

##############################################################################
# Targets fuer libfat
##############################################################################
# Quelldateien
LIBFAT_SRC= $(wildcard libfat/*.c)

# Objektdateien
LIBFAT_OBJ= $(call OBJECT_FILES,$(LIBFAT_SRC))
ALL_OBJS+= $(LIBFAT_OBJ)

libfat.a: $(LIBFAT_OBJ) 
	@ar rs $@ $^
$(LIBFAT_OBJ): CFLAGS += $(CFLAGS_LIBFAT)
libfat: libfat.a


##############################################################################
# Targets fuer Dokumentation
##############################################################################
docu:
	@doxygen doc/Doxyfile

clean-docu:
	@rm -rf doc/html

##############################################################################
# Allgemeine Targets
##############################################################################
all: libfat build_tests
clean: clean_tests
	@rm -f $(ALL_OBJS) $(TESTS:%=tests/%/test) libtest.a libfat.a

%.o: %.c
	@$(CC) $(CFLAGS) -c -o $@ $^
