#ifndef __PE_SUPPL_H__
#define __PE_SUPPL_H__

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
} EFI_PE_IMAGE_SECTION_HEADER;

///
/// Size of EFI_IMAGE_SECTION_HEADER.
///
#define EFI_PE_IMAGE_SIZEOF_SECTION_HEADER       40
         
typedef struct {
	UINT64 ImageAddress;
	UINT64 ImageSize;
	UINT64 EntryPoint;
	UINTN SizeOfHeaders;
	UINT16 ImageType;
	UINT16 NumberOfSections;
	EFI_PE_IMAGE_SECTION_HEADER *FirstSection;
	EFI_IMAGE_DATA_DIRECTORY *RelocDir;
	EFI_IMAGE_DATA_DIRECTORY *SecDir;
	UINT64 NumberOfRvaAndSizes;
	EFI_IMAGE_OPTIONAL_HEADER_UNION *PEHdr;
} EFI_PE_COFF_LOADER_IMAGE_CONTEXT;

#endif
