/*
 * Copyright 2012 <James.Bottomley@HansenPartnership.com>
 *
 * see COPYING file
 */

#include <efi.h>
#include <efilib.h>

#include <PeImage.h>		/* for ALIGN_VALUE */
#include <console.h>
#include <simple_file.h>
#include <efiauthenticated.h>
#include <execute.h>		/* for generate_path() */

static EFI_GUID IMAGE_PROTOCOL = LOADED_IMAGE_PROTOCOL;
static EFI_GUID SIMPLE_FS_PROTOCOL = SIMPLE_FILE_SYSTEM_PROTOCOL;
static EFI_GUID FILE_INFO = EFI_FILE_INFO_ID;
static EFI_GUID FS_INFO = EFI_FILE_SYSTEM_INFO_ID;

EFI_STATUS
simple_file_open_by_handle(EFI_HANDLE device, CHAR16 *name, EFI_FILE **file, UINT64 mode)
{
	EFI_STATUS efi_status;
	EFI_FILE_IO_INTERFACE *drive;
	EFI_FILE *root;

	efi_status = BS->HandleProtocol(device, &SIMPLE_FS_PROTOCOL, (VOID **)&drive);

	if (efi_status != EFI_SUCCESS) {
		Print(L"Unable to find simple file protocol (%d)\n", efi_status);
		goto error;
	}

	efi_status = drive->OpenVolume(drive, &root);

	if (efi_status != EFI_SUCCESS) {
		Print(L"Failed to open drive volume (%d)\n", efi_status);
		goto error;
	}

	efi_status = root->Open(root, file, name, mode, 0);

 error:
	return efi_status;
}

EFI_STATUS
simple_file_open(EFI_HANDLE image, CHAR16 *name, EFI_FILE **file, UINT64 mode)
{
	EFI_STATUS efi_status;
	EFI_HANDLE device;
	EFI_LOADED_IMAGE *li;
	EFI_DEVICE_PATH *loadpath = NULL;
	CHAR16 *PathName = NULL;

	efi_status = BS->HandleProtocol(image, &IMAGE_PROTOCOL, (VOID **)&li);

	if (efi_status != EFI_SUCCESS)
		return simple_file_open_by_handle(image, name, file, mode);

	efi_status = generate_path(name, li, &loadpath, &PathName);

	if (efi_status != EFI_SUCCESS) {
		Print(L"Unable to generate load path for %s\n", name);
		return efi_status;
	}

	device = li->DeviceHandle;

	efi_status = simple_file_open_by_handle(device, PathName, file, mode);

	FreePool(PathName);
	FreePool(loadpath);

	return efi_status;
}

EFI_STATUS
simple_dir_read_all_by_handle(EFI_HANDLE image, EFI_FILE *file, CHAR16* name, EFI_FILE_INFO **entries,
		    int *count)
{
	EFI_STATUS status;
	char buf[4096];
	UINTN size = sizeof(buf);
	EFI_FILE_INFO *fi = (void *)buf;
	
	status = file->GetInfo(file, &FILE_INFO, &size, fi);
	if (status != EFI_SUCCESS) {
		Print(L"Failed to get file info\n");
		goto out;
	}
	if ((fi->Attribute & EFI_FILE_DIRECTORY) == 0) {
		Print(L"Not a directory %s\n", name);
		status = EFI_INVALID_PARAMETER;
		goto out;
	}
	size = 0;
	*count = 0;
	for (;;) {
		UINTN len = sizeof(buf);
		status = file->Read(file, &len, buf);
		if (status != EFI_SUCCESS || len == 0)
			break;
		(*count)++;
		size += len;
	}
	file->SetPosition(file, 0);

	char *ptr = AllocatePool(size);
	*entries = (EFI_FILE_INFO *)ptr;
	if (!*entries)
		return EFI_OUT_OF_RESOURCES;
	int i;
	for (i = 0; i < *count; i++) {
		UINTN len = size;
		file->Read(file, &len, ptr);
		ptr += len;
		size -= len;
	}
	status = EFI_SUCCESS;
 out:
	simple_file_close(file);
	if (status != EFI_SUCCESS && *entries) {
		FreePool(*entries);
		*entries = NULL;
	}
	return status;
}

EFI_STATUS
simple_dir_read_all(EFI_HANDLE image, CHAR16 *name, EFI_FILE_INFO **entries,
		    int *count)
{
	EFI_FILE *file;
	EFI_STATUS status;

	status = simple_file_open(image, name, &file, EFI_FILE_MODE_READ);
	if (status != EFI_SUCCESS) {
		Print(L"failed to open file %s: %d\n", name, status);
		return status;
	}

	return simple_dir_read_all_by_handle(image, file, name, entries, count);
}

