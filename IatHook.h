// This file contains code from
// https://github.com/stevemk14ebr/PolyHook_2_0/blob/master/sources/IatHook.cpp
// which is licensed under the MIT License.
// See PolyHook_2_0-LICENSE for more information.

#pragma once

#pragma push_macro( "uint16_t" )

#if __cplusplus >= 201103L || _MSC_VER >= 1700
# define IATHOOK_CONSTEXPR constexpr
#else
# define IATHOOK_CONSTEXPR const
# define uint16_t WORD
#endif

typedef struct _IATHOOK_IMAGE_IMPORT_BY_NAME {
    WORD    Hint;
    CHAR   Name[1];
} IATHOOK_IMAGE_IMPORT_BY_NAME, *IATHOOK_PIMAGE_IMPORT_BY_NAME;

typedef struct _IATHOOK_IMAGE_DELAYLOAD_DESCRIPTOR {
    union {
        DWORD AllAttributes;
        struct {
            DWORD RvaBased : 1;             // Delay load version 2
            DWORD ReservedAttributes : 31;
        } DUMMYSTRUCTNAME;
    } Attributes;

    DWORD DllNameRVA;                       // RVA to the name of the target library (NULL-terminate ASCII string)
    DWORD ModuleHandleRVA;                  // RVA to the HMODULE caching location (PHMODULE)
    DWORD ImportAddressTableRVA;            // RVA to the start of the IAT (PIMAGE_THUNK_DATA)
    DWORD ImportNameTableRVA;               // RVA to the start of the name table (PIMAGE_THUNK_DATA::AddressOfData)
    DWORD BoundImportAddressTableRVA;       // RVA to an optional bound IAT
    DWORD UnloadInformationTableRVA;        // RVA to an optional unload info table
    DWORD TimeDateStamp;                    // 0 if not bound,
                                            // Otherwise, date/time of the target DLL

} IATHOOK_IMAGE_DELAYLOAD_DESCRIPTOR, *IATHOOK_PIMAGE_DELAYLOAD_DESCRIPTOR;


template <typename T, typename T1, typename T2>
IATHOOK_CONSTEXPR T RVA2VA(T1 base, T2 rva)
{
	return reinterpret_cast<T>(reinterpret_cast<ULONG_PTR>(base) + rva);
}

template <typename T>
IATHOOK_CONSTEXPR T DataDirectoryFromModuleBase(void *moduleBase, size_t entryID)
{
	auto dosHdr = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
	auto ntHdr = RVA2VA<PIMAGE_NT_HEADERS>(moduleBase, dosHdr->e_lfanew);
	auto dataDir = ntHdr->OptionalHeader.DataDirectory;
	return RVA2VA<T>(moduleBase, dataDir[entryID].VirtualAddress);
}

PIMAGE_THUNK_DATA FindAddressByName(void *moduleBase, PIMAGE_THUNK_DATA impName, PIMAGE_THUNK_DATA impAddr, const char *funcName)
{
	for (; impName->u1.Ordinal; ++impName, ++impAddr)
	{
		if (IMAGE_SNAP_BY_ORDINAL(impName->u1.Ordinal))
			continue;

		auto import = RVA2VA<IATHOOK_PIMAGE_IMPORT_BY_NAME>(moduleBase, impName->u1.AddressOfData);
		if (strcmp(import->Name, funcName) != 0)
			continue;
		return impAddr;
	}
	return nullptr;
}

PIMAGE_THUNK_DATA FindAddressByOrdinal(void *moduleBase, PIMAGE_THUNK_DATA impName, PIMAGE_THUNK_DATA impAddr, uint16_t ordinal)
{
	for (; impName->u1.Ordinal; ++impName, ++impAddr)
	{
		if (IMAGE_SNAP_BY_ORDINAL(impName->u1.Ordinal) && IMAGE_ORDINAL(impName->u1.Ordinal) == ordinal)
			return impAddr;
	}
	return nullptr;
}

PIMAGE_THUNK_DATA FindIatThunkInModule(void *moduleBase, const char *dllName, const char *funcName)
{
	auto imports = DataDirectoryFromModuleBase<PIMAGE_IMPORT_DESCRIPTOR>(moduleBase, IMAGE_DIRECTORY_ENTRY_IMPORT);
	for (; imports->Name; ++imports)
	{
		if (_stricmp(RVA2VA<LPCSTR>(moduleBase, imports->Name), dllName) != 0)
			continue;

		auto origThunk = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->OriginalFirstThunk);
		auto thunk = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->FirstThunk);
		return FindAddressByName(moduleBase, origThunk, thunk, funcName);
	}
	return nullptr;
}

PIMAGE_THUNK_DATA FindDelayLoadThunkInModule(void *moduleBase, const char *dllName, const char *funcName)
{
	auto imports = DataDirectoryFromModuleBase<IATHOOK_PIMAGE_DELAYLOAD_DESCRIPTOR>(moduleBase, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
	for (; imports->DllNameRVA; ++imports)
	{
		if (_stricmp(RVA2VA<LPCSTR>(moduleBase, imports->DllNameRVA), dllName) != 0)
			continue;

		auto impName = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportNameTableRVA);
		auto impAddr = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportAddressTableRVA);
		return FindAddressByName(moduleBase, impName, impAddr, funcName);
	}
	return nullptr;
}

PIMAGE_THUNK_DATA FindDelayLoadThunkInModule(void *moduleBase, const char *dllName, uint16_t ordinal)
{
	auto imports = DataDirectoryFromModuleBase<IATHOOK_PIMAGE_DELAYLOAD_DESCRIPTOR>(moduleBase, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT);
	for (; imports->DllNameRVA; ++imports)
	{
		if (_stricmp(RVA2VA<LPCSTR>(moduleBase, imports->DllNameRVA), dllName) != 0)
			continue;

		auto impName = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportNameTableRVA);
		auto impAddr = RVA2VA<PIMAGE_THUNK_DATA>(moduleBase, imports->ImportAddressTableRVA);
		return FindAddressByOrdinal(moduleBase, impName, impAddr, ordinal);
	}
	return nullptr;
}

#pragma pop_macro( "uint16_t" )

