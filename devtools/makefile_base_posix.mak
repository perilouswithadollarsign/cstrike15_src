#
# Base makefile for Linux and OSX
#
# !!!!! Note to future editors !!!!!
# 
# before you make changes, make sure you grok:
# 1. the difference between =, :=, +=, and ?= 
# 2. how and when this base makefile gets included in the generated makefile(s)
#  ( see http://www.gnu.org/software/make/manual/make.html#Flavors )
#
# Command line prefixes:
#  -	errors are ignored
#  @	command is not printed to stdout before being executed
#  +	command is executed even if Make is invoked in "do not exec" mode

OS := $(shell uname)
HOSTNAME := $(shell hostname)

IDENTIFY_CURRENT_MAKEFILE_RELATIVE_FUNCTION = $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))
MAKEFILE_BASE_POSIX_MAK := $(call IDENTIFY_CURRENT_MAKEFILE_RELATIVE_FUNCTION)
CROSS_COMPILE_DIR := $(realpath $(dir $(MAKEFILE_BASE_POSIX_MAK))../../cross_compile)

ifeq ($(MAKE_VERBOSE),1)
    QUIET_PREFIX =
    QUIET_ECHO_POSTFIX =
else
    QUIET_PREFIX = @
    QUIET_ECHO_POSTFIX = > /dev/null

    # Use abbreviated progress messages in the schema compiler.
    VALVE_SCHEMA_QUIET = 1
    export VALVE_SCHEMA_QUIET
endif

BASENAME := basename
CAT := cat
CP := cp
CUT := cut
DIRNAME := dirname
ECHO := echo
ETAGS := etags
EXPR := expr
FALSE := false
FGREP := fgrep
FIND := find
GREP := grep
ICONV := iconv
MKDIR := mkdir
PWD := PWD
PWD_TOOL := pwd
RM := rm
SED := sed
SLEEP := sleep
TAIL := tail
TOUCH := touch
TR := tr
TRUE := true
UNAME := uname
WHICH := which

ECHO_LF = $(ECHO) -e

BUILD_DEBUG_ECHO = $(TRUE)
#uncomment or define ENABLE_BUILD_DEBUG=1 on the make commandline to debug build phases and dependency updates
# ENABLE_BUILD_DEBUG = 1
ifneq "$(ENABLE_BUILD_DEBUG)" ""
    BUILD_DEBUG_ECHO = $(ECHO)
endif

# SPEW_UPDATED_DEPENDENCIES spews the list of dependencies whose current timestamps invalidate this rule.
#  This is only done when ENABLE_BUILD_DEBUG is 1
ifeq ("$(ENABLE_BUILD_DEBUG)","1")
SPEW_DEPENDENCY_CHUNK = \
    SpewDependencyChunk () \
    { \
        $(BUILD_DEBUG_ECHO) "	$${1}"; \
        return 0; \
    }

SPEW_UPDATED_DEPENDENCIES = \
    $(QUIET_PREFIX) \
    { \
        $(BUILD_DEBUG_ECHO) "[SPEW_UPDATED_DEPENDENCIES] rule \"$@\" triggered by files:" && \
        $(BUILD_DEBUG_ECHO) "	$?"; \
    }
else
SPEW_UPDATED_DEPENDENCIES = $(QUIET_PREFIX) $(TRUE)
endif

-include $(SRCROOT)/devtools/steam_def.mak

# To build with clang, set the following in your environment:
#   CC = clang
#   CXX = clang++

ifeq ($(CFG), release)
	# With gcc 4.6.3, engine.so went from 7,383,765 to 8,429,109 when building with -O3.
	#  There also was no speed difference running at 1280x1024. May 2012, mikesart.
	# -fno-omit-frame-pointer: need this for stack traces with perf.
	OptimizerLevel_CompilerSpecific = -O2 -fno-strict-aliasing -ffast-math -fno-omit-frame-pointer
else
	OptimizerLevel_CompilerSpecific = -O0
	#-O1 -finline-functions
endif

# CPPFLAGS == "c/c++ *preprocessor* flags" - not "cee-plus-plus flags"
ARCH_FLAGS = 
BUILDING_MULTI_ARCH = 0
CPPFLAGS = $(DEFINES) $(FORCEINCLUDES) $(addprefix -I, $(abspath $(INCLUDEDIRS) ))

ifeq ($(TARGET_PLATFORM),linux64)
	CPPFLAGS += -fPIC
endif


CFLAGS = $(ARCH_FLAGS) $(CPPFLAGS) $(WARN_FLAGS) -fvisibility=$(SymbolVisibility) $(OptimizerLevel) -ffast-math -pipe $(GCC_ExtraCompilerFlags) -Usprintf -Ustrncpy -UPROTECTED_THINGS_ENABLE
# In -std=gnu++11 mode we get lots of errors about "error: narrowing conversion". -fpermissive
# turns these into warnings in gcc, and -Wno-c++11-narrowing suppresses them entirely in clang 3.1+.

ifeq ($(OS),Linux)
	CXXFLAGS = $(CFLAGS) -std=gnu++0x -fpermissive
else
	CXXFLAGS = $(CFLAGS) -std=gnu++11 -stdlib=libc++ -Wno-c++11-narrowing -Wno-dangling-else
endif

DEFINES += -DVPROF_LEVEL=1 -DGNUC
# This causes all filesystem interfaces to default to their 64bit versions on
# 32bit systems, which means we don't break on filesystems with inodes > 32bit.
DEFINES += -D_FILE_OFFSET_BITS=64

