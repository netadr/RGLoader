// XtweakXam.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <xbdm.h>
#include <fstream>
#include <string>
#include <stdio.h>
#include "INIReader.h"
#include "xam.h"
#include "HUD.h"
#include "xshell.h"
#include "HvExpansion.h"
#include "OffsetManager.h"
// #include "sysext.h"

using namespace std;

static bool fKeepMemory = true;
static bool fExpansionEnabled = false;
static INIReader* reader;
static OffsetManager offsetmanager;

#define setmem(addr, data) { DWORD d = data; memcpy((LPVOID)addr, &d, 4);}

#define XexLoadExecutableOrd 408
#define XexLoadImageOrd 409
#define XEXLOADIMAGE_MAX_SEARCH 9

#define XEXLOAD_DASH    "\\Device\\Flash\\dash.xex"
#define XEXLOAD_DASH2   "\\SystemRoot\\dash.xex"
#define XEXLOAD_SIGNIN  "signin.xex"
#define XEXLOAD_CREATE  "createprofile.xex"
#define XEXLOAD_HUD	    "hud.xex"
#define XEXLOAD_XSHELL  "xshell.xex"
#define XEXLOAD_DEFAULT "default.xex"

/*void setmem(DWORD addr, DWORD data) {
	UINT64 d = data;
	if(addr < 0x40000)
	{
		// hv patch
		if(fExpansionEnabled)
		{
			printf("     (hv patch)\n");
			addr = addr | 0x8000000000000000ULL;
			BYTE* newdata = (BYTE*)XPhysicalAlloc(sizeof(DWORD), MAXULONG_PTR, 0, PAGE_READWRITE);
			memcpy(newdata, &d, sizeof(DWORD));
			writeHVPriv(newdata, addr, sizeof(DWORD));
			XPhysicalFree(newdata);
		}
		else
			printf("     (hv patch, but expansion didn't install :( )\n");
	}
	else
		DmSetMemory((LPVOID)addr, 4, &d, NULL);
}*/

BOOL ExpansionStuff() {
	/*
	0xC8007000 // address alignment fail
	0xC8007001 // size alignment fail
	0xC8007002 // magic/rsa sanity fail
	0xC8007003 // flags/size sanity fail
	0xC8007004 // inner header fail
	0xC8007005 // ...
	*/

	InfoPrint("Checking if the HVPP expansion is installed...\n");
	if (HvPeekWORD(0) != 0x5E4E) {
		// install signed and encrypted HVPP expansion
		InfoPrint("Installing HVPP expansion...\n");
		DWORD ret = InstallExpansion();
		if (ret != ERROR_SUCCESS) {
			InfoPrint("InstallExpansion: %04X\n", ret);
			return FALSE;
		}
		InfoPrint("Done!\n");
	}
	else
		InfoPrint("Expansion is already installed, skipping...\n");

	return TRUE;
}

BOOL CPUStuff() {
	InfoPrint("CPU key: ");
	BYTE CPUKeyHV[0x10];
	HvPeekBytes(0x20, CPUKeyHV, 0x10);
	HexPrint(CPUKeyHV, 0x10);
	printf("\n");

	return TRUE;
}

void PatchBlockLIVE(){
	InfoPrint(" * Blocking Xbox Live DNS\r\n");

	char* nullStr = "NO.%sNO.NO\0";
	DWORD nullStrSize = 18;

	XAMOffsets* offsets = offsetmanager.GetXAMOffsets();
	if(!offsets)
	{
		InfoPrint("Failed to load DNS offsets!\r\n");
		return;
	}

	// null out xbox live dns tags
	if(offsets->live_siflc)  //FIXME: check the others
		memcpy( (LPVOID)offsets->live_siflc, (LPCVOID)nullStr, nullStrSize);
	memcpy((LPVOID)offsets->live_piflc, (LPCVOID)nullStr, nullStrSize);
	memcpy((LPVOID)offsets->live_notice, (LPCVOID)nullStr, nullStrSize);
	memcpy((LPVOID)offsets->live_xexds, (LPCVOID)nullStr, nullStrSize);
	memcpy((LPVOID)offsets->live_xetgs, (LPCVOID)nullStr, nullStrSize);
	memcpy((LPVOID)offsets->live_xeas, (LPCVOID)nullStr, nullStrSize);
	memcpy((LPVOID)offsets->live_xemacs, (LPCVOID)nullStr, nullStrSize);
}