EFI_STATUS
simple_file_read_all(EFI_FILE *file, UINTN *size, void **buffer)
{
	EFI_STATUS efi_status;
	EFI_FILE_INFO *fi;
	char buf[1024];

	*size = sizeof(buf);
	fi = (void *)buf;
	

	efi_status = file->GetInfo(file, &FILE_INFO, size, fi);
	if (efi_status != EFI_SUCCESS) {
		Print(L"Failed to get file info\n");
		return efi_status;
	}

	*size = fi->FileSize;

	/* might use memory mapped, so align up to nearest page */
	*buffer = AllocateZeroPool(ALIGN_VALUE(*size, 4096));
	if (!*buffer) {
		Print(L"Failed to allocate buffer of size %d\n", *size);
		return EFI_OUT_OF_RESOURCES;
	}
	efi_status = file->Read(file, size, *buffer);

	return efi_status;
}


EFI_STATUS
simple_file_write_all(EFI_FILE *file, UINTN size, void *buffer)
{
	EFI_STATUS efi_status;

	efi_status = file->Write(file, &size, buffer);

	return efi_status;
}

void
simple_file_close(EFI_FILE *file)
{
	file->Close(file);
}

EFI_STATUS
simple_volume_selector(CHAR16 **title, CHAR16 **selected, EFI_HANDLE *h)
{
	UINTN count, i;
	EFI_HANDLE *vol_handles = NULL;
	EFI_STATUS status;
	CHAR16 **entries;
	int val;

	BS->LocateHandleBuffer(ByProtocol, &SIMPLE_FS_PROTOCOL, NULL,
			       &count, &vol_handles);

	if (!count || !vol_handles)
		return EFI_NOT_FOUND;

	entries = AllocatePool(sizeof(CHAR16 *) * (count+1));
	if (!entries)
		return EFI_OUT_OF_RESOURCES;

	for (i = 0; i < count; i++) {
		char buf[4096];
		UINTN size = sizeof(buf);
		EFI_FILE_SYSTEM_INFO *fi = (void *)buf;
		EFI_FILE *root;
		CHAR16 *name;
		EFI_FILE_IO_INTERFACE *drive;

		status = BS->HandleProtocol(vol_handles[i],
					    &SIMPLE_FS_PROTOCOL,
					    (VOID **)&drive);
		if (status != EFI_SUCCESS || !drive)
			continue;

		status = drive->OpenVolume(drive, &root);
		if (status != EFI_SUCCESS)
			continue;

		status = root->GetInfo(root, &FS_INFO, &size, fi);
		if (status != EFI_SUCCESS)
			continue;

		name = fi->VolumeLabel;
		
		if (!name || StrLen(name) == 0 || StrCmp(name, L" ") == 0) 
			name = DevicePathToStr(DevicePathFromHandle(vol_handles[i]));

		entries[i] = AllocatePool((StrLen(name) + 2) * sizeof(CHAR16));
		if (!entries[i])
			break;
		StrCpy(entries[i], name);
	}
	entries[i] = NULL;

	val = console_select(title, entries, 0);

	if (val >= 0) {
		*selected = AllocatePool((StrLen(entries[val]) + 1) * sizeof(CHAR16));
		if (*selected) {
			StrCpy(*selected , entries[val]);
		}
		*h = vol_handles[val];
	} else {
		*selected = NULL;
		*h = 0;
	}

	for (i = 0; i < count; i++) {
		if (entries[i])
			FreePool(entries[i]);
	}
	FreePool(entries);
	FreePool(vol_handles);

	
	return EFI_SUCCESS;
}

