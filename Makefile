CONFIG ?= config.default
-include $(CONFIG)


BINARY    ?= wolf4sdl
PREFIX    ?= /usr/local
MANPREFIX ?= $(PREFIX)

, := ,

INSTALL         ?= install
INSTALL_PROGRAM ?= $(INSTALL) -m 555 -s
INSTALL_MAN     ?= $(INSTALL) -m 444
INSTALL_DATA    ?= $(INSTALL) -m 444

ifeq ($(SDL_MAJOR_VERSION),1)
	SDL_CONFIG  ?= sdl-config
else
	SDL_CONFIG  ?= sdl2-config
endif
CFLAGS_SDL  ?= $(shell $(SDL_CONFIG) --cflags)
LDFLAGS_SDL ?= $(shell $(SDL_CONFIG) --libs)


CFLAGS += $(CFLAGS_SDL)

#CFLAGS += -Wall
#CFLAGS += -W
CFLAGS += -g
CFLAGS += -Wpointer-arith
CFLAGS += -Wreturn-type
CFLAGS += -Wwrite-strings
CFLAGS += -Wcast-align

ifdef GPL
    CFLAGS += -DUSE_GPL
endif

#
# Read the game data out of flash-resident resources instead of the .wl6
# files.  FLASH_ASSETS is the directory tools/assets/convert.py wrote:
#
#   make FLASH_ASSETS=$PWD/../generated
#
# The assembler include path is for the .incbin in wolf_blobs.S, whose paths
# are relative to that directory.
#
ifdef FLASH_ASSETS
    CFLAGS  += -DUSE_FLASH_ASSETS -I$(FLASH_ASSETS)
    ASFLAGS += -I$(FLASH_ASSETS)
    SRCS_EXTRA += $(FLASH_ASSETS)/wolf_assets.c
    SRCS_EXTRA += $(FLASH_ASSETS)/wolf_blobs.S
endif


CCFLAGS += $(CFLAGS)
CCFLAGS += -std=gnu99
CCFLAGS += -Werror-implicit-function-declaration
CCFLAGS += -Wimplicit-int
CCFLAGS += -Wsequence-point

CXXFLAGS += $(CFLAGS)

LDFLAGS += $(LDFLAGS_SDL)
ifneq (,$(findstring MINGW,$(shell uname -s)))
LDFLAGS += -static-libgcc
endif

SRCS :=
ifndef GPL
    SRCS += mame/fmopl.c
else
    SRCS += dosbox/dbopl.cpp
    SRCS += dosbox/dbopl_adapter.cpp
endif
SRCS += id_ca.c
SRCS += id_in.c
SRCS += id_pm.c
SRCS += id_sd.c
SRCS += id_us.c
SRCS += id_vw.c
SRCS += signon.c
SRCS += wl_act1.c
SRCS += wl_act2.c
SRCS += wl_agent.c
SRCS += wl_atmos.c
SRCS += wl_cloudsky.c
SRCS += wl_debug.c
SRCS += wl_draw.c
SRCS += wl_game.c
SRCS += wl_inter.c
SRCS += wl_main.c
SRCS += wl_menu.c
SRCS += wl_parallax.c
SRCS += wl_plane.c
SRCS += wl_play.c
SRCS += wl_scale.c
SRCS += wl_shade.c
SRCS += wl_state.c
SRCS += wl_text.c
SRCS += wl_utils.c

SRCS += $(SRCS_EXTRA)

DEPS = $(filter %.d, $(SRCS:.c=.d) $(SRCS:.cpp=.d))
OBJS = $(filter %.o, $(SRCS:.c=.o) $(SRCS:.cpp=.o) $(SRCS:.S=.o))

#
# FLASH_ASSETS and GPL change what every object is compiled from, but they are
# on the command line rather than in any prerequisite, so make cannot see them
# move: switching either one and rebuilding silently links a mixture.  That
# reads as the flash path not taking effect at all, because the one file that
# did change is the one that gets recompiled.
#
# So the objects depend on a file holding the current combination, rewritten
# only when it differs.
#
# The rules below introduce targets ahead of `all`, and FORCE would otherwise
# become the default goal.
.DEFAULT_GOAL := all

BUILDCONFIG := $(if $(FLASH_ASSETS),flash:$(FLASH_ASSETS),files)$(if $(GPL),+gpl,)

.buildconfig: FORCE
	$(Q)[ "$$(cat $@ 2>/dev/null)" = "$(BUILDCONFIG)" ] || \
	    { echo '===> CONFIG $(BUILDCONFIG)'; echo '$(BUILDCONFIG)' > $@; }

FORCE:

$(OBJS): .buildconfig

.SUFFIXES:
.SUFFIXES: .c .cpp .S .d .o

Q ?= @

all: $(BINARY)

ifndef NO_DEPS
depend: $(DEPS)

ifeq ($(findstring $(MAKECMDGOALS), clean depend Data),)
-include $(DEPS)
endif
endif

$(BINARY): $(OBJS)
	@echo '===> LD $@'
	$(Q)$(CXX) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

.c.o:
	@echo '===> CC $<'
	$(Q)$(CC) $(CCFLAGS) -c $< -o $@

.cpp.o:
	@echo '===> CXX $<'
	$(Q)$(CXX) $(CXXFLAGS) -c $< -o $@

.S.o:
	@echo '===> AS $<'
	$(Q)$(CC) $(CCFLAGS) $(addprefix -Wa$(,),$(ASFLAGS)) -c $< -o $@

.c.d:
	@echo '===> DEP $<'
	$(Q)$(CC) $(CCFLAGS) -MM $< | sed 's#^$(@F:%.d=%.o):#$@ $(@:%.d=%.o):#' > $@

.cpp.d:
	@echo '===> DEP $<'
	$(Q)$(CXX) $(CXXFLAGS) -MM $< | sed 's#^$(@F:%.d=%.o):#$@ $(@:%.d=%.o):#' > $@

clean distclean:
	@echo '===> CLEAN'
	$(Q)rm -fr $(DEPS) $(OBJS) $(BINARY) $(BINARY).exe .buildconfig

install: $(BINARY)
	@echo '===> INSTALL'
	$(Q)$(INSTALL) -d $(PREFIX)/bin
	$(Q)$(INSTALL_PROGRAM) $(BINARY) $(PREFIX)/bin