// Enable USBMASS0-2 in neighborhood
void PatchMapUSB(void) {

	XBDMOffsets* offsets = offsetmanager.GetXBDMOffsets();
	if(!offsets)
	{
		InfoPrint("Failed to load XBDM offsets!\n");
		return;
	}

	InfoPrint(" * Adding extra devices to xbox neighborhood\r\n");
	{	
		/*
		// dynamically map all the drives in the XBDMOffsets struct
		for (int x = 0; x < (sizeof(XBDMOffsets) - sizeof(DWORD)); x += (sizeof(DWORD) * 3)) {
			DWORD obname_ptr = reinterpret_cast<DWORD>(offsets + x);    // 0 - 4
			DWORD enable = reinterpret_cast<DWORD>(offsets + (x + 4));  // 4 - 8
			DWORD obname = reinterpret_cast<DWORD>(offsets + (x + 8));  // 8 - 12
			//setmem(obname_ptr, obname);
			//setmem(enable, 0x01);
			printf("0x%lx\n", obname_ptr);
			printf("0x%lx\n", enable);
			printf("0x%lx\n", obname);
		}
		//setmem((DWORD)(offsets + (sizeof(XBDMOffsets) - sizeof(DWORD))), 0x60000000);
		*/

		// add mass0 to xbn
		setmem(offsets->mass0_obname_ptr, offsets->mass0_obname);
		setmem(offsets->mass0_enable, 0x01);

		// add mass1 to xbn
		setmem(offsets->mass1_obname_ptr, offsets->mass1_obname);
		setmem(offsets->mass1_enable, 0x01);

		// add mass2 to xbn
		setmem(offsets->mass2_obname_ptr, offsets->mass2_obname);
		setmem(offsets->mass2_enable, 0x01);

		//add flash to xbn
		setmem(offsets->flash_obname_ptr, offsets->flash_obname);
		setmem(offsets->flash_enable, 0x01);

		//add hdd system ext partition  to xbn
		setmem(offsets->hddsysext_obname_ptr, offsets->hddsysext_obname);
		setmem(offsets->hddsysext_enable, 0x01);

		//add intusb system ext partition to xbn
		setmem(offsets->intusbsysext_obname_ptr, offsets->intusbsysext_obname);
		setmem(offsets->intusbsysext_enable, 0x01);

		//add intusb system ext partition to xbn
		setmem(offsets->hddsysaux_obname_ptr, offsets->hddsysaux_obname);
		setmem(offsets->hddsysaux_enable, 0x01);

		//add system partition to xbn
		setmem(offsets->y_obname_ptr, offsets->y_obname);
		setmem(offsets->y_enable, 0x01);

		//nop drivemap internal check (always be 1)
		setmem(offsets->map_internal_check, 0x60000000);
	}
}

//21076
// Changes the default dashboard
void PatchDefaultDash(string path) {
	InfoPrint(" * Reconfiguring default dash to: %s\r\n", path);
	
	ofstream dashxbx;

	//dashxbx.open("Hdd:\\Filesystems\14719-dev\dashboard.xbx", ofstream::out);
	dashxbx.open("Root:\\dashboard.xbx", ofstream::out);

	if(dashxbx.is_open()) {
		dashxbx << path;
		for(int i = path.length(); i < 0x100; i++)
			dashxbx << '\0';
		dashxbx.close();
	} else {
		InfoPrint("   ERROR: unable to write dashboard.xbx\r\n");
	}
}

bool StrCompare(char* one, char* two, int len) {
	for(int i = 0; i < len; i++){
		if(i > 0 && (one[i] == '\0' || two[i] == '\0'))
			return true; 
		if(one[i] != two[i])
			return false;
	}
	return true;
}

VOID __declspec(naked) XexpLoadImageSaveVar(VOID)
{
	__asm{
		li r3, 454 //make this unique for each hook
		nop
		nop
		nop
		nop
		nop
		nop
		blr
	}
}

NTSTATUS XexpLoadImageHook(LPCSTR xexName, DWORD typeInfo, DWORD ver, PHANDLE modHandle);
typedef NTSTATUS (*XEXPLOADIMAGEFUN)(LPCSTR xexName, DWORD typeInfo, DWORD ver, PHANDLE modHandle); // XexpLoadImage
static DWORD xexLoadOld[4];

