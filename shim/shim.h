#include <Library/PeCoffLib.h>
#include "PeSuppl.h"

extern EFI_GUID SHIM_LOCK_GUID;

struct _SHIM_LOCK;

typedef
EFI_STATUS
(*EFI_SHIM_LOCK_VERIFY) (
	IN VOID *buffer,
	IN UINT32 size
	);

typedef
EFI_STATUS
(*EFI_SHIM_LOCK_HASH) (
	IN char *data,
	IN int datasize,
	EFI_PE_COFF_LOADER_IMAGE_CONTEXT *context,
	UINT8 *sha256hash,
	UINT8 *sha1hash
	);

typedef
EFI_STATUS
(*EFI_SHIM_LOCK_CONTEXT) (
	IN VOID *data,
	IN unsigned int datasize,
	EFI_PE_COFF_LOADER_IMAGE_CONTEXT *context
	);

typedef struct _SHIM_LOCK {
	EFI_SHIM_LOCK_VERIFY Verify;
	EFI_SHIM_LOCK_HASH Hash;
	EFI_SHIM_LOCK_CONTEXT Context;
} SHIM_LOCK;

#define EFI_IMAGE_SIZEOF_SHORT_NAME 8

///
/// Section Table. This table immediately follows the optional header.
///
typedef struct {
    UINT8 Name[EFI_IMAGE_SIZEOF_SHORT_NAME];
    union {
        UINT32  PhysicalAddress;
        UINT32  VirtualSize;
    } Misc;
    UINT32  VirtualAddress;
    UINT32  SizeOfRawData;
    UINT32  PointerToRawData;
    UINT32  PointerToRelocations;
    UINT32  PointerToLinenumbers;
    UINT16  NumberOfRelocations;
    UINT16  NumberOfLinenumbers;
    UINT32  Characteristics;
} EFI_IMAGE_SECTION_TABLE_HEADER;

/* Loader definition */
#if defined(MDE_CPU_IA32)
#define DEFAULT_LOADER_CHAR "\\CLOVERIA32.efi"
#define DEFAULT_LOADER L"\\CLOVERIA32.efi"
#elif defined(MDE_CPU_X64)
#define DEFAULT_LOADER_CHAR "\\CLOVERX64.efi"
#define DEFAULT_LOADER L"\\CLOVERX64.efi"
#elif defined(MDE_CPU_ARM)
#define DEFAULT_LOADER_CHAR "\\CLOVERARM.efi"
#define DEFAULT_LOADER L"\\CLOVERARM.efi"
#elif defined(MDE_CPU_AARCH64)
#define DEFAULT_LOADER_CHAR "\\CLOVERAARCH64.efi"
#define DEFAULT_LOADER L"\\CLOVERAARCH64.efi"
#elif defined(MDE_CPU_EBC)
#define DEFAULT_LOADER_CHAR "\\CLOVEREBC.efi"
#define DEFAULT_LOADER L"\\CLOVEREBC.efi"
#endif

/*
#define DEFAULT_LOADER_CHAR "\\grub.efi"
#define DEFAULT_LOADER L"\\grub.efi"
 */