LDFLAGS = $(CFLAGS) $(GCC_ExtraLinkerFlags) $(OptimizerLevel)
GENDEP_CXXFLAGS = -MD -MP -MF $(@:.o=.P) 
MAP_FLAGS =

ifeq ($(STEAM_BRANCH),1)
	WARN_FLAGS = -Wall -Wextra -Wshadow -Wno-invalid-offsetof
else
	WARN_FLAGS = -Wno-write-strings -Wno-multichar
endif

WARN_FLAGS += -Wno-unknown-pragmas -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-sign-compare -Wno-reorder -Wno-invalid-offsetof -Wno-float-equal -Wno-switch -fdiagnostics-show-option -Wformat -Werror=format-security -Wstrict-aliasing=2


ifeq ($(OS),Linux)
	# We should always specify -Wl,--build-id, as documented at:
	# http://linux.die.net/man/1/ld and http://fedoraproject.org/wiki/Releases/FeatureBuildId.http://fedoraproject.org/wiki/Releases/FeatureBuildId
	LDFLAGS += -Wl,--build-id

	UUID_LIB =

	# Set USE_STEAM_RUNTIME to build with the Steam Runtime. Otherwise uses
	# The toolchain in /valve
	ifneq ($(USE_STEAM_RUNTIME),1)
		# dedicated server flags
		ifeq ($(TARGET_PLATFORM),linux64)
			VALVE_BINDIR = /valve/bin64/
			MARCH_TARGET = nocona
		else
			VALVE_BINDIR = /valve/bin/
			MARCH_TARGET = pentium4
		endif
		STRIP_FLAGS = -x
		LIBCPP_EXT = a

	else
		# linux desktop client flags
		VALVE_BINDIR =
		DEFINES +=
		# If the steam-runtime is available, use it. We should just default to using it when
		#  buildbot and everyone has a bit of time to get it installed.
		ifneq "$(wildcard /valve/steam-runtime/bin/)" ""
			# The steam-runtime is incompatible with clang at this point, so disable it
			# if clang is enabled.
			ifneq ($(CXX),clang++)
				VALVE_BINDIR = /valve/steam-runtime/bin/
			endif
		endif
		GCC_VER =

		ifeq ($(TARGET_PLATFORM),linux64)
			MARCH_TARGET = nocona
		else
			MARCH_TARGET = pentium4
		endif
		# On dedicated servers, some plugins depend on global variable symbols in addition to functions.
		# So symbols like _Z16ClearMultiDamagev should show up when you do "nm server_srv.so" in TF2.
		STRIP_FLAGS = -x

		LIBCPP_EXT = so

		UUID_LIB = -luuid
	endif

	# We want to make all TLS use the global-dynamic
	# model, to avoid having to use -fpic but avoid problems with dlopen()
	# failing due to TLS clashes. This happens in particular when trying
	# to run the game with primus. Note that -ftls-model=global-dynamic
	# doesn't work due to undocumented 'features' in gcc that only allow
	# the TLS model to be 'downgraded' from the default and not upgraded.
	CFLAGS += -D__thread='__thread __attribute__((tls_model("global-dynamic")))'
	CXXFLAGS += -D__thread='__thread __attribute__((tls_model("global-dynamic")))'

	ifeq ($(CXX),clang++)
		# Clang does not support -mfpmath=sse
		SSE_GEN_FLAGS = -msse2
	else
		SSE_GEN_FLAGS = -msse2 -mfpmath=sse
	endif

	# Turn this on when ready to fix errors that crop up (can merge fixes from console eventually).
	#WARN_FLAGS += -Werror=return-type
	ifeq ($(CXX),clang++)
		# The C-linkage return-type warning (no returning of references) must be disabled after the
		# return-type error is enabled.
		WARN_FLAGS += -Wno-return-type-c-linkage
		# -g0 must be specified because -g2 makes server.so.dbg so huge that the linker sometimes
		# fails due to memory exhaustion.
		CFLAGS += -g0
	endif
	CCACHE := $(SRCROOT)/devtools/bin/linux/ccache

	ifeq ($(origin GCC_VER), undefined)
	GCC_VER=-4.6
	endif
	ifeq ($(origin AR), default)
		AR = $(VALVE_BINDIR)ar crs
	endif
	ifeq ($(origin CC),default)
		CC = $(CCACHE) $(VALVE_BINDIR)gcc$(GCC_VER)	
	endif
	ifeq ($(origin CXX), default)
		CXX = $(CCACHE) $(VALVE_BINDIR)g++$(GCC_VER)
	endif
	# Support ccache with clang. Add -Qunused-arguments to avoid excessive warnings due to
	# a ccache quirk. Could also upgrade ccache.
	# http://petereisentraut.blogspot.com/2011/05/ccache-and-clang.html
	ifeq ($(CC),clang)
		CC = $(CCACHE) $(VALVE_BINDIR)clang -Qunused-arguments
	endif
	ifeq ($(CXX),clang++)
		CXX = $(CCACHE) $(VALVE_BINDIR)clang++ -Qunused-arguments
	endif
	LINK ?= $(CC)

	ifeq ($(TARGET_PLATFORM),linux64)
		# nocona = pentium4 + 64bit + MMX, SSE, SSE2, SSE3 - no SSSE3 (that's three s's - added in core2)
		ARCH_FLAGS += -march=$(MARCH_TARGET) -mtune=core2
		LD_SO = ld-linux-x86-64.so.2
		LIBSTDCXX := $(shell $(CXX) -print-file-name=libstdc++.$(LIBCPP_EXT))
		LIBSTDCXXPIC := $(shell $(CXX) -print-file-name=libstdc++.$(LIBCPP_EXT))
	else

		# core2 = Intel Core2 CPU with 64-bit extensions, MMX, SSE, SSE2, SSE3 and SSSE3 instruction set support.
		# changed for DOTA since we are running servers on newer hardware
		ifeq ($(TARGET_PLATFORM_EXT),_client)
			# on non-server Linux client builds let's be a little more conservative
			ARCH_FLAGS += -m32 -march=prescott -mtune=core2 $(SSE_GEN_FLAGS)
		else
			ARCH_FLAGS += -m32 -march=core2 -mtune=core2 $(SSE_GEN_FLAGS)
		endif
		LD_SO = ld-linux.so.2
		LIBSTDCXX := $(shell $(CXX) $(ARCH_FLAGS) -print-file-name=libstdc++.$(LIBCPP_EXT))
		LIBSTDCXXPIC := $(shell $(CXX) $(ARCH_FLAGS) -print-file-name=libstdc++.$(LIBCPP_EXT))
		LDFLAGS += -m32
	endif

	GEN_SYM ?= $(SRCROOT)/devtools/gendbg.sh
	ifeq ($(CFG),release)
		STRIP ?= strip $(STRIP_FLAGS) -S
	#	CFLAGS += -ffunction-sections -fdata-sections
	#	LDFLAGS += -Wl,--gc-sections -Wl,--print-gc-sections
	else
		STRIP ?= true
	endif
	VSIGN ?= true

	LINK_MAP_FLAGS = -Wl,-Map,$(@:.so=).map

	SHLIBLDFLAGS = -shared $(LDFLAGS) -Wl,--no-undefined
	_WRAP := -Xlinker --wrap=
	PATHWRAP = $(_WRAP)fopen $(_WRAP)freopen $(_WRAP)open    $(_WRAP)creat    $(_WRAP)access  $(_WRAP)__xstat \
		   $(_WRAP)stat  $(_WRAP)lstat   $(_WRAP)fopen64 $(_WRAP)open64   $(_WRAP)opendir $(_WRAP)__lxstat \
		   $(_WRAP)chmod $(_WRAP)chown   $(_WRAP)lchown  $(_WRAP)symlink  $(_WRAP)link    $(_WRAP)__lxstat64 \
		   $(_WRAP)mknod $(_WRAP)utimes  $(_WRAP)unlink  $(_WRAP)rename   $(_WRAP)utime   $(_WRAP)__xstat64 \
		   $(_WRAP)mount $(_WRAP)mkfifo  $(_WRAP)mkdir   $(_WRAP)rmdir    $(_WRAP)scandir $(_WRAP)realpath


	LIB_START_EXE = $(PATHWRAP) -static-libgcc -Wl,--start-group
	LIB_END_EXE = -Wl,--end-group -lm -ldl $(LIBSTDCXX) -lpthread $(UUID_LIB)

	LIB_START_SHLIB = $(PATHWRAP) -static-libgcc -Wl,--start-group
	LIB_END_SHLIB = -Wl,--end-group -lm -ldl $(LIBSTDCXXPIC) -lpthread $(UUID_LIB) -l:$(LD_SO) -Wl,--version-script=$(SRCROOT)/devtools/version_script.linux.txt

