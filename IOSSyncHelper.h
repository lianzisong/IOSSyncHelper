
#include <string>
#include <list>

#include <time.h>

#include "plist/plist.h"
#include "libimobiledevice/libimobiledevice.h"
#include "libimobiledevice/mobilesync.h"
#include "libimobiledevice/afc.h"
#include "libimobiledevice/lockdown.h"

#ifndef HAVE_PLIST_LIB
#pragma comment(lib,"libplist.lib")
#endif

#ifndef NULL
#define NULL 0
#endif

class IOSSyncHelper
{

public:
	static std::string SYN_CONTACTS_TYPE ; 
	static std::string SYN_NOTE_TYPE;
	static std::string SYN_BOOKMARK_TYPE;
	static std::string SYN_CALENDAR_TYPE;

public:
	IOSSyncHelper();
	~IOSSyncHelper();

	//this api just only for the Contacts,Notes,Bookmark,Calendar
	int SyncData(std::string type);
	std::string Plist_to_XML(plist_t plist);
	std::list<plist_t> GetPlist();
	bool IsEncrypted();
	afc_error_t AFCPull(std::string path ,char **data_buf ,uint64_t *data_size);

	//if you want to get the photos ,use the AFCPull get the /PhotoData/Photos.sqlite ,then get the ZGENERICASSET table ,column ZDIRECTORY and ZFILENAME,
	//then you can know the photos path in the device .

	//////////////////////////////////////////////////////////////////////////
	//free memory
	void FreePlist(plist_t plist);
	
private:
	std::list<plist_t> m_plist;
};