EFI_STATUS
simple_dir_filter(EFI_HANDLE image, CHAR16 *name, CHAR16 *filter,
		  CHAR16 ***result, int *count, EFI_FILE_INFO **entries)
{
	EFI_STATUS status;
	int tot, offs = StrLen(filter), i, c, filtercount = 1;
	EFI_FILE_INFO *next;
	void *ptr;
	CHAR16 *newfilter = AllocatePool((StrLen(filter) + 1) * sizeof(CHAR16)),
		**filterarr;

	if (!newfilter)
		return EFI_OUT_OF_RESOURCES;

	/* just in case efi ever stops writeable strings */
	StrCpy(newfilter, filter);

	for (i = 0; i < offs; i++) {
		if (filter[i] == '|')
			filtercount++;
	}
	filterarr = AllocatePool(filtercount * sizeof(void *));
	if (!filterarr)
		return EFI_OUT_OF_RESOURCES;
	c = 0;
	filterarr[c++] = newfilter;
	for (i = 0; i < offs; i++) {
		if (filter[i] == '|') {
			newfilter[i] = '\0';
			filterarr[c++] = &newfilter[i+1];
		}
	}

	*count = 0;

	status = simple_dir_read_all(image, name, entries, &tot);
		
	if (status != EFI_SUCCESS)
		goto out;
	ptr = next = *entries;

	for (i = 0; i < tot; i++) {
		int len = StrLen(next->FileName);

		for (c = 0; c < filtercount; c++) {
			offs = StrLen(filterarr[c]);

			if (StrCmp(&next->FileName[len - offs], filterarr[c]) == 0
			    || (next->Attribute & EFI_FILE_DIRECTORY)) {
				(*count)++;
				break;
			}
		}
		ptr += OFFSET_OF(EFI_FILE_INFO, FileName) + (len + 1)*sizeof(CHAR16);
		next = ptr;
	}
	if (*count)
		*result = AllocatePool(((*count) + 1) * sizeof(void *));
	else
		*result = AllocatePool(2 * sizeof(void *));

	*count = 0;
	ptr = next = *entries;

	for (i = 0; i < tot; i++) {
		int len = StrLen(next->FileName);

		if (StrCmp(next->FileName, L".") == 0)
			/* ignore . directory */
			goto next;

		if (next->Attribute & EFI_FILE_DIRECTORY) {
				(*result)[(*count)] = next->FileName;
				(*result)[(*count)][len] = '/';
				(*result)[(*count)++][len + 1] = '\0';
				goto next;
		}

		for (c = 0; c < filtercount; c++) {
			offs = StrLen(filterarr[c]);

			if (StrCmp(&next->FileName[len - offs], filterarr[c]) == 0) {
				(*result)[(*count)++] = next->FileName;
			} else {
				continue;
			}
			break;
		}

	next:		
		if (StrCmp(next->FileName, L"../") == 0) {
			/* place .. directory first */
			CHAR16 *tmp = (*result)[(*count) - 1];

			(*result)[(*count) - 1] = (*result)[0];
			(*result)[0] = tmp;
		}

		ptr += OFFSET_OF(EFI_FILE_INFO, FileName) + (len + 1)*sizeof(CHAR16);
		next = ptr;
	}
	if (*count == 0) {
		/* no entries at all ... can happen because top level dir has no . or .. */
		(*result)[(*count)++] = L"./";
	}
	(*result)[*count] = NULL;
	status = EFI_SUCCESS;

 out:
	if (status != EFI_SUCCESS) {
		if (*entries)
			FreePool(*entries);
		*entries = NULL;
		if (*result)
			FreePool(*result);
		*result = NULL;
	}
	return status;
}

void
simple_file_selector(EFI_HANDLE *im, CHAR16 **title, CHAR16 *name,
		     CHAR16 *filter, CHAR16 **result)
{
	EFI_STATUS status;
	CHAR16 **entries;
	EFI_FILE_INFO *dmp;
	int count, select, len;
	CHAR16 *newname, *selected;

	*result = NULL;
	if (!name)
		name = L"\\";
	if (!filter)
		filter = L"";
	if (!*im) {
		EFI_HANDLE h;
		CHAR16 *volname;

		simple_volume_selector(title, &volname, &h);
		if (!volname)
			return;
		FreePool(volname);
		*im = h;
	}

	newname = AllocatePool((StrLen(name) + 1)*sizeof(CHAR16));
	if (!newname)
		return;

	StrCpy(newname, name);
	name = newname;

 redo:
	status = simple_dir_filter(*im, name, filter, &entries, &count, &dmp);

	if (status != EFI_SUCCESS)
		goto out_free_name;

	select = console_select(title, entries, 0);
	if (select < 0)
		/* ESC key */
		goto out_free;
	selected = entries[select];
	FreePool(entries);
	entries = NULL;
	/* note that memory used by selected is valid until dmp is freed */
	len = StrLen(selected);
	if (selected[len - 1] == '/') {
		CHAR16 *newname;

		/* stay where we are */
		if (StrCmp(selected, L"./") == 0) {
			FreePool(dmp);
			goto redo;
		} else if (StrCmp(selected, L"../") == 0) {
			int i;

			i = StrLen(name) - 1;


			for (i = StrLen(name); i > 0; --i) {
				if (name[i] == '\\')
					break;
			}
			if (i == 0)
				i = 1;

			if (StrCmp(name, L"\\") != 0
			    && StrCmp(&name[i], L"..") != 0) {
				name[i] = '\0';
				FreePool(dmp);
				goto redo;
			}
		}
		newname = AllocatePool((StrLen(name) + len + 2)*sizeof(CHAR16));
		if (!newname)
			goto out_free;
		StrCpy(newname, name);
			
		if (name[StrLen(name) - 1] != '\\')
			StrCat(newname, L"\\");
		StrCat(newname, selected);
		/* remove trailing / */
		newname[StrLen(newname) - 1] = '\0';

		FreePool(dmp);
		FreePool(name);
		name = newname;

		goto redo;
	}
	*result = AllocatePool((StrLen(name) + len + 2)*sizeof(CHAR16));
	if (*result) {
		StrCpy(*result, name);
		if (name[StrLen(name) - 1] != '\\')
			StrCat(*result, L"\\");
		StrCat(*result, selected);
	}

 out_free:
	FreePool(dmp);
	if (entries)
		FreePool(entries);
 out_free_name:
	FreePool(name);
}