endif

ifeq ($(OS),Darwin)
	LDFLAGS += -stdlib=libc++

	OSXVER := $(shell sw_vers -productVersion)
	CCACHE := $(SRCROOT)/devtools/bin/osx32/ccache
	DEVELOPER_DIR := $(shell /usr/bin/xcode-select -print-path)
	XCODEVER := $(shell /usr/bin/xcode-select -version)


	USE_DEV_USR_BIN := 0
	ifeq (,$(findstring 10.7, $(OSXVER)))
		USE_DEV_USR_BIN := 1	
	endif
	ifeq (/Developer, $(DEVELOPER_DIR))	
		USE_DEV_USR_BIN := 1
	endif
		
	ifeq (1,$(USE_DEV_USR_BIN))
                COMPILER_BIN_DIR := $(DEVELOPER_DIR)/usr/bin
                SDK_DIR := $(DEVELOPER_DIR)/SDKs
        else
                COMPILER_BIN_DIR := $(DEVELOPER_DIR)/Toolchains/XcodeDefault.xctoolchain/usr/bin
                SDK_DIR := $(DEVELOPER_DIR)/Platforms/MacOSX.platform/Developer/SDKs
        endif

	SDKROOT ?= $(SDK_DIR)/MacOSX10.9.sdk
		

	#test to see if you have a compiler in the right place, if you don't abort with an error

        ifeq ($(wildcard $(COMPILER_BIN_DIR)/clang),)
        $(error Unable to find compiler, install and configure XCode)
        endif

        ifeq ($(wildcard $(COMPILER_BIN_DIR)/clang++),)
        $(error Unable to find compiler, install and configure XCode)
        endif


	ifeq ($(origin AR), default)
		AR = libtool -static -o
	endif
	ifeq ($(origin CC), default)
		CC = $(CCACHE) $(COMPILER_BIN_DIR)/clang -Qunused-arguments -Wno-c++11-narrowing -Wno-dangling-else
	endif
	ifeq ($(origin CXX), default)
		CXX = $(CCACHE) $(COMPILER_BIN_DIR)/clang++ -Qunused-arguments -Wno-c++11-narrowing -Wno-dangling-else
	endif
	LINK ?= $(CXX)

	ifeq ($(TARGET_PLATFORM),osx64)
		ARCH_FLAGS += -arch x86_64 -m64 -march=core2 
	else ifeq (,$(findstring -arch x86_64,$(GCC_ExtraCompilerFlags)))
		ARCH_FLAGS += -arch i386 -m32 -march=prescott -momit-leaf-frame-pointer -mtune=core2
	else
		# dirty hack to build a universal binary - don't specify the architecture
		ARCH_FLAGS += -arch i386 -Xarch_i386 -march=prescott -Xarch_i386 -mtune=core2 -Xarch_i386 -momit-leaf-frame-pointer -Xarch_x86_64 -march=core2 
	endif

	#FIXME: NOTE:Full path specified because the xcode 4.0 preview has a terribly broken dsymutil, so ref the 3.2 one
	GEN_SYM ?= /usr/bin/dsymutil
	ifeq ($(CFG),release)
		STRIP ?= strip -x -S
	else
		STRIP ?= true
	endif
	VSIGN ?= true

	CPPFLAGS += -I$(SDKROOT)/usr/include/malloc -ftemplate-depth=1024
	CFLAGS += -isysroot $(SDKROOT) -mmacosx-version-min=10.7 -fasm-blocks -fno-color-diagnostics
	WARN_FLAGS += -Wno-parentheses -Wno-constant-logical-operand -Wno-deprecated

	LIB_START_EXE = -lm -ldl -lpthread
	LIB_END_EXE = 

	LIB_START_SHLIB = 
	LIB_END_SHLIB = 

	SHLIBLDFLAGS = $(LDFLAGS) -bundle -flat_namespace -undefined suppress -Wl,-dead_strip -Wl,-no_dead_strip_inits_and_terms 

	ifeq (lib,$(findstring lib,$(GAMEOUTPUTFILE)))
		SHLIBLDFLAGS = $(LDFLAGS) -dynamiclib -current_version 1.0 -compatibility_version 1.0 -install_name @rpath/$(basename $(notdir $(GAMEOUTPUTFILE))).dylib $(SystemLibraries) -Wl,-dead_strip -Wl,-no_dead_strip_inits_and_terms 
	endif

