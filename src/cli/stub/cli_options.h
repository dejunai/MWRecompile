/* MWRecompile global options (set from the command line). */
#pragma once
#include <string>
#include "project/EsmOptionsDef.h"
struct mwrecompile_options_t {
	bool AllowExtFuncs = false;
	bool UseExtraFile = false;
	bool AllowBloodmoon = true;
	bool AllowTribunal = true;
	int WarnLevel = ESMSCR_COMPILEMSG_DEFAULT;
	std::string ExtraFile;
};
extern mwrecompile_options_t g_MwrOptions;
