/*
 * Copyright 2012-2013 Red Hat, Inc.
 * All rights reserved.
 *
 * See "COPYING" for license terms.
 *
 * Author(s): Peter Jones <pjones@redhat.com>
 */

#include <Uefi.h>
#include <Guid/GlobalVariable.h>
#include <Library/BaseLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/GenericBdsLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDevicePathLib/UefiDevicePathLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>

#include "ucs2.h"

EFI_LOADED_IMAGE *this_image = NULL;

static EFI_STATUS
get_file_size(EFI_FILE_HANDLE fh, UINTN *retsize)
{
	EFI_STATUS rc;
	void *buffer = NULL;
	UINTN bs = 0;
	EFI_GUID finfo = EFI_FILE_INFO_ID;

	/* The API here is "Call it once with bs=0, it fills in bs,
	 * then allocate a buffer and ask again to get it filled. */
	rc = fh->GetInfo(fh, &finfo, &bs, NULL);
	if (rc == EFI_BUFFER_TOO_SMALL) {
		buffer = AllocateZeroPool(bs);
		if (!buffer) {
			Print(L"Could not allocate memory\n");
			return EFI_OUT_OF_RESOURCES;
		}
		rc = fh->GetInfo(fh, &finfo,
					&bs, buffer);
	}
	/* This checks *either* the error from the first GetInfo, if it isn't
	 * the EFI_BUFFER_TOO_SMALL we're expecting, or the second GetInfo call
	 * in *any* case. */
	if (EFI_ERROR(rc)) {
		Print(L"Could not get file info: %d\n", rc);
		if (buffer)
			FreePool(buffer);
		return rc;
	}
	EFI_FILE_INFO *fi = buffer;
	*retsize = (UINTN)fi->FileSize;
	FreePool(buffer);
	return EFI_SUCCESS;
}

EFI_STATUS
read_file(EFI_FILE_HANDLE fh, CHAR16 *fullpath, CHAR16 **buffer, UINT64 *bs)
{
	EFI_FILE_HANDLE fh2;
	EFI_STATUS rc = fh->Open(fh, &fh2, fullpath,
				EFI_FILE_READ_ONLY, 0);
	if (EFI_ERROR(rc)) {
		Print(L"Couldn't open \"%s\": %d\n", fullpath, rc);
		return rc;
	}

	UINTN len = 0;
	CHAR16 *b = NULL;
	rc = get_file_size(fh2, &len);
	if (EFI_ERROR(rc)) {
		fh2->Close(fh2);
		return rc;
	}

	b = AllocateZeroPool(len + 2);
	if (!buffer) {
		Print(L"Could not allocate memory\n");
		fh2->Close(fh2);
		return EFI_OUT_OF_RESOURCES;
	}

	rc = fh->Read(fh, &len, b);
	if (EFI_ERROR(rc)) {
		FreePool(buffer);
		fh2->Close(fh2);
		Print(L"Could not read file: %d\n", rc);
		return rc;
	}
	*buffer = b;
	*bs = len;
	fh2->Close(fh2);
	return EFI_SUCCESS;
}

EFI_STATUS
make_full_path(CHAR16 *dirname, CHAR16 *filename, CHAR16 **out, UINT64 *outlen)
{
	UINT64 len;
	
	len = StrLen(dirname) + StrLen(filename) + StrLen(L"\\EFI\\\\") + 2;

	CHAR16 *fullpath = AllocateZeroPool((UINTN)(len*sizeof(CHAR16)));
	if (!fullpath) {
		Print(L"Could not allocate memory\n");
		return EFI_OUT_OF_RESOURCES;
	}

	StrCat(fullpath, L"\\EFI\\");
	StrCat(fullpath, dirname);
	StrCat(fullpath, L"\\");
	StrCat(fullpath, filename);

	*out = fullpath;
	*outlen = len;
	return EFI_SUCCESS;
}

