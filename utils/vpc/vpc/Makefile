# Make command to use for dependencies
SHELL=/bin/sh
RM:=rm
MKDIR:=mkdir
OS:=$(shell uname)
EXE_POSTFIX:=""


# ---------------------------------------------------------------- #
# Figure out if we're building in the Steam tree or not.
# ---------------------------------------------------------------- #

SRCROOT:=../../../
-include $(SRCROOT)/devtools/steam_def.mak


# ---------------------------------------------------------------- #
# Set paths to gcc.
# ---------------------------------------------------------------- #

CC:=gcc
CXX:=g++

ifeq ($(OS),Darwin)
SDKROOT:=$(shell xcodebuild -sdk macosx -version Path)
CC:=clang -m32
CXX:=clang++ -m32
EXE_POSTFIX:=_osx
endif

ifeq ($(OS),Linux)
ifeq ($(wildcard /valve/bin/gcc),)
CC:=gcc
CXX:=g++
else
CC:=/valve/bin/gcc-4.7
CXX:=/valve/bin/g++-4.7
endif
EXE_POSTFIX:=_linux
endif

ifneq ($(CC_OVERRIDE),)
		CC:=$(CC_OVERRIDE)
		CXX:=$(CPP_OVERRIDE)
endif


# ---------------------------------------------------------------- #
# Lists of files.
# ---------------------------------------------------------------- #

VPC_SRC:= \
	baseprojectdatacollector.cpp \
	clanggenerator.cpp \
	conditionals.cpp \
	configuration.cpp \
	../vpccrccheck/crccheck_shared.cpp \
	dependencies.cpp \
	../../../common/environment_utils.cpp \
	../../../common/bundled_module_info.cpp \
	generatordefinition.cpp \
	generated_files.cpp \
	groupscript.cpp \
	macros.cpp \
	main.cpp \
	pch_helpers.cpp \
	projectgenerator_android.cpp \
	projectgenerator_vcproj.cpp \
	projectgenerator_makefile.cpp \
	projectgenerator_win32.cpp \
	projectgenerator_win32_2010.cpp \
	projectgenerator_ps3.cpp \
	projectgenerator_xbox360.cpp \
	projectgenerator_xbox360_2010.cpp \
	projectscript.cpp \
	qtgenerator.cpp \
	schemagenerator.cpp \
	scriptsource.cpp \
	solutiongenerator_makefile.cpp \
	solutiongenerator_xcode.cpp \
	solutiongenerator_win32.cpp \
	sys_utils.cpp \
	../../../common/clang/clang_utils.cpp \
	unity.cpp

TIER0_SRC:= \
	../../../tier0/assert_dialog.cpp \
	../../../tier0/cpu_posix.cpp \
	../../../tier0/cpu.cpp \
	../../../tier0/dbg.cpp \
	../../../tier0/fasttimer.cpp \
	../../../tier0/mem.cpp \
	../../../tier0/mem_helpers.cpp \
	../../../tier0/memdbg.cpp \
	../../../tier0/memstd.cpp \
	../../../tier0/memvalidate.cpp \
	../../../tier0/minidump.cpp \
	../../../tier0/pch_tier0.cpp \
	../../../tier0/threadtools.cpp \
	../../../tier0/valobject.cpp \
	../../../tier0/vprof.cpp 


TIER1_SRC:= \
	../../../tier1/keyvalues.cpp \
	../../../tier1/checksum_crc.cpp \
	../../../tier1/checksum_md5.cpp \
	../../../tier1/convar.cpp \
	../../../tier1/generichash.cpp \
	../../../tier1/interface.cpp \
	../../../tier1/mempool.cpp \
	../../../tier1/memstack.cpp \
	../../../tier1/stringpool.cpp \
	../../../tier1/utlbuffer.cpp \
	../../../tier1/utlsymbol.cpp \
	../../../tier0/platform_posix.cpp \
	../../../tier0/pme_posix.cpp \
	../../../tier0/commandline.cpp \
	../../../tier0/win32consoleio.cpp \
	../../../tier0/logging.cpp \
	../../../tier0/tier0_strtools.cpp \
	../../../tier1/utlstring.cpp \
	../../../tier1/tier1.cpp \
	../../../tier1/characterset.cpp \
	../../../tier1/splitstring.cpp \
	../../../tier1/strtools.cpp \
	../../../tier1/strtools_unicode.cpp \
	../../../tier1/exprevaluator.cpp \

VSTDLIB_SRC:= \
	../../../vstdlib/cvar.cpp \
	../../../vstdlib/vstrtools.cpp \
	../../../vstdlib/random.cpp \
	../../../vstdlib/keyvaluessystem.cpp


INTERFACES_SRC= \
	../../../interfaces/interfaces.cpp



SRC:=$(VPC_SRC) $(TIER0_SRC) $(TIER1_SRC) $(VSTDLIB_SRC) $(INTERFACES_SRC) $(BINLAUNCH_SRC)


# -----Begin user-editable area-----

# -----End user-editable area-----

# If no configuration is specified, "Debug" will be used
ifndef "CFG"
CFG:=Release
endif


#
# Configuration: Debug
#
ifeq "$(CFG)" "Debug"

