CFLAGS += -MMD -g -w
VERSION ?= $(shell git rev-parse --short HEAD || echo nix)
ARCHITECTURE ?= x64
CC ?= gcc
PJASS_TYPE ?= standalone
UNAME_S := $(shell uname -s)


ifeq ($(findstring MINGW, $(UNAME_S)), MINGW)
    TARGET_EXT = .exe
else
    ifeq ($(ARCHITECTURE), x86)
        CC = i686-w64-mingw32-gcc
    else
        CC = x86_64-w64-mingw32-gcc
    endif
endif

ifeq ($(PJASS_TYPE), standalone)
    CFLAGS += -DPJASS_STANDALONE
else
  ifeq ($(PJASS_TYPE), static)
    ifeq ($(findstring MINGW, $(UNAME_S)), MINGW)
        TARGET_EXT = .lib
    else
        TARGET_EXT = .a
    endif
  else
    CFLAGS += -fPIC
    CFLAGS_EXTRA += -shared
    ifeq ($(findstring MINGW, $(UNAME_S)), MINGW)
        TARGET_EXT = .dll
    else
        TARGET_EXT = .so
    endif
  endif
endif

ifeq ($(ARCHITECTURE), x86)
    CFLAGS += -m32
    PJASS_OUT_NAME := pjass$(TARGET_EXT)
else
    CFLAGS += -m64
    PJASS_OUT_NAME := pjass64$(TARGET_EXT)
endif

ifeq ($(PJASS_TYPE), shared)
  PJASS_OUT_NAME := lib$(PJASS_OUT_NAME)
endif

SRC := misc.c hashtable.c paramlist.c funcdecl.c typeandname.c blocks.c tree.c sstrhash.c
    
OBJS := $(SRC:.c=.o)
OBJS += main.o token.yy.o grammar.tab.o

.PHONY: all release clean prof
.PHONY: clean-release-files clean-prof-files clean-build-files
.PHONY: binary-release src-release
.PHONY: help

all: $(PJASS_OUT_NAME)

help:
	@awk -F ':|##' \
		'/^[^\t].+?:.*?##/ { \
			printf "\033[36m%-20s\033[0m %s\n", $$1, $$NF \
		}' $(MAKEFILE_LIST)

-include $(OBJS:.o=.d)

ifeq ($(PJASS_TYPE), static)
$(PJASS_OUT_NAME): $(OBJS)
	ar rcs $@ $^  
else
$(PJASS_OUT_NAME): $(OBJS)
	$(CC) $(CFLAGS) $^ $(CFLAGS_EXTRA) -o $@
endif

test: should-fail should-check map-scripts ## Runs all tests

release: src-release binary-release ## Builds a pjass release with src- and bin-zipballs

clean: clean-build-files clean-release-files clean-prof-files ## Cleans the whole project


prof: CFLAGS = -w -pg
prof: pjass ## Builds pjass with profiling support. You can run all tests with profiling enabled via `make PROF=1 test`



main.o: main.c token.yy.h grammar.tab.h
	$(CC) $(CFLAGS) -c -o $@ $< -DVERSIONSTR="\"git-$(VERSION)\""

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<


token.yy.o: token.yy.c | grammar.tab.h
main.o: main.c | grammar.tab.h

# see token.l options block
%.yy.c %.yy.h: %.l
	flex $<

%.tab.c %.tab.h: %.y
	bison -d $<


clean-build-files: ## Cleans all build files
	rm -f grammar.tab.h grammar.tab.c token.yy.c token.yy.h \
		$(OBJS) $(OBJS:.o=.d) \
		pjass pjass.exe pjass.lib pjass64.lib libpjass.so libpjass64.so pjass.dll pjass64.dll pjass.a pjass64.a

clean-release-files: ## Cleans all release zipballs
	rm -f pjass-git-*.zip

clean-prof-files: ## Cleans all profiling files
	rm -f tests/should-check/*-analysis.txt \
          tests/should-fail/*-analysis.txt \
          tests/map-scripts/*-analysis.txt \
          gmon.out


SHOULD_FAIL := $(wildcard tests/should-fail/*.j)
SHOULD_FAIL += $(wildcard tests/should-fail/**/*.j)

SHOULD_CHECK := $(wildcard tests/should-check/*.j)
SHOULD_CHECK += $(wildcard tests/should-check/**/*.j)

MAP_SCRIPTS := $(wildcard tests/map-scripts/*.j)

.PHONY: test print-test should-check should-fail map-scripts
.PHONY: $(SHOULD_CHECK) $(SHOULD_FAIL) $(MAP_SCRIPTS)

$(MAP_SCRIPTS): pjass print-test
	@MAPSCRIPT=1 ./check.sh $@

$(SHOULD_CHECK): pjass print-test
	@./check.sh $@


$(SHOULD_FAIL): pjass print-test
	@./fail.sh $@


should-fail: $(SHOULD_FAIL) ## Tests that should fail
should-check: $(SHOULD_CHECK) ## Tests that should check
map-scripts: $(MAP_SCRIPTS) ## Tests which are run with common.j and Blizzard.j



print-test: pjass
	@echo 'Testing... '