CHAR16 *bootorder = NULL;
int nbootorder = 0;

EFI_DEVICE_PATH *first_new_option = NULL;
VOID *first_new_option_args = NULL;
UINTN first_new_option_size = 0;

EFI_STATUS
add_boot_option(EFI_DEVICE_PATH *dp, CHAR16 *filename, CHAR16 *label, CHAR16 *arguments)
{
	static int i = 0;
	CHAR16 varname[] = L"Boot0000";
	CHAR16 hexmap[] = L"0123456789ABCDEF";
	EFI_GUID global = EFI_GLOBAL_VARIABLE;
	UINTN size = 0;
	EFI_STATUS rc;

	for(; i <= 0xffff; i++) {
		varname[4] = hexmap[(i & 0xf000) >> 12];
		varname[5] = hexmap[(i & 0x0f00) >> 8];
		varname[6] = hexmap[(i & 0x00f0) >> 4];
		varname[7] = hexmap[(i & 0x000f) >> 0];

		void *var = BdsLibGetVariableAndSize(varname, &global, &size);
		if (!var) {
			size = (UINTN)(sizeof(UINT32) + sizeof (UINT16) +
				StrLen(label)*2 + 2 + UefiDevicePathLibGetDevicePathSize(dp) +
				StrLen(arguments) * 2 + 2);

			CHAR8 *data = AllocateZeroPool(size);
			CHAR8 *cursor = data;
			*(UINT32 *)cursor = LOAD_OPTION_ACTIVE;
			cursor += sizeof (UINT32);
			*(UINT16 *)cursor = (UINT16)(UefiDevicePathLibGetDevicePathSize(dp) & 0xFFFF);
			cursor += sizeof (UINT16);
			StrCpy((CHAR16 *)cursor, label);
			cursor += StrLen(label)*2 + 2;
			CopyMem(cursor, dp, UefiDevicePathLibGetDevicePathSize(dp));
			cursor += UefiDevicePathLibGetDevicePathSize(dp);
			StrCpy((CHAR16 *)cursor, arguments);

			Print(L"Creating boot entry \"%s\" with label \"%s\" "
					L"for file \"%s\"\n",
				varname, label, filename);
			rc = gRT->SetVariable(varname,
				&global, EFI_VARIABLE_NON_VOLATILE |
					 EFI_VARIABLE_BOOTSERVICE_ACCESS |
					 EFI_VARIABLE_RUNTIME_ACCESS,
				size, data);

			FreePool(data);

			if (EFI_ERROR(rc)) {
				Print(L"Could not create variable: %d\n", rc);
				return rc;
			}

			CHAR16 *newbootorder = AllocateZeroPool(sizeof (CHAR16)
							* (nbootorder + 1));
			if (!newbootorder)
				return EFI_OUT_OF_RESOURCES;

			int j = 0;
			if (nbootorder) {
				for (j = 0; j < nbootorder; j++)
					newbootorder[j] = bootorder[j];
				FreePool(bootorder);
			}
			newbootorder[j] = i & 0xffff;
			bootorder = newbootorder;
			nbootorder += 1;
#ifdef DEBUG_FALLBACK
			Print(L"nbootorder: %d\nBootOrder: ", nbootorder);
			for (j = 0 ; j < nbootorder ; j++)
				Print(L"%04x ", bootorder[j]);
			Print(L"\n");
#endif

			return EFI_SUCCESS;
		}
	}
	return EFI_OUT_OF_RESOURCES;
}