int PatchHookXexLoad(void) {
	//printf(" * Hooking xeximageload for persistant patches\n");
	//hookImpStubDebug("xam.xex", "xboxkrnl.exe", XexLoadExecutableOrd, (DWORD)XexLoadExecutableHook);
	//hookImpStubDebug("xam.xex", "xboxkrnl.exe", XexLoadImageOrd, (DWORD)XexLoadImageHook);
	
	PDWORD xexLoadHookAddr = (PDWORD)FindInterpretBranchOrdinal("xboxkrnl.exe", XexLoadImageOrd, XEXLOADIMAGE_MAX_SEARCH);
	// InfoPrint("  - Found addr\r\n");
	if(xexLoadHookAddr != NULL)
	{
		//printf("  - Applying hook at %08X  with  save @ %08X\r\n", xexLoadHookAddr, (PDWORD)XexpLoadImageSaveVar);
		HookFunctionStart(xexLoadHookAddr, (PDWORD)XexpLoadImageSaveVar, xexLoadOld, (DWORD)XexpLoadImageHook);
	}

	return 1;
}

XEXPLOADIMAGEFUN XexpLoadImageSave = (XEXPLOADIMAGEFUN)XexpLoadImageSaveVar;
NTSTATUS XexpLoadImageHook(LPCSTR xexName, DWORD typeInfo, DWORD ver, PHANDLE modHandle) {
	NTSTATUS ret = XexpLoadImageSave(xexName, typeInfo, ver, modHandle);

	// DWORD tid = XamGetCurrentTitleId();

	// InfoPrint("New Title ID: 0x%04X\n", tid);

	if (ret >= 0) {
		if (stricmp(xexName, XEXLOAD_HUD) == 0) {
			// printf("\n\n ***RGLoader.xex*** \n   -Re-applying patches to: %s!\n\n", xexName);
			
			bool hudJumpToXShell = reader->GetBoolean("Expansion", "HudJumpToXShell", true);
			if (hudJumpToXShell) {
				// printf("     * Replacing family settings button with \"Jump to XShell\"");
				PatchHudReturnToXShell();
			}
		} else if (stricmp(xexName, XEXLOAD_XSHELL) == 0) {
			// printf("\n\n ***RGLoader.xex*** \n   -Re-applying patches to: %s!\n\n", xexName);
	
			string redirectXShellButton = reader->GetString("Config", "RedirectXShellButton", "none");
			if(redirectXShellButton != "none" && FileExists(redirectXShellButton.c_str())) {
				// printf("     * Remapping xshell start button to %s.\n\n", rTemp.c_str());
				PatchXShellStartPath(redirectXShellButton);
			}
		} else if (stricmp(xexName, XEXLOAD_SIGNIN) == 0) {
			//printf("\n\n ***RGLoader.xex*** \n   -Re-applying patches to: %s!\n", xexName);

			bool noSignInNotice = reader->GetBoolean("Config", "NoSignInNotice", false);
			if(noSignInNotice) {
				// printf("     * Disabling xbox live sign in notice.\n\n");
				SIGNINOffsets* offsets = offsetmanager.GetSigninOffsets();
				if (offsets != NULL) {
					setmem(offsets->NoSignInNotice, 0x38600000);
				} else {
					printf("Failed to load signin offsets!\r\n");
				}
			}
		} else {  // any other modules

		}
	}
	return ret;
}

DWORD PatchApplyBinary(string filepath) {
	DWORD fileSize = FileSize(filepath.c_str());
	if (fileSize == -1) {
		InfoPrint("    ERROR: Invalid patch path\n");
		return FALSE;
	}
	if (fileSize % 4 != 0) {
		InfoPrint("    ERROR: Invalid patch size\n");
		return FALSE;
	}
	BYTE* patchData = new BYTE[fileSize];
	if (!ReadFile(filepath.c_str(), patchData, fileSize)) {
		InfoPrint("    ERROR: Unable to read patch file\n");
		return FALSE;
	}

	DWORD offset = 0;
	DWORD patchesApplied = 0;
	if(*(DWORD*)&patchData[offset] == 0x52474C50)  // RGLP
		offset += 4;
	DWORD dest = *(DWORD*)&patchData[offset];
	offset += 4;

	while(dest != 0xFFFFFFFF && offset < fileSize){
		DWORD numPatches = *(DWORD*)&patchData[offset];
		offset += 4;
		for(DWORD i = 0; i < numPatches; i++, offset += 4, dest += 4) {
			//printf("     %08X  -> 0x%08X\n", dest, *(DWORD*)&buffer[offset]);
			setmem(dest, *(DWORD*)&patchData[offset]);
		}
		dest = *(DWORD*)&patchData[offset];
		offset += 4;
		patchesApplied++;
	}
	return patchesApplied;
}