endif

#
# Profile-directed optimizations.
# Note: Last time these were tested 3/5/08, it actually slowed down the server benchmark by 5%!
#
# First, uncomment these, build, and test. It will generate .gcda and .gcno files where the .o files are.
# PROFILE_LINKER_FLAG=-fprofile-arcs
# PROFILE_COMPILER_FLAG=-fprofile-arcs
#
# Then, comment the above flags out again and rebuild with this flag uncommented:
# PROFILE_COMPILER_FLAG=-fprofile-use
#

#############################################################################
# The compiler command lne for each src code file to compile
#############################################################################

OBJ_DIR = ./obj_$(NAME)_$(TARGET_PLATFORM)$(TARGET_PLATFORM_EXT)/$(CFG)
CPP_TO_OBJ = $(CPPFILES:.cpp=.o)
CXX_TO_OBJ = $(CPP_TO_OBJ:.cxx=.o)
CC_TO_OBJ = $(CXX_TO_OBJ:.cc=.o)
MM_TO_OBJ = $(CC_TO_OBJ:.mm=.o)
C_TO_OBJ = $(MM_TO_OBJ:.c=.o)
OBJS = $(addprefix $(OBJ_DIR)/, $(notdir $(C_TO_OBJ)))

export OBJ_DIR

ifeq ($(MAKE_VERBOSE),1)
	QUIET_PREFIX = 
	QUIET_ECHO_POSTFIX = 
else
	QUIET_PREFIX = @
	QUIET_ECHO_POSTFIX = > /dev/null
endif

ifeq ($(MAKE_CC_VERBOSE),1)
CC += -v
endif

ifeq ($(CONFTYPE),lib)
  LIB_File = $(OUTPUTFILE)
endif

ifeq ($(CONFTYPE),dll)
  SO_File = $(OUTPUTFILE)
endif

ifeq ($(CONFTYPE),exe)
  EXE_File = $(OUTPUTFILE)
endif

# we generate dependencies as a side-effect of compilation now
GEN_DEP_FILE=

PRE_COMPILE_FILE = 

POST_COMPILE_FILE = 

ifeq ($(BUILDING_MULTI_ARCH),1)
	SINGLE_ARCH_CXXFLAGS=$(subst -arch x86_64,,$(CXXFLAGS))
	COMPILE_FILE = \
		$(QUIET_PREFIX) \
		echo "---- $(lastword $(subst /, ,$<)) as MULTIARCH----";\
		mkdir -p $(OBJ_DIR) && \
		$(CXX) $(SINGLE_ARCH_CXXFLAGS) $(GENDEP_CXXFLAGS) -o $@ -c $< && \
		$(CXX) $(CXXFLAGS) -o $@ -c $<
else
	COMPILE_FILE = \
		$(QUIET_PREFIX) \
		echo "---- $(lastword $(subst /, ,$<)) ----";\
		mkdir -p $(OBJ_DIR) && \
		$(CXX) $(CXXFLAGS) $(GENDEP_CXXFLAGS) -o $@ -c $<
endif

ifneq "$(origin VALVE_NO_AUTO_P4)" "undefined"
	P4_EDIT_START = chmod -R +w
	P4_EDIT_END = || true
	P4_REVERT_START = true
	P4_REVERT_END =
