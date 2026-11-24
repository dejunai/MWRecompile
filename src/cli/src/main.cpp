/*===========================================================================
 *
 * MWRecompile - headless Morrowind MWScript recompiler.
 *
 * Recompiles the SCTX (source) of every SCPT record in a plugin/master into
 * SCDT (bytecode), SCHD (header counts/sizes) and SCVR (local names) using
 * the script compiler from Dave Humphrey's MWEdit (via the Walrus-Tech
 * MWEdit 0.7.0 code base). Everything except the rewritten script
 * sub-records is copied through byte-for-byte.
 *
 * MWRecompile by dej (dejunai), built with Grok Bot (Code Review agent).
 * Copyright (c) 2026 dej (dejunai). MWRecompile's own code and changes are
 * MIT licensed; the MWEdit code stays under its original MIT terms
 * (c) 2025 Walrus Tech. See LICENSE.txt.
 *
 *=========================================================================*/
#include "project/StdAfx.h"
#include "project/MWEditDoc.h"
#include "project/EsmScriptCompile.h"
#include "esm/EsmScript.h"
#include "cli_options.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>

#define MWRECOMPILE_VERSION "1.0.0"

/*---------------------------------------------------------------------------
 * Raw ESP model: records are kept as raw bytes, only SCPT records are split
 * into sub-records so their compiled data can be replaced.
 *-------------------------------------------------------------------------*/
struct RawSub {
	std::string type;
	std::string data;
};

struct RawRec {
	std::string type;
	uint32_t unknown = 0;
	uint32_t flags = 0;
	std::string body;            /* raw sub-record bytes */
	std::vector<RawSub> subs;    /* parsed for SCPT/TES3 only */
	bool parsed = false;
	bool modified = false;
};

static uint32_t rd32(const char *p) {
	uint32_t v;
	memcpy(&v, p, 4);
	return v;
}

static void wr32(std::string &s, uint32_t v) {
	s.append((const char *)&v, 4);
}

static bool ReadWholeFile(const std::string &path, std::string &out) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	out.resize(n > 0 ? (size_t)n : 0);
	bool ok = n <= 0 || fread(&out[0], 1, (size_t)n, f) == (size_t)n;
	fclose(f);
	return ok;
}

static bool ParseSubs(RawRec &r, std::string &err) {
	r.subs.clear();
	size_t p = 0;
	while (p < r.body.size()) {
		if (p + 8 > r.body.size()) { err = "truncated sub-record header in " + r.type; return false; }
		RawSub s;
		s.type.assign(r.body.data() + p, 4);
		uint32_t sz = rd32(r.body.data() + p + 4);
		p += 8;
		if (p + sz > r.body.size()) { err = "truncated sub-record " + s.type + " in " + r.type; return false; }
		s.data.assign(r.body.data() + p, sz);
		p += sz;
		r.subs.push_back(std::move(s));
	}
	r.parsed = true;
	return true;
}

static bool ParseEsp(const std::string &buf, std::vector<RawRec> &recs, std::string &err) {
	size_t p = 0;
	while (p < buf.size()) {
		if (p + 16 > buf.size()) { err = "truncated record header at offset " + std::to_string(p); return false; }
		RawRec r;
		r.type.assign(buf.data() + p, 4);
		uint32_t sz = rd32(buf.data() + p + 4);
		r.unknown = rd32(buf.data() + p + 8);
		r.flags = rd32(buf.data() + p + 12);
		p += 16;
		if (p + sz > buf.size()) { err = "truncated record " + r.type + " at offset " + std::to_string(p - 16); return false; }
		r.body.assign(buf.data() + p, sz);
		p += sz;
		if (r.type == "SCPT" || r.type == "TES3") {
			if (!ParseSubs(r, err)) return false;
		}
		recs.push_back(std::move(r));
	}
	if (recs.empty() || recs[0].type != "TES3") { err = "not a TES3 plugin/master (no TES3 header)"; return false; }
	return true;
}