OUTDIR:=obj/$(OS)/debug
CONFIG_DEPENDENT_FLAGS:=-O0 -g3 -ggdb

else

OUTDIR:=obj/$(OS)/release
CONFIG_DEPENDENT_FLAGS:=-O3 -g1 -ggdb

endif

OBJS:=$(addprefix $(OUTDIR)/, $(subst ../../../, ,$(SRC:.cpp=.o)))


OUTFILE:=$(OUTDIR)/vpc
CFG_INC:=-I../../../public -I../../../common -I../../../public/tier0 \
	-I../../../public/tier1 -I../../../public/tier2 -I../../../public/vstdlib \
	-I../../../thirdparty/clang/include/ -I../../../thirdparty/SDL2/


CFLAGS=-std=c++11 -DMAKE_VPC -D_POSIX -DPOSIX -DGNUC -DNDEBUG $(CONFIG_DEPENDENT_FLAGS) -msse -mmmx -pipe -w -fpermissive -fPIC $(CFG_INC)
ifeq "$(STEAM_BRANCH)" "1"
CFLAGS+= -DSTEAM
endif


ifeq "$(OS)" "Darwin"
CFLAGS+=-I$(SDKROOT)/usr/include/malloc
CFLAGS+= -DOSX -D_OSX -DOSX64
CFLAGS+= -arch i386 -fasm-blocks
endif

ifeq "$(OS)" "Linux"
CFLAGS+= -DPLATFORM_LINUX -D_LINUX -DLINUX -DLINUXSTEAMRT64
endif

ifeq ($(CYGWIN),1)
CFLAGS+=-D_CYGWIN -DCYGWIN -D_CYGWIN_WINDOWS_TARGET
endif

CFLAGS+= -DCOMPILER_GCC

# the sed magic here adds the dependency file to the list of things that depend on the computed dependency
# set, so if any of them change, the dependencies are re-made
MAKEDEPEND=$(CXX) -M -MT $@ -MM $(CFLAGS) $< | sed -e 's@^\(.*\)\.o:@\1.d \1.o:@' > $(@:.o=.d)
COMPILE=$(CXX) -c $(CFLAGS) -o $@ $<
LINK=$(CXX) $(CONFIG_DEPENDENT_FLAGS)  -o "$(OUTFILE)" $(OBJS) -ldl -lpthread

ifeq "$(OS)" "Darwin"
LINK+=-liconv -framework Foundation
endif

ifeq "$(OS)" "Darwin"
LINK+= -arch i386
endif


# Build rules
all: $(OUTFILE) ../../../devtools/bin/vpc$(EXE_POSTFIX)

../../../devtools/bin/vpc$(EXE_POSTFIX) : $(OUTFILE)
	cp "$(OUTFILE)" ../../../devtools/bin/vpc$(EXE_POSTFIX)

$(OUTFILE): Makefile $(OBJS)
	$(LINK)


# Rebuild this project
rebuild: cleanall all

show:
	echo $(OUTDIR)
	echo $(OBJS)

# Clean this project
clean:
	$(RM) -f $(OUTFILE)
	$(RM) -f $(OBJS)
	$(RM) -f $(OBJS:.o=.d)
	$(RM) -f ../../../devtools/bin/vpc$(EXE_POSTFIX)

# Clean this project and all dependencies
cleanall: clean

# magic rules - tread with caution
-include $(OBJS:.o=.d)

# Pattern rules
$(OUTDIR)/%.o : %.cpp
	-$(MKDIR) -p $(@D)
	@$(MAKEDEPEND);
	$(COMPILE)

$(OUTDIR)/common/%.o : ../../../common/%.cpp
	-$(MKDIR) -p $(@D)
	@$(MAKEDEPEND);
	$(COMPILE)

$(OUTDIR)/tier0/%.o : ../../../tier0/%.cpp
	-$(MKDIR) -p $(@D)
	@$(MAKEDEPEND);
	$(COMPILE)

$(OUTDIR)/tier1/%.o : ../../../tier1/%.cpp
	-$(MKDIR) -p $(@D)
	@$(MAKEDEPEND);
	$(COMPILE)

$(OUTDIR)/vstdlib/%.o : ../../../vstdlib/%.cpp
	-$(MKDIR) -p $(@D)
	@$(MAKEDEPEND);
	$(COMPILE)

$(OUTDIR)/interfaces/%.o : ../../../interfaces/%.cpp
	if [ ! -d $(@D) ]; then $(MKDIR) $(@D); fi
	@$(MAKEDEPEND);
	$(COMPILE)

$(OUTDIR)/utils/binlaunch/%.o : ../../../binlaunch/%.cpp
	if [ ! -d $(@D) ]; then $(MKDIR) $(@D); fi
	@$(MAKEDEPEND);
	$(COMPILE)


# the tags file) seems like more work than it's worth.  feel free to fix that up
# if it bugs you. 
TAGS:
	@find . -name '*.cpp' -print0 | xargs -0 etags --declarations --ignore-indentation
	@find . -name '*.h' -print0 | xargs -0 etags --language=c++ --declarations --ignore-indentation --append
	@find . -name '*.c' -print0 | xargs -0 etags --declarations --ignore-indentation --append


