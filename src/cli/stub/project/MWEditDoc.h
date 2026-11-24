/*===========================================================================
 * MWRecompile: headless stand-in for MWEdit's CMWEditDoc (MFC CDocument).
 * Only the record lookups the script compiler needs are provided. Records
 * are indexed from a load order of CEsmFile objects (masters first, the
 * target plugin last); later files override earlier ones, as in MWEdit.
 *=========================================================================*/
#ifndef __MWEditDoc_H
#define __MWEditDoc_H

#include "esm/EsmFile.h"
#include "esm/EsmGlobal.h"
#include <string>
#include <vector>
#include <unordered_map>

#define MWEDIT_ERR_COMPILEALL 10931

class CMWEditDoc {
  public:
	CMWEditDoc() {}
	~CMWEditDoc();

	/* Index every record of the file (call in load order) */
	void AddFile(CEsmFile *pFile);

	esmrecinfo_t *FindRecord(const TCHAR *pID);
	esmrecinfo_t *FindRecord(const TCHAR *pID, const TCHAR *pType);
	esmrecinfo_t *FindRecordCarryable(const TCHAR *pID);
	/* MWRecompile: placeable object (NPC, door, activator...) with this ID, ignoring DIAL/CELL/etc. */
	esmrecinfo_t *FindObjectRecord(const TCHAR *pID);
	CEsmGlobal *GetGlobal(const TCHAR *pName);

  private:
	std::vector<esmrecinfo_t *> m_All;
	/* lower-case id -> every record info with that id (one per type) */
	std::unordered_map<std::string, std::vector<esmrecinfo_t *>> m_ById;
	/* lower-case id -> most recently added record info (MWEdit's hash map behaviour) */
	std::unordered_map<std::string, esmrecinfo_t *> m_Last;

	static std::string Key(const TCHAR *pID);
};

#endif