EFI_STATUS
update_boot_order(void)
{
	CHAR16 *oldbootorder;
	UINTN size;
	EFI_GUID global = EFI_GLOBAL_VARIABLE;
	CHAR16 *newbootorder = NULL;

	oldbootorder = BdsLibGetVariableAndSize(L"BootOrder", &global, &size);
	if (oldbootorder) {
		int n = (int)(size / sizeof (CHAR16) + nbootorder);

		newbootorder = AllocateZeroPool(n * sizeof (CHAR16));
		if (!newbootorder)
			return EFI_OUT_OF_RESOURCES;
		CopyMem(newbootorder, bootorder, nbootorder * sizeof (CHAR16));
		CopyMem(newbootorder + nbootorder, oldbootorder, size);
		size = n * sizeof (CHAR16);
	} else {
		size = nbootorder * sizeof(CHAR16);
		newbootorder = AllocateZeroPool(size);
		if (!newbootorder)
			return EFI_OUT_OF_RESOURCES;
		CopyMem(newbootorder, bootorder, size);
	}

#ifdef DEBUG_FALLBACK
	Print(L"nbootorder: %d\nBootOrder: ", size / sizeof (CHAR16));
	int j;
	for (j = 0 ; j < size / sizeof (CHAR16); j++)
		Print(L"%04x ", newbootorder[j]);
	Print(L"\n");
#endif

	if (oldbootorder) {
        gRT->SetVariable(L"BootOrder", &global,  EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_NON_VOLATILE, 0, NULL);
		FreePool(oldbootorder);
	}

	EFI_STATUS rc;
	rc = gRT->SetVariable(L"BootOrder", &global,
					EFI_VARIABLE_NON_VOLATILE |
					 EFI_VARIABLE_BOOTSERVICE_ACCESS |
					 EFI_VARIABLE_RUNTIME_ACCESS,
					size, newbootorder);
	FreePool(newbootorder);
	return rc;
}

EFI_STATUS
add_to_boot_list(EFI_FILE_HANDLE fh, CHAR16 *dirname, CHAR16 *filename, CHAR16 *label, CHAR16 *arguments)
{
	CHAR16 *fullpath = NULL;
	UINT64 pathlen = 0;
	EFI_STATUS rc = EFI_SUCCESS;

	rc = make_full_path(dirname, filename, &fullpath, &pathlen);
	if (EFI_ERROR(rc))
		return rc;
	
	EFI_DEVICE_PATH *dph = NULL, *dpf = NULL, *dp = NULL;
	
	dph = DevicePathFromHandle(this_image->DeviceHandle);
	if (!dph) {
		rc = EFI_OUT_OF_RESOURCES;
		goto err;
	}

	dpf = FileDevicePath(fh, fullpath);
	if (!dpf) {
		rc = EFI_OUT_OF_RESOURCES;
		goto err;
	}

	dp = AppendDevicePath(dph, dpf);
	if (!dp) {
		rc = EFI_OUT_OF_RESOURCES;
		goto err;
	}

#ifdef DEBUG_FALLBACK
	UINTN s = UefiDevicePathLibGetDevicePathSize(dp);
	int i;
	UINT8 *dpv = (void *)dp;
	for (i = 0; i < s; i++) {
		if (i > 0 && i % 16 == 0)
			Print(L"\n");
		Print(L"%02x ", dpv[i]);
	}
	Print(L"\n");

	CHAR16 *dps = DevicePathToStr(dp);
	Print(L"device path: \"%s\"\n", dps);
#endif
	if (!first_new_option) {
		CHAR16 *dps = DevicePathToStr(dp);
		Print(L"device path: \"%s\"\n", dps);
		first_new_option = DuplicateDevicePath(dp);
		first_new_option_args = arguments;
		first_new_option_size = StrLen(arguments) * sizeof (CHAR16);
	}

	add_boot_option(dp, fullpath, label, arguments);

err:
	if (dpf)
		FreePool(dpf);
	if (dp)
		FreePool(dp);
	if (fullpath)
		FreePool(fullpath);
	return rc;
}