else
	ifndef P4_EDIT_CHANGELIST
		# You can use an environment variable to specify what changelist to check the Linux Binaries out into. Normally the default
		# setting is best, but here is an alternate example:
		# export P4_EDIT_CHANGELIST_CMD="echo 1424335"
		# ?= means that if P4_EDIT_CHANGELIST_CMD is already set it won't be changed.
		P4_EDIT_CHANGELIST_CMD ?= p4 changes -c `p4 client -o | grep ^Client | cut -f 2` -s pending | fgrep 'POSIX Auto Checkout' | cut -d' ' -f 2 | tail -n 1
		P4_EDIT_CHANGELIST := $(shell $(P4_EDIT_CHANGELIST_CMD))
	endif
	ifeq ($(P4_EDIT_CHANGELIST),)
		# If we haven't found a changelist to check out to then create one. The name must match the one from a few
		# lines above or else a new changelist will be created each time.
		# Warning: the behavior of 'echo' is not consistent. In bash you need the "-e" option in order for \n to be
		# interpreted as a line-feed, but in dash you do not, and if "-e" is passed along then it is printed, which
		# confuses p4. So, if you run this command from the bash shell don't forget to add "-e" to the echo command.
		P4_EDIT_CHANGELIST = $(shell echo "Change: new\nDescription: POSIX Auto Checkout" | p4 change -i | cut -f 2 -d ' ')
	endif

	P4_EDIT_START := for f in
	P4_EDIT_END := ; do if [ -n $$f ]; then if [ -d $$f ]; then find $$f -type f -print | p4 -x - edit -c $(P4_EDIT_CHANGELIST); else p4 edit -c $(P4_EDIT_CHANGELIST) $$f; fi; fi; done $(QUIET_ECHO_POSTFIX)
	P4_REVERT_START := for f in  
	P4_REVERT_END := ; do if [ -n $$f ]; then if [ -d $$f ]; then find $$f -type f -print | p4 -x - revert; else p4 revert $$f; fi; fi; done $(QUIET_ECHO_POSTFIX) 
endif

ifeq ($(CONFTYPE),dll)
all: $(OTHER_DEPENDENCIES) $(OBJS) $(GAMEOUTPUTFILE)
	@echo $(GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX)
else
all: $(OTHER_DEPENDENCIES) $(OBJS) $(OUTPUTFILE)
	@echo $(OUTPUTFILE) $(QUIET_ECHO_POSTFIX)
endif

.PHONY: clean cleantargets cleanandremove rebuild relink RemoveOutputFile SingleFile


rebuild :
	$(MAKE) -f $(firstword $(MAKEFILE_LIST)) cleanandremove
	$(MAKE) -f $(firstword $(MAKEFILE_LIST))


# Use the relink target to force to relink the project.
relink: RemoveOutputFile all

RemoveOutputFile:
	rm -f $(OUTPUTFILE)


# This rule is so you can say "make SingleFile SingleFilename=/home/myname/valve_main/src/engine/language.cpp" and have it only build that file.
# It basically just translates the full filename to create a dependency on the appropriate .o file so it'll build that.
SingleFile : RemoveSingleFile $(OBJ_DIR)/$(basename $(notdir $(SingleFilename))).o
	@echo ""

RemoveSingleFile:
	$(QUIET_PREFIX) rm -f $(OBJ_DIR)/$(basename $(notdir $(SingleFilename))).o

clean:
ifneq "$(OBJ_DIR)" ""
	$(QUIET_PREFIX) echo "rm -rf $(OBJ_DIR)"
	$(QUIET_PREFIX) rm -rf $(OBJ_DIR)
endif
ifneq "$(OUTPUTFILE)" ""
	$(QUIET_PREFIX) if [ -e $(OUTPUTFILE) ]; then \
		echo "p4 revert $(OUTPUTFILE)"; \
		$(P4_REVERT_START) $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END); \
	fi;
endif
ifneq "$(OTHER_DEPENDENCIES)" ""
	$(QUIET_PREFIX) echo "rm -f $(OTHER_DEPENDENCIES)"
ifneq "$(GAMEOUTPUTFILE)" ""
endif
	$(QUIET_PREFIX) rm -f $(OTHER_DEPENDENCIES)
endif
ifneq "$(GAMEOUTPUTFILE)" ""
	$(QUIET_PREFIX) echo "p4 revert $(GAMEOUTPUTFILE)"
	$(QUIET_PREFIX) $(P4_REVERT_START) $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_REVERT_END)
endif


# Do the above cleaning, except with p4 edit and rm. Reason being ar crs adds and replaces obj files to the
# archive. However if you've renamed or deleted a source file, $(AR) won't remove it. This can leave
# us with archive files that have extra unused symbols, and also potentially cause compilation errors
# when you rename a file and have many duplicate symbols.
cleanandremove:
ifneq "$(OBJ_DIR)" ""
	$(QUIET_PREFIX) echo "rm -rf $(OBJ_DIR)"
	$(QUIET_PREFIX) -rm -rf $(OBJ_DIR)
endif
ifneq "$(OUTPUTFILE)" ""
	$(QUIET_PREFIX) if [ -e $(OUTPUTFILE) ]; then \
		echo "p4 edit and rm -f $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT)"; \
		$(P4_EDIT_START) $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END); \
	fi;
	$(QUIET_PREFIX) -rm -f $(OUTPUTFILE) $(OUTPUTFILE)$(SYM_EXT);
endif
ifneq "$(OTHER_DEPENDENCIES)" ""
	$(QUIET_PREFIX) echo "rm -f $(OTHER_DEPENDENCIES)"
	$(QUIET_PREFIX) -rm -f $(OTHER_DEPENDENCIES)
