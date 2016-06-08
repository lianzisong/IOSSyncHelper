
#include "IOSSyncHelper.h"


std::string IOSSyncHelper::SYN_CONTACTS_TYPE ="Contacts";
std::string IOSSyncHelper::SYN_NOTE_TYPE = "Notes";
std::string IOSSyncHelper::SYN_BOOKMARK_TYPE = "Bookmarks";
std::string IOSSyncHelper::SYN_CALENDAR_TYPE = "Calendars";

IOSSyncHelper::IOSSyncHelper()
{

}

IOSSyncHelper::~IOSSyncHelper()
{
	if (m_plist.size() >0)
	{
		for (auto it = m_plist.begin() ; it != m_plist.end() ; it++)
		{
			FreePlist(*it);
		}		
	}
}

void IOSSyncHelper::FreePlist(plist_t plist)
{
	if (plist != NULL)
	{
		plist_free(plist);
	}
}

int IOSSyncHelper::SyncData( std::string type )
{
	/*std::list<plist_t> plist;*/
	idevice_t device;
	mobilesync_client_t client;
	idevice_error_t deviceerr;
	mobilesync_error_t syncerr;
	deviceerr = idevice_new(&device,NULL);
	if (deviceerr != IDEVICE_E_SUCCESS )
	{
		return deviceerr;
	}

	syncerr = mobilesync_client_start_service(device,&client,"com.apple.mobilesync");
	if (syncerr != MOBILESYNC_E_SUCCESS)
	{
		return syncerr;
	}


	std::string strdataclass = "com.apple.";
	strdataclass += type;
	std::string strdata_device_anchor = type;
	strdata_device_anchor += "-Device-Anchor";
	mobilesync_anchors_t anchors;
	char dataclass[200] = {0};
	char data_device_anchor[200] = {0};
	char data_computer_anchor[200] = {0};
	uint64_t device_data_class_version;
	char* error_description;


	mobilesync_sync_type_t syn_type;
	memcpy(dataclass,strdataclass.c_str(),strdataclass.length());
	memcpy(data_device_anchor,strdata_device_anchor.c_str(),strdata_device_anchor.length());
	time_t time(0);
	strftime(data_computer_anchor,sizeof(data_computer_anchor),"%Y-%m-%d %H:%M:%S +0800",localtime(&time));


	anchors->device_anchor = data_device_anchor;
	anchors->computer_anchor = data_computer_anchor;
	//106 is for the contacts
	syncerr = mobilesync_start(client,dataclass,anchors,106,&syn_type,&device_data_class_version,&error_description);
	if (syncerr != MOBILESYNC_E_SUCCESS)
	{
		return syncerr;
	}

	syncerr = mobilesync_get_all_records_from_device(client);
	if (syncerr != MOBILESYNC_E_SUCCESS)
	{
		return syncerr;
	}

	while(true)
	{
		plist_t t_plist = NULL;
		plist_t response_type_node = NULL;
		char *response_type = NULL;
		syncerr = mobilesync_receive(client,&t_plist);
		if (syncerr != MOBILESYNC_E_SUCCESS)
		{
			break;
		}		

		response_type_node = plist_array_get_item(t_plist, 0);
		if (!response_type_node) {
			break;
		}

		plist_get_string_val(response_type_node, &response_type);
		if (!response_type) {
			break;
		}

		if (strcmp(response_type, "SDMessageDeviceReadyToReceiveChanges") == 0) {
			break;
		}

		if (strcmp(response_type, "SDMessageProcessChanges") != 0) {
			break;
		}

		m_plist.push_back(t_plist);

		//plist_free(t_plist);
		mobilesync_acknowledge_changes_from_device(client);
	}

	mobilesync_finish(client);
	mobilesync_client_free(client);
	client = NULL;

	idevice_free(device);
	device = NULL;

	return MOBILESYNC_E_SUCCESS;
}

std::string IOSSyncHelper::Plist_to_XML( plist_t plist )
{
	char* xml = NULL;
	uint32_t len = 0;
	std::string out;
	plist_to_xml(plist,&xml,&len);
	out = xml;
	free(xml);
	return out;
}

