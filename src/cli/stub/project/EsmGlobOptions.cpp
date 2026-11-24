/* MWRecompile: option accessors normally backed by MWEdit's registry/INI options. */
#include "project/StdAfx.h"
#include "project/EsmGlobOptions.h"
#include "project/EsmScriptCompile.h"
#include "cli_options.h"

mwrecompile_options_t g_MwrOptions;

bool GetEsmOptNoScriptFormat() { return true; }
bool GetEsmOptBackupSaves() { return false; }
bool GetEsmOptNoScriptPrompt() { return true; }
bool GetEsmOptAllowExtFuncs() { return g_MwrOptions.AllowExtFuncs; }
bool GetEsmOptUseExtraFile() { return g_MwrOptions.UseExtraFile; }
bool GetEsmOptStrictIDs() { return false; }
bool GetEsmOptInitialIndentLevel() { return false; }
bool GetEsmOptIndentCommentsMore() { return false; }
bool GetEsmOptAllowBloodmoon() { return g_MwrOptions.AllowBloodmoon; }
bool GetEsmOptAllowTribunal() { return g_MwrOptions.AllowTribunal; }
int GetEsmOptScriptWarnLevel() { return g_MwrOptions.WarnLevel; }
int GetEsmOptScriptFormatType() { return ESMSCR_FORMAT_WHITE; }
const TCHAR *GetEsmOptDefaultAuthor() { return _T(""); }
const TCHAR *GetEsmOptExtraFile() { return g_MwrOptions.ExtraFile.c_str(); }
const TCHAR *GetEsmOptScriptIndentString() { return _T("\t"); }
CEsmScriptOptions *GetEsmOptScriptOptions() { return NULL; }
