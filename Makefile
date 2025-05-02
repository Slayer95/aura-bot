AURABUILD_STATIC ?= 0
AURABUILD_CPR ?= 1
AURABUILD_DPP ?= 1
AURABUILD_MDNS ?= 0
AURABUILD_MINIUPNP ?= 1
AURABUILD_PJASS ?= 0

AURABUILD_STATIC := $(strip $(AURABUILD_STATIC))
AURABUILD_CPR := $(strip $(AURABUILD_CPR))
AURABUILD_DPP := $(strip $(AURABUILD_DPP))
AURABUILD_MDNS := $(strip $(AURABUILD_MDNS))
AURABUILD_MINIUPNP := $(strip $(AURABUILD_MINIUPNP))
AURABUILD_PJASS := $(strip $(AURABUILD_PJASS))

ifneq ($(filter $(AURABUILD_STATIC),0 1),$(AURABUILD_STATIC))
$(error AURABUILD_STATIC must be 0 or 1, but got '$(AURABUILD_STATIC)')
endif

ifneq ($(filter $(AURABUILD_CPR),0 1),$(AURABUILD_CPR))
$(error AURABUILD_CPR must be 0 or 1, but got '$(AURABUILD_CPR)')
endif

ifneq ($(filter $(AURABUILD_DPP),0 1),$(AURABUILD_DPP))
$(error AURABUILD_DPP must be 0 or 1, but got '$(AURABUILD_DPP)')
endif

ifneq ($(filter $(AURABUILD_MDNS),0 1),$(AURABUILD_MDNS))
$(error AURABUILD_MDNS must be 0 or 1, but got '$(AURABUILD_MDNS)')
endif

ifneq ($(filter $(AURABUILD_MINIUPNP),0 1),$(AURABUILD_MINIUPNP))
$(error AURABUILD_MINIUPNP must be 0 or 1, but got '$(AURABUILD_MINIUPNP)')
endif

ifneq ($(filter $(AURABUILD_PJASS),0 1),$(AURABUILD_PJASS))
$(error AURABUILD_PJASS must be 0 or 1, but got '$(AURABUILD_PJASS)')
endif

SHELL = /bin/sh
SYSTEM = $(shell uname)
ARCH = $(shell uname -m)
INSTALL_DIR = /usr
CC ?= gcc
CXX ?= g++
CCFLAGS += -fno-builtin
CXXFLAGS += -g -std=c++17 -pipe -pthread -Wall -Wextra -fno-builtin -fno-rtti
DFLAGS = -DNDEBUG
OFLAGS = -O3 -flto
LFLAGS += -pthread -L. -Llib/ -L/usr/local/lib/ -Ldeps/bncsutil/src/bncsutil/ -lgmp -lbz2 -lz -lstorm -lbncsutil

ifeq ($(AURABUILD_STATIC),1)
  LFLAGS += -static
endif

ifeq ($(AURABUILD_CPR),1)
  LFLAGS += -lcpr
else
  CXXFLAGS += -DDISABLE_CPR
endif

ifeq ($(AURABUILD_DPP),1)
  LFLAGS += -ldpp
else
  CXXFLAGS += -DDISABLE_DPP
endif

ifeq ($(AURABUILD_MINIUPNP),1)
  LFLAGS += -lminiupnpc
else
  CXXFLAGS += -DDISABLE_MINIUPNP
endif

CXXFLAGS += -DDISABLE_PJASS
CXXFLAGS += -DDISABLE_BONJOUR

ifeq ($(AURABUILD_PJASS),1)
$(error AURABUILD_PJASS is not yet supported. Please set AURABUILD_PJASS=0)
endif

ifeq ($(AURABUILD_MDNS),1)
$(error AURABUILD_MDNS is not yet supported. Please set AURABUILD_MDNS=0)
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

LFLAGS += -ldl

CCFLAGS += $(OFLAGS) -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION -I.
CXXFLAGS += $(OFLAGS) $(DFLAGS) -I. -Ilib/ -Ideps/bncsutil/src/ -Ideps/StormLib/src/ -Ideps/miniupnpc/include/ -Icpr-src/include/ -Idpp-src/include/

OBJS = lib/base64/base64.o \
       lib/csvparser/csvparser.o \
       lib/crc32/crc32.o \
       lib/sha1/sha1.o \
       src/protocol/bnet_protocol.o \
       src/protocol/game_protocol.o \
       src/protocol/gps_protocol.o \
       src/protocol/vlan_protocol.o \
       src/config/config.o \
       src/config/config_bot.o \
       src/config/config_realm.o \
       src/config/config_commands.o \
       src/config/config_game.o \
       src/config/config_irc.o \
       src/config/config_discord.o \
       src/config/config_net.o \
       src/auradb.o \
       src/bncsutil_interface.o \
       src/bonjour.o \
       src/file_util.o \
       src/json.o \
       src/os_util.o \
       src/pjass.o \
       src/map.o \
       src/packed.o \
       src/save_game.o \
       src/socket.o \
       src/connection.o \
       src/net.o \
       src/realm.o \
       src/realm_chat.o \
       src/async_observer.o \
       src/game_controller_data.o \
       src/game_interactive_host.o \
       src/game_result.o \
       src/game_seeker.o \
       src/game_setup.o \
       src/game_slot.o \
       src/game_structs.o \
       src/game_user.o \
       src/game_virtual_user.o \
       src/game.o \
       src/aura.o \
       src/cli.o \
       src/command.o \
       src/command_history.o \
       src/locations.o \
       src/rate_limiter.o \
       src/integration/discord.o \
       src/integration/irc.o \
       src/stats/dota.o \
       src/stats/w3mmd.o \

COBJS = lib/sqlite3/sqlite3.o

PROG = aura

all: $(OBJS) $(COBJS) $(PROG)
	@echo "Used CCFLAGS: $(CCFLAGS)"
	@echo "Used CXXFLAGS: $(CXXFLAGS)"

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