EFI_STATUS
populate_stanza(EFI_FILE_HANDLE fh, CHAR16 *dirname, CHAR16 *filename, CHAR16 *csv)
{
#ifdef DEBUG_FALLBACK
	Print(L"CSV data: \"%s\"\n", csv);
#endif
	CHAR16 *file = csv;

	UINTN comma0 = StrCSpn(csv, L",");
	if (comma0 == 0)
		return EFI_INVALID_PARAMETER;
	file[comma0] = L'\0';
#ifdef DEBUG_FALLBACK
	Print(L"filename: \"%s\"\n", file);
#endif

	CHAR16 *label = csv + comma0 + 1;
	UINTN comma1 = StrCSpn(label, L",");
	if (comma1 == 0)
		return EFI_INVALID_PARAMETER;
	label[comma1] = L'\0';
#ifdef DEBUG_FALLBACK
	Print(L"label: \"%s\"\n", label);
#endif

	CHAR16 *arguments = csv + comma0 +1 + comma1 +1;
	UINTN comma2 = StrCSpn(arguments, L",");
	arguments[comma2] = L'\0';
	/* This one is optional, so don't check if comma2 is 0 */
#ifdef DEBUG_FALLBACK
	Print(L"arguments: \"%s\"\n", arguments);
#endif

	add_to_boot_list(fh, dirname, file, label, arguments);

	return EFI_SUCCESS;
}

EFI_STATUS
try_boot_csv(EFI_FILE_HANDLE fh, CHAR16 *dirname, CHAR16 *filename)
{
	CHAR16 *fullpath = NULL;
	UINT64 pathlen = 0;
	EFI_STATUS rc;

	rc = make_full_path(dirname, filename, &fullpath, &pathlen);
	if (EFI_ERROR(rc))
		return rc;

#ifdef DEBUG_FALLBACK
	Print(L"Found file \"%s\"\n", fullpath);
#endif

	CHAR16 *buffer;
	UINT64 bs;
	rc = read_file(fh, fullpath, &buffer, &bs);
	if (EFI_ERROR(rc)) {
		Print(L"Could not read file \"%s\": %d\n", fullpath, rc);
		FreePool(fullpath);
		return rc;
	}
	FreePool(fullpath);

#ifdef DEBUG_FALLBACK
	Print(L"File looks like:\n%s\n", buffer);
#endif

	CHAR16 *start = buffer;
	/* The file may or may not start with the Unicode byte order marker.
	 * Sadness ensues.  Since UEFI is defined as LE, I'm going to decree
	 * that these files must also be LE.
	 *
	 * IT IS THUS SO.
	 *
	 * But if we find the LE byte order marker, just skip it.
	 */
	if (*start == 0xfeff)
		start++;
	while (*start) {
		while (*start == L'\r' || *start == L'\n')
			start++;
		UINTN l = StrCSpn(start, L"\r\n");
		if (l == 0) {
			if (start[l] == L'\0')
				break;
			start++;
			continue;
		}
		CHAR16 c = start[l];
		start[l] = L'\0';

		populate_stanza(fh, dirname, filename, start);

		start[l] = c;
		start += l;
	}

	FreePool(buffer);
	return EFI_SUCCESS;
}