void PatchSearchBinary(void) {
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	InfoPrint(" * Searching for additional RGLP binary patch files\n");

	// HDD
	hFind = FindFirstFile("HDD:\\*.rglp", &FindFileData);
	while (hFind != INVALID_HANDLE_VALUE) {
		InfoPrint("  **located binary: %s\n", FindFileData.cFileName);

		if (PatchApplyBinary("HDD:\\" + (string)FindFileData.cFileName) <= 0)
			InfoPrint("  ERROR: Cannot apply patch\n");

		if (!FindNextFile(hFind, &FindFileData))
			hFind = INVALID_HANDLE_VALUE;
	}

	// USB
	hFind = FindFirstFile("Mass0:\\*.rglp", &FindFileData);
	while (hFind != INVALID_HANDLE_VALUE) {
		InfoPrint("  **located binary: %s\n", FindFileData.cFileName);

		if (PatchApplyBinary("Mass0:\\" + (string)FindFileData.cFileName) <= 0)
			InfoPrint("  ERROR: Cannot apply patch\n");

		if (!FindNextFile(hFind, &FindFileData))
			hFind = INVALID_HANDLE_VALUE;
	}
}

VOID LoadPlugins() {
	string temp = reader->GetString("Plugins", "Plugin1", "none");
	if(temp != "none" && FileExists(temp.c_str())) {
		if (XexLoadImage(temp.c_str(), 8, 0, NULL))
			InfoPrint(" ERROR: Failed to load %s", temp.c_str());
	}
	temp = reader->GetString("Plugins", "Plugin2", "none");
	if (temp != "none" && FileExists(temp.c_str())) {
		if (XexLoadImage(temp.c_str(), 8, 0, NULL))
			InfoPrint(" ERROR: Failed to load %s", temp.c_str());
	}
	temp = reader->GetString("Plugins", "Plugin3", "none");
	if (temp != "none" && FileExists(temp.c_str())) {
		if (XexLoadImage(temp.c_str(), 8, 0, NULL))
			InfoPrint(" ERROR: Failed to load %s", temp.c_str());
	}
	temp = reader->GetString("Plugins", "Plugin4", "none");
	if (temp != "none" && FileExists(temp.c_str())) {
		if (XexLoadImage(temp.c_str(), 8, 0, NULL))
			InfoPrint(" ERROR: Failed to load %s", temp.c_str());
	}
	temp = reader->GetString("Plugins", "Plugin5", "none");
	if (temp != "none" && FileExists(temp.c_str())) {
		if(XexLoadImage(temp.c_str(), 8, 0, NULL))
			InfoPrint(" ERROR: Failed to load %s", temp.c_str());
	}
}