static std::string SerializeEsp(std::vector<RawRec> &recs) {
	std::string out;
	for (auto &r : recs) {
		if (r.modified) {
			r.body.clear();
			for (auto &s : r.subs) {
				r.body.append(s.type.data(), 4);
				wr32(r.body, (uint32_t)s.data.size());
				r.body += s.data;
			}
		}
		out.append(r.type.data(), 4);
		wr32(out, (uint32_t)r.body.size());
		wr32(out, r.unknown);
		wr32(out, r.flags);
		out += r.body;
	}
	return out;
}

static RawSub *FindSub(RawRec &r, const char *type) {
	for (auto &s : r.subs) if (s.type == type) return &s;
	return nullptr;
}

static std::string CStr(const std::string &d) {
	size_t n = d.find('\0');
	return n == std::string::npos ? d : d.substr(0, n);
}

static std::string Lower(std::string s) {
	for (auto &c : s) c = (char)tolower((unsigned char)c);
	return s;
}

static bool FileExists(const std::string &p) {
	DWORD a = GetFileAttributesA(p.c_str());
	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string DirName(const std::string &p) {
	size_t n = p.find_last_of("\\/");
	return n == std::string::npos ? std::string(".") : p.substr(0, n);
}

static std::string JoinPath(const std::string &d, const std::string &f) {
	if (d.empty()) return f;
	char c = d.back();
	return (c == '\\' || c == '/') ? d + f : d + "\\" + f;
}

/* Find a file in a directory case-insensitively (Windows is already case-insensitive, Wine mostly) */
static std::string FindInDir(const std::string &dir, const std::string &name) {
	std::string p = JoinPath(dir, name);
	if (FileExists(p)) return p;
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(JoinPath(dir, "*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return "";
	std::string want = Lower(name), found;
	do {
		if (Lower(fd.cFileName) == want) { found = JoinPath(dir, fd.cFileName); break; }
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	return found;
}

/*---------------------------------------------------------------------------
 * Script compile result
 *-------------------------------------------------------------------------*/
struct CompileOut {
	bool ok = false;
	int numShorts = 0, numLongs = 0, numFloats = 0;
	std::string data;   /* SCDT */
	std::string vars;   /* SCVR */
	std::string beginName;
	std::vector<std::string> errors, warnings;
};

static void Compile(CMWEditDoc &doc, const std::string &text, CompileOut &out) {
	CEsmScriptCompile c;
	c.SetDocument(&doc);
	c.SetMsgLevel(g_MwrOptions.WarnLevel);
	/* The compiler splits lines on CR; accept LF-only or CR-only text from external editors */
	std::string norm;
	norm.reserve(text.size() + 64);
	for (size_t i = 0; i < text.size(); ++i) {
		char ch = text[i];
		if (ch == '\r') {
			norm += "\r\n";
			if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
		} else if (ch == '\n') {
			norm += "\r\n";
		} else {
			norm += ch;
		}
	}
	c.SetScriptText(norm.data(), (int)norm.size());
	int res = c.Compile();
	out.ok = res >= 0 && c.GetNumErrors() == 0;
	out.numShorts = c.GetNumShortLocals();
	out.numLongs = c.GetNumLongLocals();
	out.numFloats = c.GetNumFloatLocals();
	out.data.assign(c.GetScriptData(), c.GetScriptDataSize());
	if (c.GetLocalVarDataSize() > 0 && c.GetLocalVarData())
		out.vars.assign(c.GetLocalVarData(), c.GetLocalVarDataSize());
	out.beginName = c.GetScriptName();
	CEsmScriptErrArray *pErrs = c.GetErrorArray();
	for (int i = 0; i < pErrs->GetNumElements(); ++i) {
		CEsmScriptError *e = pErrs->GetAt(i);
		char buf[1200];
		snprintf(buf, sizeof(buf), "line %d, col %d: %s", e->GetLine(), e->GetCharacter(), e->GetMessage());
		if (e->IsError()) out.errors.push_back(buf);
		else out.warnings.push_back(buf);
	}
	if (!out.ok && out.errors.empty()) {
		const TCHAR *pMsg = c.GetLastErrorMessage();
		out.errors.push_back(pMsg && *pMsg ? std::string(pMsg) : std::string("compile failed (no message from compiler)"));
	}
}

/* Apply a compile result to the in-memory MWEdit record so other scripts see the new locals */
static void ApplyToRecord(CEsmScript *pScript, const CompileOut &o) {
	if (pScript == NULL) return;
	CEsmSubSCHD *pHdr = pScript->GetScriptHeader();
	if (pHdr == NULL) pHdr = (CEsmSubSCHD *)pScript->AllocateSubRecord(MWESM_SUBREC_SCHD);
	if (pHdr) {
		pHdr->GetScriptHeadData()->LocalVarSize = (long)o.vars.size();
		pHdr->GetScriptHeadData()->ScriptDataSize = (long)o.data.size();
		pHdr->GetScriptHeadData()->NumShorts = o.numShorts;
		pHdr->GetScriptHeadData()->NumLongs = o.numLongs;
		pHdr->GetScriptHeadData()->NumFloats = o.numFloats;
	}
	CEsmSubRecord *pVars = pScript->GetScriptVars();
	if (pVars == NULL && !o.vars.empty()) pVars = pScript->AllocateSubRecord(MWESM_SUBREC_SCVR);
	if (pVars) pVars->CopyData((char *)o.vars.data(), (int)o.vars.size());
}

/*---------------------------------------------------------------------------
 * Command line
 *-------------------------------------------------------------------------*/
static void Usage() {
	printf(
	"MWRecompile " MWRECOMPILE_VERSION " - headless Morrowind script recompiler (MWEdit compiler)\n"
	"Copyright (c) 2026 dej (dejunai). MIT License; free to redistribute.\n"
	"Based on MWEdit 0.7.0 (c) 2025 Walrus Tech (MIT); original MWEdit by Dave Humphrey.\n"
	"\n"
	"Usage: MWRecompile <plugin.esp|.esm> [options]\n"
	"\n"
	"Recompiles every script's source text (SCTX) into bytecode (SCDT), header\n"
	"(SCHD) and local variable list (SCVR). Other records are copied unchanged.\n"
	"\n"
	"Output:\n"
	"  -o, --output FILE     write the result to FILE (default: overwrite the input,\n"
	"                        after copying the original to <input>.bak)\n"
	"  --no-backup           do not create <input>.bak when overwriting in place\n"
	"  -n, --check           dry run: compile and report, write nothing\n"
	"  -k, --keep-going      write the file even if some scripts fail; failing\n"
	"                        scripts keep their existing compiled data (exit 1)\n"
	"\n"
	"Which scripts:\n"
	"  -s, --script ID       only recompile this script (repeatable)\n"
	"  --changed-from FILE   only recompile scripts whose source differs from (or is\n"
	"                        new relative to) the same plugin's original FILE\n"
	"  (default)             recompile all scripts; only those whose compiled\n"
	"                        data actually changes are rewritten\n"
	"\n"
	"Masters / lookups:\n"
	"  -d, --data DIR        folder to search for the plugin's masters (repeatable;\n"
	"                        default: the plugin's own folder)\n"
	"  -m, --master FILE     load this file as a master, in order (repeatable;\n"
	"                        replaces the automatic master list)\n"
	"  --extra-file FILE     also resolve IDs from this plugin (MWEdit 'extra file')\n"
	"  --mwse                allow MWSE/MWEdit extended functions (xGetRef etc.)\n"
	"  --no-tribunal         reject Tribunal-only functions\n"
	"  --no-bloodmoon        reject Bloodmoon-only functions\n"
	"  --warn weak|default|strong   compiler warning level (default: default)\n"
	"\n"
	"Reporting:\n"
	"  -l, --list            list every script with its status\n"
	"  -w, --warnings        print compiler warnings\n"
	"  --report FILE         write a tab-separated per-script report\n"
	"  -q, --quiet           only print errors\n"
	"  -h, --help            this help\n"
	"\n"
	"Exit codes: 0 ok, 1 compile errors (nothing written), 2 usage or file error.\n");
}

int main(int argc, char **argv) {
	std::string input, output, changedFrom, reportFile, textFile;
	std::vector<std::string> dataDirs, explicitMasters;
	std::set<std::string> onlyScripts;
	bool check = false, keepGoing = false, list = false, showWarn = false, quiet = false, noBackup = false;

	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		auto next = [&](const char *what) -> std::string {
			if (i + 1 >= argc) { fprintf(stderr, "error: %s needs a value\n", what); exit(2); }
			return argv[++i];
		};
		if (a == "-h" || a == "--help" || a == "/?") { Usage(); return 0; }
		else if (a == "--version") { printf("MWRecompile %s\n" "Copyright (c) 2026 dej (dejunai). MIT License; free to redistribute.\n" "Based on MWEdit 0.7.0 (c) 2025 Walrus Tech (MIT); original MWEdit by Dave Humphrey.\n", MWRECOMPILE_VERSION); return 0; }
		else if (a == "-o" || a == "--output") output = next("--output");
		else if (a == "-n" || a == "--check" || a == "--dry-run") check = true;
		else if (a == "--no-backup") noBackup = true;
		else if (a == "-k" || a == "--keep-going") keepGoing = true;
		else if (a == "-s" || a == "--script") onlyScripts.insert(Lower(next("--script")));
		else if (a == "--changed-from") changedFrom = next("--changed-from");
		else if (a == "-d" || a == "--data" || a == "--masters") dataDirs.push_back(next("--data"));
		else if (a == "-m" || a == "--master") explicitMasters.push_back(next("--master"));
		else if (a == "--extra-file") { g_MwrOptions.ExtraFile = next("--extra-file"); g_MwrOptions.UseExtraFile = true; }
		else if (a == "--mwse") g_MwrOptions.AllowExtFuncs = true;
		else if (a == "--no-tribunal") g_MwrOptions.AllowTribunal = false;
		else if (a == "--no-bloodmoon") g_MwrOptions.AllowBloodmoon = false;
		else if (a == "--warn") {
			std::string v = Lower(next("--warn"));
			if (v == "weak") g_MwrOptions.WarnLevel = ESMSCR_COMPILEMSG_WEAK;
			else if (v == "strong") g_MwrOptions.WarnLevel = ESMSCR_COMPILEMSG_STRONG;
			else g_MwrOptions.WarnLevel = ESMSCR_COMPILEMSG_DEFAULT;
		}
		else if (a == "-l" || a == "--list") list = true;
		else if (a == "-w" || a == "--warnings") showWarn = true;
		else if (a == "--report") reportFile = next("--report");
		else if (a == "--compile-text") textFile = next("--compile-text");
		else if (a == "-q" || a == "--quiet") quiet = true;
		else if (!a.empty() && a[0] == '-') { fprintf(stderr, "error: unknown option %s (see --help)\n", a.c_str()); return 2; }
		else if (input.empty()) input = a;
		else { fprintf(stderr, "error: more than one input file given\n"); return 2; }
	}
	if (input.empty()) { Usage(); return 2; }

	/* Read the target plugin raw */
	std::string buf, err;
	std::vector<RawRec> recs;
	if (!ReadWholeFile(input, buf)) { fprintf(stderr, "error: cannot read %s\n", input.c_str()); return 2; }
	if (!ParseEsp(buf, recs, err)) { fprintf(stderr, "error: %s: %s\n", input.c_str(), err.c_str()); return 2; }

	/* Resolve masters */
	std::vector<std::string> masterPaths;
	if (!explicitMasters.empty()) {
		for (auto &m : explicitMasters) {
			if (!FileExists(m)) { fprintf(stderr, "error: master not found: %s\n", m.c_str()); return 2; }
			masterPaths.push_back(m);
		}
	} else {
		if (dataDirs.empty()) dataDirs.push_back(DirName(input));
		for (auto &s : recs[0].subs) {
			if (s.type != "MAST") continue;
			std::string name = CStr(s.data), found;
			for (auto &d : dataDirs) { found = FindInDir(d, name); if (!found.empty()) break; }
			if (found.empty()) {
				fprintf(stderr, "error: master '%s' not found (searched:", name.c_str());
				for (auto &d : dataDirs) fprintf(stderr, " \"%s\"", d.c_str());
				fprintf(stderr, "); use -d <Data Files folder> or -m <file>\n");
				return 2;
			}
			masterPaths.push_back(found);
		}
	}

	/* Load masters and the plugin through MWEdit's record library for ID lookups */
	CMWEditDoc doc;
	std::vector<std::unique_ptr<CEsmFile>> files;
	for (auto &m : masterPaths) {
		if (!quiet) printf("Loading master %s\n", m.c_str());
		files.emplace_back(new CEsmFile);
		files.back()->SetMaster(true);
		if (!files.back()->Read(m.c_str())) { fprintf(stderr, "error: failed to load master %s\n", m.c_str()); return 2; }
		doc.AddFile(files.back().get());
	}
	files.emplace_back(new CEsmFile);
	CEsmFile *pPlugin = files.back().get();
	pPlugin->SetActive(true);
	if (!pPlugin->Read(input.c_str())) { fprintf(stderr, "error: MWEdit failed to load %s\n", input.c_str()); return 2; }
	doc.AddFile(pPlugin);

	if (g_MwrOptions.UseExtraFile) {
		if (!CEsmScriptCompile::GetExtraFile().Read(g_MwrOptions.ExtraFile.c_str())) {
			fprintf(stderr, "error: failed to load extra file %s\n", g_MwrOptions.ExtraFile.c_str());
			return 2;
		}
		CEsmScriptCompile::InitializeExtraRecords();
	}

	/* Debug/test mode: compile one script text file against the loaded files and hex-dump it */
	if (!textFile.empty()) {
		std::string t;
		if (!ReadWholeFile(textFile, t)) { fprintf(stderr, "error: cannot read %s\n", textFile.c_str()); return 2; }
		/* normalise to CR LF like the Construction Set stores it */
		std::string n;
		for (size_t i = 0; i < t.size(); ++i) {
			if (t[i] == '\r') continue;
			if (t[i] == '\n') n += "\r\n"; else n += t[i];
		}
		CompileOut o;
		Compile(doc, n, o);
		for (auto &e : o.errors) fprintf(stderr, "%s: %s\n", textFile.c_str(), e.c_str());
		for (auto &w : o.warnings) fprintf(stderr, "%s: warning: %s\n", textFile.c_str(), w.c_str());
		printf("name=%s ok=%d shorts=%d longs=%d floats=%d data=%zu vars=%zu\n", o.beginName.c_str(), o.ok ? 1 : 0,
		       o.numShorts, o.numLongs, o.numFloats, o.data.size(), o.vars.size());
		for (size_t i = 0; i < o.data.size(); ++i) printf("%02x%s", (unsigned char)o.data[i], (i % 32 == 31) ? "\n" : " ");
		printf("\n");
		return o.ok ? 0 : 1;
	}

	/* Original sources for --changed-from */
	std::map<std::string, std::string> origText;
	if (!changedFrom.empty()) {
		std::string obuf;
		std::vector<RawRec> orecs;
		if (!ReadWholeFile(changedFrom, obuf) || !ParseEsp(obuf, orecs, err)) {
			fprintf(stderr, "error: cannot read --changed-from file %s %s\n", changedFrom.c_str(), err.c_str());
			return 2;
		}
		for (auto &r : orecs) {
			if (r.type != "SCPT") continue;
			RawSub *h = FindSub(r, "SCHD"), *t = FindSub(r, "SCTX");
			if (h) origText[Lower(CStr(h->data.substr(0, 32)))] = t ? t->data : std::string();
		}
	}

	/* Map plugin script IDs to MWEdit records */
	std::map<std::string, CEsmScript *> pluginScripts;
	{
		int pos;
		for (CEsmRecord *p = pPlugin->FindFirst(MWESM_REC_SCPT, pos); p; p = pPlugin->FindNext(MWESM_REC_SCPT, pos))
			pluginScripts[Lower(p->GetID())] = (CEsmScript *)p;
	}

	/* Select scripts */
	struct Job { RawRec *rec; std::string id; std::string text; CompileOut out; std::string status; };
	std::vector<Job> jobs;
	int totalScripts = 0;
	for (auto &r : recs) {
		if (r.type != "SCPT") continue;
		++totalScripts;
		RawSub *h = FindSub(r, "SCHD"), *t = FindSub(r, "SCTX");
		if (!h || h->data.size() < 32) { fprintf(stderr, "warning: SCPT record without valid SCHD skipped\n"); continue; }
		std::string id = CStr(h->data.substr(0, 32));
		std::string lid = Lower(id);
		if ((r.flags & 0x20) != 0) continue; /* deleted record */
		if (!onlyScripts.empty() && !onlyScripts.count(lid)) continue;
		std::string text = t ? t->data : std::string();
		if (!changedFrom.empty()) {
			auto it = origText.find(lid);
			if (it != origText.end() && it->second == text) continue;
		}
		jobs.push_back({&r, id, text, {}, ""});
	}
	for (auto &s : onlyScripts) {
		bool found = false;
		for (auto &j : jobs) if (Lower(j.id) == s) found = true;
		if (!found) { fprintf(stderr, "error: script '%s' not found in %s\n", s.c_str(), input.c_str()); return 2; }
	}

	/* Pass 1: compile and publish new locals so cross-script references (Foo.bar) resolve */
	for (auto &j : jobs) {
		Compile(doc, j.text, j.out);
		if (j.out.ok) ApplyToRecord(pluginScripts[Lower(j.id)], j.out);
	}
	/* Pass 2: final compile against the updated locals */
	int nErr = 0, nChanged = 0, nSame = 0, nWarn = 0;
	for (auto &j : jobs) {
		j.out = CompileOut();
		Compile(doc, j.text, j.out);
		nWarn += (int)j.out.warnings.size();
		if (!j.out.ok) {
			++nErr;
			j.status = "ERROR";
			for (auto &e : j.out.errors) fprintf(stderr, "%s: %s: %s\n", input.c_str(), j.id.c_str(), e.c_str());
			continue;
		}
		if (showWarn) for (auto &w : j.out.warnings) fprintf(stderr, "%s: %s: warning: %s\n", input.c_str(), j.id.c_str(), w.c_str());
		if (!j.out.beginName.empty() && Lower(j.out.beginName) != Lower(j.id))
			fprintf(stderr, "%s: %s: warning: 'Begin %s' does not match the record ID; the record ID is kept\n",
			        input.c_str(), j.id.c_str(), j.out.beginName.c_str());

		RawRec &r = *j.rec;
		RawSub *h = FindSub(r, "SCHD");
		std::string newHdr = h->data;
		if (newHdr.size() < 52) newHdr.resize(52, '\0');
		uint32_t vals[5] = {(uint32_t)j.out.numShorts, (uint32_t)j.out.numLongs, (uint32_t)j.out.numFloats,
		                    (uint32_t)j.out.data.size(), (uint32_t)j.out.vars.size()};
		memcpy(&newHdr[32], vals, 20);

		RawSub *v = FindSub(r, "SCVR"), *d = FindSub(r, "SCDT");
		bool same = newHdr == h->data && (v ? v->data : std::string()) == j.out.vars &&
		            (d ? d->data : std::string()) == j.out.data && (d != nullptr || j.out.data.empty());
		if (same) { ++nSame; j.status = "unchanged"; continue; }

		/* Rebuild the SCPT sub-record list in CS order: SCHD, SCVR, SCDT, SCTX, others kept in place */
		std::vector<RawSub> subs;
		bool wroteVars = false, wroteData = false;
		for (auto &s : r.subs) {
			if (s.type == "SCHD") {
				subs.push_back({"SCHD", newHdr});
				if (!j.out.vars.empty()) { subs.push_back({"SCVR", j.out.vars}); }
				wroteVars = true;
				if (!d) { subs.push_back({"SCDT", j.out.data}); wroteData = true; }
			} else if (s.type == "SCVR") {
				if (!wroteVars && !j.out.vars.empty()) subs.push_back({"SCVR", j.out.vars});
				wroteVars = true;
			} else if (s.type == "SCDT") {
				if (!wroteData) subs.push_back({"SCDT", j.out.data});
				wroteData = true;
			} else {
				subs.push_back(s);
			}
		}
		r.subs.swap(subs);
		r.modified = true;
		++nChanged;
		j.status = "recompiled";
	}

	if (list) {
		for (auto &j : jobs)
			printf("  %-10s %s  (%d shorts, %d longs, %d floats, %zu bytes)\n", j.status.c_str(), j.id.c_str(),
			       j.out.numShorts, j.out.numLongs, j.out.numFloats, j.out.data.size());
	}
	if (!reportFile.empty()) {
		FILE *f = fopen(reportFile.c_str(), "wb");
		if (f) {
			fprintf(f, "script\tstatus\tshorts\tlongs\tfloats\tdata_size\tvar_size\tmessages\n");
			for (auto &j : jobs) {
				std::string msgs;
				for (auto &e : j.out.errors) msgs += (msgs.empty() ? "" : " | ") + e;
				for (auto &w : j.out.warnings) msgs += (msgs.empty() ? "warning: " : " | warning: ") + w;
				for (auto &c : msgs) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
				fprintf(f, "%s\t%s\t%d\t%d\t%d\t%zu\t%zu\t%s\n", j.id.c_str(), j.status.c_str(), j.out.numShorts,
				        j.out.numLongs, j.out.numFloats, j.out.data.size(), j.out.vars.size(), msgs.c_str());
			}
			fclose(f);
		}
	}

	if (!quiet)
		printf("%s: %d script(s) in file, %d compiled: %d recompiled/changed, %d unchanged, %d with errors, %d warning(s)\n",
		       input.c_str(), totalScripts, (int)jobs.size(), nChanged, nSame, nErr, nWarn);

	if (nErr > 0 && !keepGoing) {
		fprintf(stderr, "%d script(s) failed to compile; no file was written.\n", nErr);
		return 1;
	}
	if (nErr > 0)
		fprintf(stderr, "%d script(s) failed to compile; --keep-going: their existing compiled data is kept.\n", nErr);
	if (check) {
		if (!quiet) printf("Check only: nothing written.\n");
		return 0;
	}
	if (nChanged == 0 && output.empty()) {
		if (!quiet) printf("No changes; %s left untouched.\n", input.c_str());
		return 0;
	}

	std::string outPath = output.empty() ? input : output;
	std::string data = SerializeEsp(recs);
	if (output.empty() && !noBackup) {
		std::string bak = input + ".bak";
		if (!CopyFileA(input.c_str(), bak.c_str(), FALSE)) {
			fprintf(stderr, "error: could not create backup %s; nothing written\n", bak.c_str());
			return 2;
		}
		if (!quiet) printf("Backup written: %s\n", bak.c_str());
	}
	std::string tmp = outPath + ".mwrecompile.tmp";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (!f || fwrite(data.data(), 1, data.size(), f) != data.size()) {
		if (f) fclose(f);
		DeleteFileA(tmp.c_str());
		fprintf(stderr, "error: failed writing %s\n", tmp.c_str());
		return 2;
	}
	fclose(f);
	if (!MoveFileExA(tmp.c_str(), outPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
		DeleteFileA(tmp.c_str());
		fprintf(stderr, "error: could not replace %s\n", outPath.c_str());
		return 2;
	}
	if (!quiet) printf("Wrote %s (%zu bytes)\n", outPath.c_str(), data.size());
	return nErr > 0 ? 1 : 0;
}
