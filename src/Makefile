# MWRecompile - headless MWScript recompiler built from MWEdit's compiler.
# Cross-compiles a static Windows console exe with mingw-w64.
CXX      := x86_64-w64-mingw32-g++-posix
WINDRES  := x86_64-w64-mingw32-windres
OBJDIR   := objs
CXXFLAGS := -MMD -MP -std=gnu++20 -O2 -fpermissive -w -DNDEBUG -D_CONSOLE -DMWRECOMPILE \
            -Icli/stub -Icli -Ivendor -finput-charset=cp1252


COMMON := Common/Contain/GenStack.cpp Common/Contain/PtrArray.cpp Common/Contain/temarray.cpp \
          Common/File/GenFile.cpp Common/File/GenFind.cpp Common/String/SSArray.cpp Common/String/SString.cpp \
          Common/Utility/namelist.cpp Common/dl_base.cpp Common/dl_block.cpp Common/dl_chr.cpp Common/dl_err.cpp \
          Common/dl_file.cpp Common/dl_log.cpp Common/dl_math.cpp Common/dl_mem.cpp Common/dl_str.cpp Common/dl_time.cpp
ESM    := $(filter-out esm/EsmWinUtils.cpp esm/mwcommon.cpp,$(patsubst vendor/%,%,$(wildcard vendor/esm/*.cpp)))
PROJ   := project/EsmScriptCompile.cpp project/EsmScriptCompileEx.cpp project/EsmScriptDefs.cpp \
          project/EsmScriptError.cpp project/EsmScriptFuncs.cpp project/EsmScrFuncArray.cpp \
          project/EsmScrFuncData.cpp project/mwcustomfunc.cpp
VSRC   := $(addprefix vendor/,$(COMMON) $(ESM) $(PROJ))
CSRC   := cli/stub/project/MWEditDoc.cpp cli/stub/project/EsmGlobOptions.cpp $(wildcard cli/src/*.cpp)
SRCS   := $(VSRC) $(CSRC)
OBJS   := $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))

all: MWRecompile.exe

MWRecompile.exe: $(OBJS)
	$(CXX) -o $@ $^ -static -static-libgcc -static-libstdc++ -mconsole

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) MWRecompile.exe
.PHONY: all clean
-include $(OBJS:.o=.d)
