SHELL = /bin/sh
SYSTEM = $(shell uname)
ARCH = $(shell uname -m)
INSTALL_DIR = /usr

ifndef CC
  CC = gcc
endif

ifndef CXX
  CXX = g++
endif

CCFLAGS += -fno-builtin
CXXFLAGS += -g -std=c++17 -pipe -Wall -Wextra -fno-builtin -fno-rtti
DFLAGS =
OFLAGS = -O3 -flto
LFLAGS += -L. -L/usr/local/lib/ -Lbncsutil/src/bncsutil/ -lgmp -lbz2 -lz -lstorm -lbncsutil

ifeq ($(AURASTATIC), 1)
  LFLAGS += -static
endif

ifeq ($(AURALINKMINIUPNP), 0)
  CXXFLAGS += -DDISABLE_MINIUPNP
else
  ifeq ($(AURASTATIC), 1)
    LFLAGS += -lminiupnpc
  else
    LFLAGS += -lminiupnpc
  endif
endif

ifeq ($(AURALINKCPR), 0)
  CXXFLAGS += -DDISABLE_CPR
else
  ifeq ($(AURASTATIC), 1)
    LFLAGS += -lcpr
  else
    LFLAGS += -lcpr
  endif
endif

ifeq ($(ARCH),x86_64)
	CCFLAGS += -m64
	CXXFLAGS += -m64
endif

ifeq ($(SYSTEM),Darwin)
	INSTALL_DIR = /usr/local
	CXXFLAGS += -stdlib=libc++
	CC = clang
	CXX = clang++
	DFLAGS += -D__APPLE__
else
	LFLAGS += -lrt
endif

ifeq ($(SYSTEM),FreeBSD)
	DFLAGS += -D__FREEBSD__
endif

ifeq ($(SYSTEM),SunOS)
	DFLAGS += -D__SOLARIS__
	LFLAGS += -lresolv -lsocket -lnsl
endif

CCFLAGS += $(OFLAGS) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -I.
CXXFLAGS += $(OFLAGS) $(DFLAGS) -I. -Ibncsutil/src/ -IStormLib/src/ -Iminiupnpc/include/

OBJS = src/fileutil.o \
			 src/socket.o \
			 src/net.o \
			 src/csvparser.o \
       src/bncsutilinterface.o \
			 src/bnetprotocol.o \
			 src/gameprotocol.o \
			 src/gpsprotocol.o \
			 src/config.o \
			 src/config_bot.o \
			 src/config_realm.o \
			 src/config_commands.o \
			 src/config_game.o \
			 src/config_irc.o \
			 src/config_net.o \
			 src/cli.o \
			 src/crc32.o \
			 src/sha1.o \
			 src/realm.o \
       src/realm-chat.o \
			 src/irc.o \
			 src/map.o \
			 src/gameplayer.o \
			 src/gamesetup.o \
			 src/gameslot.o \
			 src/game.o \
			 src/command.o \
			 src/aura.o \
			 src/auradb.o \
			 src/stats.o

COBJS = src/sqlite3.o

PROG = aura

all: $(OBJS) $(COBJS) $(PROG)
	@echo "Used CFLAGS: $(CXXFLAGS)"

$(PROG): $(OBJS) $(COBJS)
	@$(CXX) -o aura $(OBJS) $(COBJS) $(CXXFLAGS) $(LFLAGS)
	@echo "[BIN] $@ created."
	@strip "$(PROG)"
	@echo "[BIN] Stripping the binary."

clean:
	@rm -f $(OBJS) $(COBJS) $(PROG)
	@echo "Binary and object files cleaned."

install:
	@install -d "$(DESTDIR)$(INSTALL_DIR)/bin"
	@install $(PROG) "$(DESTDIR)$(INSTALL_DIR)/bin/$(PROG)"
	@echo "Binary $(PROG) installed to $(DESTDIR)$(INSTALL_DIR)/bin"

$(OBJS): %.o: %.cpp
	@$(CXX) -o $@ $(CXXFLAGS) -c $<
	@echo "[$(CXX)] $@"

$(COBJS): %.o: %.c
	@$(CC) -o $@ $(CCFLAGS) -c $<
	@echo "[$(CC)] $@"

clang-tidy:
	@for file in $(OBJS); do \
		clang-tidy "src/$$(basename $$file .o).cpp" -fix -checks=* -header-filter=src/* -- $(CXXFLAGS) $(DFLAGS); \
	done;