EFI_STATUS
find_boot_csv(EFI_FILE_HANDLE fh, CHAR16 *dirname)
{
	EFI_STATUS rc;
	void *buffer = NULL;
	UINTN bs = 0;
	EFI_GUID finfo = EFI_FILE_INFO_ID;

	/* The API here is "Call it once with bs=0, it fills in bs,
	 * then allocate a buffer and ask again to get it filled. */
	rc = fh->GetInfo(fh, &finfo, &bs, NULL);
	if (rc == EFI_BUFFER_TOO_SMALL) {
		buffer = AllocateZeroPool(bs);
		if (!buffer) {
			Print(L"Could not allocate memory\n");
			return EFI_OUT_OF_RESOURCES;
		}
		rc = fh->GetInfo(fh, &finfo,
					&bs, buffer);
	}
	/* This checks *either* the error from the first GetInfo, if it isn't
	 * the EFI_BUFFER_TOO_SMALL we're expecting, or the second GetInfo call
	 * in *any* case. */
	if (EFI_ERROR(rc)) {
		Print(L"Could not get info for \"%s\": %d\n", dirname, rc);
		if (buffer)
			FreePool(buffer);
		return rc;
	}

	EFI_FILE_INFO *fi = buffer;
	if (!(fi->Attribute & EFI_FILE_DIRECTORY)) {
		FreePool(buffer);
		return EFI_SUCCESS;
	}
	FreePool(buffer);

	bs = 0;
	do {
		bs = 0;
		rc = fh->Read(fh, &bs, NULL);
		if (rc == EFI_BUFFER_TOO_SMALL) {
			buffer = AllocateZeroPool(bs);
			if (!buffer) {
				Print(L"Could not allocate memory\n");
				return EFI_OUT_OF_RESOURCES;
			}

			rc = fh->Read(fh, &bs, buffer);
		}
		if (EFI_ERROR(rc)) {
			Print(L"Could not read \\EFI\\%s\\: %d\n", dirname, rc);
			FreePool(buffer);
			return rc;
		}
		if (bs == 0)
			break;

		fi = buffer;

		if (!StrCaseCmp(fi->FileName, L"boot.csv")) {
			EFI_FILE_HANDLE fh2;
			rc = fh->Open(fh, &fh2,
						fi->FileName,
						EFI_FILE_READ_ONLY, 0);
			if (EFI_ERROR(rc) || fh2 == NULL) {
				Print(L"Couldn't open \\EFI\\%s\\%s: %d\n",
					dirname, fi->FileName, rc);
				FreePool(buffer);
				buffer = NULL;
				continue;
			}
			rc = try_boot_csv(fh2, dirname, fi->FileName);
			fh2->Close(fh2);
		}

		FreePool(buffer);
		buffer = NULL;
	} while (bs != 0);

	rc = EFI_SUCCESS;

	return rc;
}

EFI_STATUS
find_boot_options(EFI_HANDLE device)
{
	EFI_STATUS rc = EFI_SUCCESS;

	EFI_FILE_IO_INTERFACE *fio = NULL;
	rc = gBS->HandleProtocol(device,
				&gEfiSimpleFileSystemProtocolGuid, (void **)&fio);
	if (EFI_ERROR(rc)) {
		Print(L"Couldn't find file system: %d\n", rc);
		return rc;
	}

	/* EFI_FILE_HANDLE is a pointer to an EFI_FILE, and I have
	 * *no idea* what frees the memory allocated here. Hopefully
	 * Close() does. */
	EFI_FILE_HANDLE fh = NULL;
	rc = fio->OpenVolume(fio, &fh);
	if (EFI_ERROR(rc) || fh == NULL) {
		Print(L"Couldn't open file system: %d\n", rc);
		return rc;
	}

	EFI_FILE_HANDLE fh2 = NULL;
	rc = fh->Open(fh, &fh2, L"EFI",
						EFI_FILE_READ_ONLY, 0);
	if (EFI_ERROR(rc) || fh2 == NULL) {
		Print(L"Couldn't open EFI: %d\n", rc);
		fh->Close(fh);
		return rc;
	}
	rc = fh2->SetPosition(fh2, 0);
	if (EFI_ERROR(rc)) {
		Print(L"Couldn't set file position: %d\n", rc);
		fh2->Close(fh2);
		fh->Close(fh);
		return rc;
	}

	void *buffer;
	UINTN bs;
	do {
		bs = 0;
		rc = fh2->Read(fh2, &bs, NULL);
		if (rc == EFI_BUFFER_TOO_SMALL ||
				(rc == EFI_SUCCESS && bs != 0)) {
			buffer = AllocateZeroPool(bs);
			if (!buffer) {
				Print(L"Could not allocate memory\n");
				/* sure, this might work, why not? */
				fh2->Close(fh2);
				fh->Close(fh);
				return EFI_OUT_OF_RESOURCES;
			}

			rc = fh2->Read(fh2, &bs, buffer);
		}
		if (bs == 0)
			break;

		if (EFI_ERROR(rc)) {
			Print(L"Could not read \\EFI\\: %d\n", rc);
			if (buffer) {
				FreePool(buffer);
				buffer = NULL;
			}
			fh2->Close(fh2);
			fh->Close(fh);
			return rc;
		}
		EFI_FILE_INFO *fi = buffer;

		if (!(fi->Attribute & EFI_FILE_DIRECTORY)) {
			FreePool(buffer);
			buffer = NULL;
			continue;
		}
		if (!StrCmp(fi->FileName, L".") ||
				!StrCmp(fi->FileName, L"..") ||
				!StrCaseCmp(fi->FileName, L"BOOT")) {
			FreePool(buffer);
			buffer = NULL;
			continue;
		}
#ifdef DEBUG_FALLBACK
		Print(L"Found directory named \"%s\"\n", fi->FileName);
#endif

		EFI_FILE_HANDLE fh3;
		rc = fh->Open(fh2, &fh3, fi->FileName,
						EFI_FILE_READ_ONLY, 0);
		if (EFI_ERROR(rc)) {
			Print(L"%d Couldn't open %s: %d\n", __LINE__, fi->FileName, rc);
			FreePool(buffer);
			buffer = NULL;
			continue;
		}

		rc = find_boot_csv(fh3, fi->FileName);
		FreePool(buffer);
		buffer = NULL;
		if (rc == EFI_OUT_OF_RESOURCES)
			break;

	} while (1);

	if (rc == EFI_SUCCESS && nbootorder > 0)
		rc = update_boot_order();

	fh2->Close(fh2);
	fh->Close(fh);
	return rc;
}

