/* MWRecompile: implementation of the headless CMWEditDoc stand-in. */
#include "project/MWEditDoc.h"
#include <cctype>

std::string CMWEditDoc::Key(const TCHAR *pID) {
	std::string s;
	if (pID == NULL) return s;
	const TCHAR *p = pID;
	size_t n = strlen(p);
	/* Remove surrounding ID quotes as MWEdit does */
	if (n > 0 && *p == '"') {
		++p; --n;
		if (n > 0 && p[n - 1] == '"') --n;
	}
	s.assign(p, n);
	for (auto &c : s) c = (char)tolower((unsigned char)c);
	return s;
}

CMWEditDoc::~CMWEditDoc() {
	for (auto *p : m_All) delete p;
}

void CMWEditDoc::AddFile(CEsmFile *pFile) {
	for (int Index = 0; Index < pFile->GetNumRecords(); ++Index) {
		CEsmRecord *pRecord = pFile->GetRecord(Index);
		if (pRecord == NULL || pRecord->IsType(MWESM_REC_TES3)) continue;
		std::string k = Key(pRecord->GetID());
		auto &vec = m_ById[k];
		esmrecinfo_t *pInfo = NULL;
		for (auto *p : vec) {
			if (p->pRecord->IsType(pRecord->GetType())) { pInfo = p; break; }
		}
		if (pInfo == NULL) {
			pInfo = new esmrecinfo_t;
			pInfo->UserData = 0;
			m_All.push_back(pInfo);
			vec.push_back(pInfo);
			pRecord->SetPrevRecord(NULL);
		} else {
			pRecord->SetPrevRecord(pInfo->pRecord);
		}
		pInfo->pFile = pFile;
		pInfo->pRecord = pRecord;
		m_Last[k] = pInfo;
	}
}

esmrecinfo_t *CMWEditDoc::FindRecord(const TCHAR *pID) {
	auto it = m_Last.find(Key(pID));
	return it == m_Last.end() ? NULL : it->second;
}

esmrecinfo_t *CMWEditDoc::FindRecord(const TCHAR *pID, const TCHAR *pType) {
	auto it = m_ById.find(Key(pID));
	if (it == m_ById.end()) return NULL;
	for (auto *p : it->second) if (p->pRecord->IsType(pType)) return p;
	return NULL;
}

esmrecinfo_t *CMWEditDoc::FindRecordCarryable(const TCHAR *pID) {
	auto it = m_ById.find(Key(pID));
	if (it == m_ById.end()) return NULL;
	esmrecinfo_t *pLast = FindRecord(pID);
	if (pLast && IsESMRecordCarryable(pLast->pRecord->GetType())) return pLast;
	for (auto *p : it->second) if (IsESMRecordCarryable(p->pRecord->GetType())) return p;
	return NULL;
}

CEsmGlobal *CMWEditDoc::GetGlobal(const TCHAR *pName) {
	esmrecinfo_t *p = FindRecord(pName, MWESM_REC_GLOB);
	return p ? (CEsmGlobal *)p->pRecord : NULL;
}

static bool IsObjectType(const TCHAR *pType) {
	static const char *types[] = {"ACTI", "ALCH", "APPA", "ARMO", "BODY", "BOOK", "CLOT", "CONT", "CREA", "DOOR",
	                              "INGR", "LEVC", "LEVI", "LIGH", "LOCK", "MISC", "NPC_", "PROB", "REPA", "STAT",
	                              "WEAP", NULL};
	for (int i = 0; types[i]; ++i) if (strncmp(pType, types[i], 4) == 0) return true;
	return false;
}

esmrecinfo_t *CMWEditDoc::FindObjectRecord(const TCHAR *pID) {
	auto it = m_ById.find(Key(pID));
	if (it == m_ById.end()) return NULL;
	for (auto *p : it->second) if (IsObjectType(p->pRecord->GetType())) return p;
	return NULL;
}