endif
ifneq "$(GAMEOUTPUTFILE)" ""
	$(QUIET_PREFIX) echo "p4 edit and rm -f $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT)"
	$(QUIET_PREFIX) $(P4_EDIT_START) $(GAMEOUTPUTFILE) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END)
	$(QUIET_PREFIX) -rm -f $(GAMEOUTPUTFILE)
endif


# This just deletes the final targets so it'll do a relink next time we build.
cleantargets:
	$(QUIET_PREFIX) rm -f $(OUTPUTFILE) $(GAMEOUTPUTFILE)


$(LIB_File): $(OTHER_DEPENDENCIES) $(OBJS) 
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(LIB_File) $(P4_EDIT_END); 
	$(QUIET_PREFIX) $(AR) $(LIB_File) $(OBJS) $(LIBFILES);

SO_GameOutputFile = $(GAMEOUTPUTFILE)

$(SO_GameOutputFile): $(SO_File)
	$(QUIET_PREFIX) \
	$(P4_EDIT_START) $(GAMEOUTPUTFILE) $(P4_EDIT_END) && \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- COPYING TO $@ [$(CFG)] ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(GAMEOUTPUTFILE) $(P4_EDIT_END);
	$(QUIET_PREFIX) -mkdir -p `dirname $(GAMEOUTPUTFILE)` > /dev/null;
	$(QUIET_PREFIX) cp -v $(OUTPUTFILE) $(GAMEOUTPUTFILE) $(QUIET_ECHO_POSTFIX);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(GAMEOUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END);
	$(QUIET_PREFIX) $(GEN_SYM) $(GAMEOUTPUTFILE); 
	$(QUIET_PREFIX) -$(STRIP) $(GAMEOUTPUTFILE);
	$(QUIET_PREFIX) $(VSIGN) -signvalve $(GAMEOUTPUTFILE);
	$(QUIET_PREFIX) if [ "$(IMPORTLIBRARY)" != "" ]; then\
		echo "----" $(QUIET_ECHO_POSTFIX);\
		echo "---- COPYING TO IMPORT LIBRARY $(IMPORTLIBRARY) ----";\
		echo "----" $(QUIET_ECHO_POSTFIX);\
		$(P4_EDIT_START) $(IMPORTLIBRARY) $(P4_EDIT_END) && \
		mkdir -p `dirname $(IMPORTLIBRARY)` > /dev/null && \
		cp -v $(OUTPUTFILE) $(IMPORTLIBRARY); \
	fi;


$(SO_File): $(OTHER_DEPENDENCIES) $(OBJS) $(LIBFILENAMES)
	$(QUIET_PREFIX) \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- LINKING $@ [$(CFG)] ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);\
	\
	$(LINK) $(LINK_MAP_FLAGS) $(SHLIBLDFLAGS) $(PROFILE_LINKER_FLAG) -o $(OUTPUTFILE) $(LIB_START_SHLIB) $(OBJS) $(LIBFILES) $(SystemLibraries) $(LIB_END_SHLIB);
	$(VSIGN) -signvalve $(OUTPUTFILE);


$(EXE_File) : $(OTHER_DEPENDENCIES) $(OBJS) $(LIBFILENAMES)
	$(QUIET_PREFIX) \
	echo "----" $(QUIET_ECHO_POSTFIX);\
	echo "---- LINKING EXE $@ [$(CFG)] ----";\
	echo "----" $(QUIET_ECHO_POSTFIX);\
	\
	$(P4_EDIT_START) $(OUTPUTFILE) $(P4_EDIT_END);\
	$(LINK) $(LINK_MAP_FLAGS) $(LDFLAGS) $(PROFILE_LINKER_FLAG) -o $(OUTPUTFILE) $(LIB_START_EXE) $(OBJS) $(LIBFILES) $(SystemLibraries) $(LIB_END_EXE);
	$(QUIET_PREFIX) -$(P4_EDIT_START) $(OUTPUTFILE)$(SYM_EXT) $(P4_EDIT_END);
	$(QUIET_PREFIX) $(GEN_SYM) $(OUTPUTFILE);
	$(QUIET_PREFIX) -$(STRIP) $(OUTPUTFILE);
	$(QUIET_PREFIX) $(VSIGN) -signvalve $(OUTPUTFILE);


tags:
	etags -a -C -o $(SRCROOT)/TAGS *.cpp *.cxx *.h *.hxx

P4EXE ?= p4

# DETECT_STRING_CHANGE_BETWEEN_BUILDS is a macro that lets you update the timestamp on a file whenever a string changes between invokations of make
#  This lets us know that our compile flags are consistent with the last time we ran and avoid overbuilding
#
# Parameters:	$(1) = a unique name as a basis for intermediate variables and file names, the exact name will not be used if you want to give it a name used for $(2).
#				$(2) = extra escaped deref or call that you would invoke to fully evaluate the string in $(1)
#  A file specified with $(call DETECT_STRING_CHANGE_BETWEEN_BUILDS_TIMESTAMP_FILE,$(1)) will have it's timestamp updated whenever the cached settings change