static EFI_STATUS
try_start_first_option(EFI_HANDLE parent_image_handle)
{
	EFI_STATUS rc;
	EFI_HANDLE image_handle;

	if (!first_new_option) {
		return EFI_SUCCESS;
	}

	rc = gBS->LoadImage(0, parent_image_handle,
			       first_new_option, NULL, 0,
			       &image_handle);
	if (EFI_ERROR(rc)) {
		Print(L"LoadImage failed: %d\n", rc);
		gBS->Stall(2000000);
		return rc;
	}

	EFI_LOADED_IMAGE *image;
	rc = gBS->HandleProtocol(image_handle, &gEfiLoadedImageProtocolGuid, (void *)&image);
	if (!EFI_ERROR(rc)) {
		image->LoadOptions = first_new_option_args;
		image->LoadOptionsSize = (UINT32)first_new_option_size;
	}

	rc = gBS->StartImage(image_handle, NULL, NULL);
	if (EFI_ERROR(rc)) {
		Print(L"StartImage failed: %d\n", rc);
		gBS->Stall(2000000);
	}
	return rc;
}

EFI_STATUS
EFIAPI
UefiMain(IN EFI_HANDLE image, IN EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS rc;

	/*InitializeLib(image, systab);*/

	rc = gBS->HandleProtocol(image, &gEfiLoadedImageProtocolGuid, (void *)&this_image);
	if (EFI_ERROR(rc)) {
		Print(L"Error: could not find loaded image: %d\n", rc);
		return rc;
	}

	Print(L"System BootOrder not found.  Initializing defaults.\n");

	rc = find_boot_options(this_image->DeviceHandle);
	if (EFI_ERROR(rc)) {
		Print(L"Error: could not find boot options: %d\n", rc);
		return rc;
	}

	try_start_first_option(image);

	Print(L"Reset System\n");
	gRT->ResetSystem(EfiResetCold,
			  EFI_SUCCESS, 0, NULL);

	return EFI_SUCCESS;
}