BOOL Initialize(HANDLE hModule) {
	InfoPrint("===RGLoader Runtime Patcher - Version 02===\n");

	Mount("\\Device\\Harddisk0\\Partition1", "\\System??\\Hdd:");
	Mount("\\Device\\Harddisk0\\Partition1", "\\System??\\HDD:");

	Mount("\\Device\\Mass0", "\\System??\\Mass0:");
	Mount("\\Device\\Mass1", "\\System??\\Mass1:");
	Mount("\\Device\\Mass2", "\\System??\\Mass2:");

	Mount("\\SystemRoot", "\\System??\\Root:");

	// install the expansion
	fExpansionEnabled = ExpansionStuff();
	
	// check for ini
	reader = new INIReader("Mass0:\\rgloader.ini");
	if(reader->ParseError() < 0)
		reader = new INIReader("Mass1:\\rgloader.ini");
	if(reader->ParseError() < 0)
		reader = new INIReader("Mass2:\\rgloader.ini");
	if(reader->ParseError() < 0)
		reader = new INIReader("Hdd:\\rgloader.ini");

	if(reader->ParseError() < 0) {
		InfoPrint("ERROR: Unable to open ini file!\r\n");
		PatchMapUSB();
		fKeepMemory = false;
		return 0;
	}

	/*
	char* SysExtPath = "HDD:\\Filesystems\\17489-dev\\$SystemUpdate";
	if (FileExists(SysExtPath)) {
		printf(" * Attemping to install system extended partion from %s\n", SysExtPath);

		if (setupSysPartitions(SysExtPath) == ERROR_SEVERITY_SUCCESS)
			printf("  -Success!\n");
		else
			printf("  -Failed\n");

		printf(" * Fixing XAM FEATURES\n");
#define XamFeatureEnableDisable 0x817483A8
		((void(*)(...))XamFeatureEnableDisable)(1, 2);
		((void(*)(...))XamFeatureEnableDisable)(1, 3);
		((void(*)(...))XamFeatureEnableDisable)(1, 4);

		((void(*)(...))XamFeatureEnableDisable)(0, 1);
		((void(*)(...))XamFeatureEnableDisable)(0, 5);
		((void(*)(...))XamFeatureEnableDisable)(0, 6);
		((void(*)(...))XamFeatureEnableDisable)(0, 7);
		((void(*)(...))XamFeatureEnableDisable)(0, 0x21);
		((void(*)(...))XamFeatureEnableDisable)(0, 0x22);
		((void(*)(...))XamFeatureEnableDisable)(0, 0x23);
		((void(*)(...))XamFeatureEnableDisable)(0, 0x24);
		((void(*)(...))XamFeatureEnableDisable)(0, 0x26);
		((void(*)(...))XamFeatureEnableDisable)(0, 0x27);
	}
	else {
		printf(" * No system extended files found, skipping..\n");
	}
	*/

	// booleans - config
	if (!reader->GetBoolean("Config", "NoRGLP", false))
		PatchSearchBinary();
	// booleans - expansion
	if (reader->GetBoolean("Expansion", "MapUSBMass", false))
		PatchMapUSB();
	if (reader->GetBoolean("Expansion", "PersistentPatches", false))
		PatchHookXexLoad();
	if (reader->GetBoolean("Expansion", "BootAnimation", false) && FileExists("Root:\\RGL_bootanim.xex"))
		PatchDefaultDash("\\SystemRoot\\RGL_bootanim.xex");
	if (reader->GetBoolean("Expansion", "RetailProfileEncryption", false))
		XamProfileCryptoHookSetup();
	// booleans - protections
	if (reader->GetBoolean("Protections", "BlockLiveDNS", false))
		PatchBlockLIVE();
	bool disableExpansionInstall = reader->GetBoolean("Protections", "DisableExpansionInstall", true);
	bool disableShadowboots = reader->GetBoolean("Protections", "DisableShadowboot", true);
	
	// strings
	string defaultDashboard = reader->GetString("Config", "DefaultDashboard", "none");
	if (defaultDashboard != "none" && FileExists(defaultDashboard.c_str()))
		PatchDefaultDash(defaultDashboard);

	InfoPrint("Patches successfully applied!\n");

	/* if(fExpansionEnabled)
	{
		bool ret = LoadKV("Mass0:\\rgkv.bin");
		if(!ret) ret = LoadKV("Mass1:\\rgkv.bin");
		if(!ret) ret = LoadKV("Mass2:\\rgkv.bin");
		if(!ret) ret = LoadKV("Hdd:\\rgkv.bin");
	} */

	if (fExpansionEnabled) {
		CPUStuff();

		if (disableExpansionInstall) {
			if (DisableExpansionInstalls() == TRUE)
				InfoPrint("HvxExpansionInstall unpatched successfully!\n");
		}

		if (disableShadowboots) {
			if (DisableShadowbooting() == TRUE)
				InfoPrint("HvxShadowboot disabled!\n");
		}
	}

	// skip plugin loading
	if (XamLoaderGetDvdTrayState() == DVD_TRAY_STATE_OPEN) {
		InfoPrint("Skipping RGLoader init...\n");
		return TRUE;
	}

	// load plugins after expansion shit
	InfoPrint("Loading plugins...\n");
	LoadPlugins();

	return TRUE;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved) {
	if(dwReason == DLL_PROCESS_ATTACH) {
		Initialize(hModule);

		Sleep(2000);

		//set load count to 1
		if(!fKeepMemory) {
			*(WORD*)((DWORD)hModule + 64) = 1;
			return FALSE;
		} else return true;
	}
	return TRUE;
}