# Make string substitions on the value so it can be represented cleanly in an "$(ECHO) > file" operation
DETECT_STRING_CHANGE_BETWEEN_BUILDS_STRING_FILTER = $(strip $(subst $$,_dollar,$(subst \\,_bs,$(subst =,_eq,$(subst ',_sq,$(subst ",_dq,$(1)))))))

DETECT_STRING_CHANGE_BETWEEN_BUILDS_TIMESTAMP_FILE = $(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated

define DETECT_STRING_CHANGE_BETWEEN_BUILDS

ifeq "$$(DISABLE_DETECT_STRING_CHANGE_BETWEEN_BUILDS)" ""


include $$(wildcard $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_previous)
unexport $(1)_FILTERED_PREV
$(1)_FILTERED_CURRENT := $$(call DETECT_STRING_CHANGE_BETWEEN_BUILDS_STRING_FILTER,$(2))
unexport $(1)_FILTERED_CURRENT

ifeq "1" "0"
# Invalidate any cached settings whenever the base makefile changes in any way. This is mostly paranoia and we should be able to rely on the second rule by itself
$$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_eval:: $(MAKEFILE_BASE_POSIX_MAK) ; \
    $$(QUIET_PREFIX) \
    { \
        $$(BUILD_DEBUG_ECHO) detect string change between builds $(1) eval base start && \
        if [ -e "$$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated" ]; then \
        { \
            $$(BUILD_DEBUG_ECHO) "Discarding $(1) cached value due to changes in \"$$^\""; \
        }; \
        fi; \
        $$(RM) -f $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated; \
        $$(RM) -f $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_previous; \
        $$(RM) -f $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_eval; \
    }
endif

ifneq ("$$($(1)_FILTERED_CURRENT)","$$($(1)_FILTERED_PREV)")

$(1)_WRITE_CHUNK_FUNC = \
    WriteStringChunkToPrevFile () \
    { \
        $$(ECHO) -n "$$$${1}" >> $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_previous && \
        return 0; \
    }

#value changed, write out the new value, touch the updated and eval file
$$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_eval:: $(MAKEFILE_BASE_POSIX_MAK) $(COMPILE_DEPENDANT_MAKEFILES)
	$$(QUIET_PREFIX) $$(BUILD_DEBUG_ECHO) detect string change between builds $(1) eval incremental start
	$$(SPEW_UPDATED_DEPENDENCIES)
	$$(QUIET_PREFIX) \
	{ \
		{ $$(MKDIR) -p $$(OBJ_DIR)/_detect_string_change_between_builds $$(QUIET_ECHO_POSTFIX) || $(TRUE); } && \
		$$(BUILD_DEBUG_ECHO) "$(1) changed since last build" && \
		$$(TOUCH) $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated && \
		$$(ECHO) -n "$(1)_FILTERED_PREV = " > $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_previous; \
	}
	$$(QUIET_PREFIX) $$(call CHUNK_OUT_STRING_FOR_SHELL_LIMITS,$$($(1)_WRITE_CHUNK_FUNC),WriteStringChunkToPrevFile, ,$$($(1)_FILTERED_CURRENT))
	$$(QUIET_PREFIX) $$(TOUCH) $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_eval

else

#value is the same, just touch the eval file
$$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_eval:: $(MAKEFILE_BASE_POSIX_MAK) $(COMPILE_DEPENDANT_MAKEFILES)
	$$(QUIET_PREFIX) $$(BUILD_DEBUG_ECHO) detect string change between builds $(1) eval incremental start
	$$(SPEW_UPDATED_DEPENDENCIES)
	$$(QUIET_PREFIX) \
    { \
        { $$(MKDIR) -p $$(OBJ_DIR)/_detect_string_change_between_builds $$(QUIET_ECHO_POSTFIX) || $(TRUE); } && \
        $$(BUILD_DEBUG_ECHO) $(1) unchanged since last build && \
        $$(TOUCH) $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_eval; \
    }

endif


$$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated: $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_eval ; \
    $$(QUIET_PREFIX) \
    { \
        if [ ! -e "$$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated" ]; then \
        { \
            $$(TOUCH) $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated; \
        }; \
        fi; \
        $$(BUILD_DEBUG_ECHO) detect string change between builds $(1) has evaluated; \
    }

else

# if $(DISABLE_DETECT_STRING_CHANGE_BETWEEN_BUILDS) is defined to not do anything, always update the strings when any relevant makefile changes

$$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated: $(MAKEFILE_BASE_POSIX_MAK) $(COMPILE_DEPENDANT_MAKEFILES) ; \
    $$(QUIET_PREFIX) \
    {
        $$(BUILD_DEBUG_ECHO) detect string change between builds $(1) updated disabled start && \
        $$(TOUCH) $$(OBJ_DIR)/_detect_string_change_between_builds/$(1)_updated; \
    }

endif

endef

unexport DETECT_STRING_CHANGE_BETWEEN_BUILDS


# RUN_RECIPE_ACTION_ONCE ensures you run a recipe action exactly one time among many parallel threads
#  and none of the recipes trying to run it return from the function until the action has completed
#
# Parameters: $(1) = unique "already run" sentinel file, $(2) = unique mutex directory, $(3) = action to run
#  The existence of the sentinel is used to note that the action has already run and should not run again. The contents of the file are a shell script to replciate the exit status of the action. You are responsible for ensuring this file is deleted before the action will run again
#  The existence of the mutex directory causes parallel recipes to spin until it is removed (which automatically happens at the end of the function). You are responsible for ensuring this directory is deleted before any locks can be acquired
#   "_prebuild_always::" is a good place to remove both files.
#  You are also responsible for ensuring the base directories for both items exist prior to calling RUN_RECIPE_ACTION_ONCE
#
# The sentinel file is used because shell scripts cannot promote environment variables to the parent make process, so the filesystem is used to create a boolean flag out of the sentinel file
# The mutex directory is used because directory creation is atomic and will return an error if the directory cannot be created (already exists)
RUN_RECIPE_ACTION_ONCE = \
	( \
		until [ -e $(1) ]; do \
		{ \
			$(MKDIR) -p $(2) > /dev/null 2>&1 && \
			{ \
				! [ -e $(1) ] && \
				{ \
					{ $(3); } && \
					$(ECHO) "exit 0" > $(1) || \
					$(ECHO) "exit 1" > $(1); \
				}; \
				$(RM) -fr $(2); \
			} \
			|| \
			{ \
				$(SLEEP) 1; \
			}; \
		}; \
		done; \
		$(SHELL) $(1); \
	)



#
# Standard directory creation targets to ensure we can just touch files we need knowing that their directory exists
#

# Ensure $(OBJ_DIR) exists
$(OBJ_DIR)/_create_dir:
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) $(OBJ_DIR)/_create_dir start
	$(QUIET_PREFIX) $(MKDIR) -p $(OBJ_DIR) $(QUIET_ECHO_POSTFIX)
	$(QUIET_PREFIX) $(TOUCH) $(OBJ_DIR)/_create_dir


PREBUILD_EVENT_ACTION ?= { $(TRUE); }
PREBUILD_EVENT_WRAPPED = \
	{ \
		{ \
			$(BUILD_DEBUG_ECHO) "Pre-Build Event For \"$(NAME)\"" && \
			$(call PREBUILD_EVENT_ACTION); \
		} \
		|| \
		{ \
			$(ECHO) "Error executing Pre-Build Event for \"$(NAME)\""; \
			$(FALSE); \
		}; \
	}

RUN_PREBUILD_EVENT_ONCE = $(call RUN_RECIPE_ACTION_ONCE,"$(OBJ_DIR)/_ran_prebuild_event","$(OBJ_DIR)/_lock_prebuild_event",$(call PREBUILD_EVENT_WRAPPED))

_prebuild_always::
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _prebuild_always delete prebuild event start
	$(QUIET_PREFIX) $(RM) -fr $(OBJ_DIR)/_ran_prebuild_event
	$(QUIET_PREFIX) $(RM) -fr $(OBJ_DIR)/_lock_prebuild_event

# Analogous to MSVC Pre-Link Event
PRELINK_EVENT_ACTION ?= { $(TRUE); }

$(OBJ_DIR)/_prelink_event: $(LINK_STEP_DEPENDENCIES) | _prebuild_steps _predepgen_steps _precompile_steps
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _prelink_event start
	$(SPEW_UPDATED_DEPENDENCIES)
	$(QUIET_PREFIX) \
		{ \
			{ \
				$(call RUN_PREBUILD_EVENT_ONCE) && \
				$(BUILD_DEBUG_ECHO) "Pre-Link Event For \"$(NAME)\"" && \
				$(call PRELINK_EVENT_ACTION) && \
				$(TOUCH) $(OBJ_DIR)/_prelink_event && \
				$(RM) -f $(OBJ_DIR)/_prelink_event_failed; \
			} \
			|| \
			{ \
				$(ECHO) "Error executing Pre-Link Event for \"$(NAME)\""; \
				$(RM) -f $(OBJ_DIR)/_prelink_event; \
				$(TOUCH) $(OBJ_DIR)/_prelink_event_failed; \
				$(FALSE); \
			}; \
		}

# Analogous to MSVC Post-Build Event
POSTBUILD_EVENT_ACTION ?= { $(TRUE); }

$(OBJ_DIR)/_postbuild_event: $(ALL_CUSTOM_BUILD_TOOL_OUTPUTS) $(LINK_STEP) $(wildcard $(OBJ_DIR)/_postbuild_event_failed) | _prebuild_steps
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _postbuild_event start
	$(SPEW_UPDATED_DEPENDENCIES)
	$(QUIET_PREFIX) \
		{ \
			{ \
				$(call RUN_PREBUILD_EVENT_ONCE) && \
				$(BUILD_DEBUG_ECHO) "Post-Build Event For \"$(NAME)\"" && \
				$(call POSTBUILD_EVENT_ACTION) && \
				$(TOUCH) $(OBJ_DIR)/_postbuild_event && \
				$(RM) -f $(OBJ_DIR)/_postbuild_event_failed; \
			} \
			|| \
			{ \
				$(ECHO) "Error executing Post-Build Event for \"$(NAME)\""; \
				$(RM) -f $(OBJ_DIR)/_postbuild_event; \
				$(TOUCH) $(OBJ_DIR)/_postbuild_event_failed; \
				$(FALSE); \
			}; \
		}

# Everything that should run before anything starts generating intermediate files
_prebuild_steps: _prebuild_always $(OBJ_DIR)/_create_dir
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _prebuild_steps completed

# Everything that needs to run before depgen. Running after custom build tools because they can generate cpp's we depend on
_predepgen_steps: _prebuild_steps $(ALL_CUSTOM_BUILD_TOOL_OUTPUTS)
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _predepgen_steps completed

# Everything that needs to finish before compiling cpps.
_precompile_steps: _predepgen_steps
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _precompile_steps completed

# Everything that needs to finish before linking
_prelink_steps: $(OBJ_DIR)/_prelink_event
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _prelink_steps completed

# Everything to do after linking
_postbuild_steps: $(OBJ_DIR)/_postbuild_event
	$(QUIET_PREFIX) $(BUILD_DEBUG_ECHO) _postbuild_steps completed