std::list<plist_t> IOSSyncHelper::GetPlist()
{
	return m_plist;
}

bool IOSSyncHelper::IsEncrypted()
{
	idevice_t device = NULL;
	lockdownd_client_t lockdown = NULL;
	lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
	plist_t node_tmp = NULL;
	uint8_t willEncrypt = 0;
	idevice_error_t deviceerr;


	deviceerr = idevice_new(&device, NULL);
	if (deviceerr != IDEVICE_E_SUCCESS) {
		idevice_free(device);
		return true;
	}
	if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(device, &lockdown, "IOSSyncHelper_IsEncrypted"))) {
		idevice_free(device);
		return true;
	}


	ldret = lockdownd_get_value(lockdown, "com.apple.mobile.backup", "WillEncrypt", &node_tmp);
	if (node_tmp) {
		if (plist_get_node_type(node_tmp) == PLIST_BOOLEAN) {
			plist_get_bool_val(node_tmp, &willEncrypt);
		}
		//plist_free(node_tmp);
		node_tmp = NULL;
	}
	idevice_free(device);
	lockdownd_client_free(lockdown);
	lockdown = NULL;
	return willEncrypt == 0 ? false : true;
}

afc_error_t IOSSyncHelper::AFCPull(std::string path ,char **data_buf ,uint64_t *data_size)
{
	afc_client_t afc = NULL;
	lockdownd_service_descriptor_t service = NULL;
	lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
	idevice_error_t ret = IDEVICE_E_UNKNOWN_ERROR;

	afc_error_t afcerr = AFC_E_UNKNOWN_ERROR;

	idevice_t device = NULL;
	lockdownd_client_t lockdown = NULL;

	ret = idevice_new(&device, NULL);
	if (ret != IDEVICE_E_SUCCESS) {
		printf("No device found, is it plugged in?\n");
		return afcerr;
	}
	
	if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(device, &lockdown, "IOSSyncHelper_AFCPull"))) {
		printf("ERROR: Could not connect to lockdownd, error code %d\n", ldret);
		idevice_free(device);
		return afcerr;
	}


	/* start AFC, we need this for the lock file */
	ldret = lockdownd_start_service(lockdown, AFC_SERVICE_NAME, &service);
	if ((ldret == LOCKDOWN_E_SUCCESS) && service->port) {
		afcerr = afc_client_new(device, service, &afc);
		if (afcerr != AFC_E_SUCCESS)
		{
			return afcerr;
		}
	}

	if (service) {
		lockdownd_service_descriptor_free(service);
		service = NULL;
	}

	if (lockdown)
	{
		lockdownd_client_free(lockdown);
		lockdown = NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	//char *data_buf = NULL;
	//uint64_t data_size = 0;

	char **fileinfo = NULL;
	uint32_t fsize = 0;

	afcerr = afc_get_file_info(afc, path.c_str(), &fileinfo);
	if (!fileinfo) {
		return afcerr;
	}

	for (int i = 0; fileinfo[i]; i+=2) 
	{
		if (!strcmp(fileinfo[i], "st_size")) 
		{
			fsize = atol(fileinfo[i+1]);
			break;
		}
	}

	afcerr = afc_dictionary_free(fileinfo);

	if (fsize == 0) {
		return afcerr;
	}

	uint64_t f = 0;
	afcerr = afc_file_open(afc, path.c_str(), AFC_FOPEN_RDONLY, &f);
	if (!f) {		
		return afcerr;
	}

	char *buf = (char*)malloc((uint32_t)fsize);
	uint32_t done = 0;

	while (done < fsize) {
		uint32_t bread = 0;
		afcerr = afc_file_read(afc, f, buf+done, 65536, &bread);
		if (afcerr != AFC_E_SUCCESS)
		{
			free(buf);
			afc_file_close(afc, f);
			return afcerr;
		}
		if (bread > 0) {
			done += bread;
		} else {
			break;
		}
	}
	if (done == fsize) {
		*data_size = fsize;
		*data_buf = buf;
	} else {
		free(buf);
	}
	afc_file_close(afc, f);

	return afcerr;
}



