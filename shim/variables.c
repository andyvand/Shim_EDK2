/*
 * Copyright 2012 <James.Bottomley@HansenPartnership.com>
 *
 * see COPYING file
 *
 * Portions of this file are a direct cut and paste from Tianocore
 * (http://tianocore.sf.net)
 *
 *  SecurityPkg/VariableAuthenticated/SecureBootConfigDxe/SecureBootConfigImpl.c
 *
 * Copyright (c) 2011 - 2012, Intel Corporation. All rights reserved.<BR>
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found 
 * at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 * 
 */
#include <Uefi.h>
#include <Uefi/UefiSpec.h>
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
#include <Library/GenericBdsLib.h>
#include <Library/UefiDevicePathLib/UefiDevicePathLib.h>
#include <Library/PeCoffLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/BaseCryptLib.h>
#include <openssl/x509.h>

/*#include <efiauthenticated.h>*/
/*#include <guid.h>*/

#include "variables.h"
#include "console.h"
/*#include <errors.h>*/

EFI_STATUS
variable_create_esl(void *cert, int cert_len, EFI_GUID *type, EFI_GUID *owner,
		    void **out, int *outlen)
{
	*outlen = cert_len + sizeof(EFI_SIGNATURE_LIST) + sizeof(EFI_GUID);

	*out = AllocateZeroPool(*outlen);
	if (!*out)
		return EFI_OUT_OF_RESOURCES;

	EFI_SIGNATURE_LIST *sl = *out;

	sl->SignatureHeaderSize = 0;
	sl->SignatureType = *type;
	sl->SignatureSize = cert_len + sizeof(EFI_GUID);
	sl->SignatureListSize = *outlen;

	EFI_SIGNATURE_DATA *sd = (EFI_SIGNATURE_DATA *)((char *)*out + sizeof(EFI_SIGNATURE_LIST));

	if (owner)
		sd->SignatureOwner = *owner;

	CopyMem(sd->SignatureData, cert, cert_len);

	return EFI_SUCCESS;
}


EFI_STATUS
CreateTimeBasedPayload (
  IN OUT UINTN            *DataSize,
  IN OUT UINT8            **Data
  )
{
  EFI_STATUS                       Status;
  UINT8                            *NewData;
  UINT8                            *Payload;
  UINTN                            PayloadSize;
  EFI_VARIABLE_AUTHENTICATION_2    *DescriptorData;
  UINTN                            DescriptorSize;
  EFI_TIME                         Time;
  EFI_GUID efi_cert_type = EFI_CERT_TYPE_PKCS7_GUID;
  
  if (Data == NULL || DataSize == NULL) {
    return EFI_INVALID_PARAMETER;
  }
  
  //
  // In Setup mode or Custom mode, the variable does not need to be signed but the 
  // parameters to the SetVariable() call still need to be prepared as authenticated
  // variable. So we create EFI_VARIABLE_AUTHENTICATED_2 descriptor without certificate
  // data in it.
  //
  Payload     = *Data;
  PayloadSize = *DataSize;
  
  DescriptorSize    = OFFSET_OF(EFI_VARIABLE_AUTHENTICATION_2, AuthInfo) + OFFSET_OF(WIN_CERTIFICATE_UEFI_GUID, CertData);
  NewData = (UINT8*) AllocateZeroPool (DescriptorSize + PayloadSize);
  if (NewData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if ((Payload != NULL) && (PayloadSize != 0)) {
    CopyMem (NewData + DescriptorSize, Payload, PayloadSize);
  }

  DescriptorData = (EFI_VARIABLE_AUTHENTICATION_2 *) (NewData);

  ZeroMem (&Time, sizeof (EFI_TIME));
  Status = gRT->GetTime(&Time, NULL);
  if (EFI_ERROR (Status)) {
    FreePool(NewData);
    return Status;
  }
  Time.Pad1       = 0;
  Time.Nanosecond = 0;
  Time.TimeZone   = 0;
  Time.Daylight   = 0;
  Time.Pad2       = 0;
  CopyMem (&DescriptorData->TimeStamp, &Time, sizeof (EFI_TIME));
 
  DescriptorData->AuthInfo.Hdr.dwLength         = OFFSET_OF (WIN_CERTIFICATE_UEFI_GUID, CertData);
  DescriptorData->AuthInfo.Hdr.wRevision        = 0x0200;
  DescriptorData->AuthInfo.Hdr.wCertificateType = WIN_CERT_TYPE_EFI_GUID;
  DescriptorData->AuthInfo.CertType =  efi_cert_type;
 
  /* we're expecting an EFI signature list, so don't free the input since
   * it might not be in a pool */
#if 0
  if (Payload != NULL) {
    FreePool(Payload);
  }
#endif
  
  *DataSize = DescriptorSize + PayloadSize;
  *Data     = NewData;
  return EFI_SUCCESS;
}

EFI_STATUS
SetSecureVariable(CHAR16 *var, UINT8 *Data, UINTN len, EFI_GUID owner,
		  UINT32 options, int createtimebased)
{
	EFI_SIGNATURE_LIST *Cert;
	UINTN DataSize;
	EFI_STATUS efi_status;
	EFI_GUID x509_guid = EFI_CERT_X509_GUID;

	/* Microsoft request: Bugs in some UEFI platforms mean that PK or any
	 * other secure variable can be updated or deleted programmatically,
	 * so prevent */
	if (!variable_is_setupmode())
		return EFI_SECURITY_VIOLATION;

	if (createtimebased) {
		int ds;
		efi_status = variable_create_esl(Data, (int)len, &x509_guid, NULL,
						 (void **)&Cert, &ds);
		if (efi_status != EFI_SUCCESS) {
			Print(L"Failed to create %s certificate %d\n", var, efi_status);
			return efi_status;
		}

		DataSize = ds;
	} else {
		/* we expect an efi signature list rather than creating it */
		Cert = (EFI_SIGNATURE_LIST *)Data;
		DataSize = len;
	}
	efi_status = CreateTimeBasedPayload(&DataSize, (UINT8 **)&Cert);
	if (efi_status != EFI_SUCCESS) {
		Print(L"Failed to create time based payload %d\n", efi_status);
		return efi_status;
	}

	efi_status = gRT->SetVariable(var, &owner,
				       EFI_VARIABLE_NON_VOLATILE
				       | EFI_VARIABLE_RUNTIME_ACCESS 
				       | EFI_VARIABLE_BOOTSERVICE_ACCESS
				       | EFI_VARIABLE_TIME_BASED_AUTHENTICATED_WRITE_ACCESS
				       | options,
				       DataSize, Cert);

	return efi_status;
}

UINT64
GetOSIndications(void)
{
	UINT64 indications;
	UINTN DataSize = sizeof(indications);
	EFI_STATUS efi_status;
    EFI_GUID GV = EFI_GLOBAL_VARIABLE;

	efi_status = gRT->GetVariable(L"OsIndicationsSupported", &GV, NULL, &DataSize, &indications);
	if (efi_status != EFI_SUCCESS)
		return 0;

	return indications;
}

EFI_STATUS
SETOSIndicationsAndReboot(UINT64 indications)
{
	UINTN DataSize = sizeof(indications);
	EFI_STATUS efi_status;
    EFI_GUID GV = EFI_GLOBAL_VARIABLE;

	efi_status = gRT->SetVariable(L"OsIndications",
				       &GV,
				       EFI_VARIABLE_NON_VOLATILE
				       | EFI_VARIABLE_RUNTIME_ACCESS 
				       | EFI_VARIABLE_BOOTSERVICE_ACCESS,
				       DataSize, &indications);

	if (efi_status != EFI_SUCCESS)
		return efi_status;

	gRT->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
	/* does not return */

	return EFI_SUCCESS;
}

EFI_STATUS
get_variable_attr(CHAR16 *var, UINT8 **data, UINTN *len, EFI_GUID owner,
		  UINT32 *attributes)
{
	EFI_STATUS efi_status;

	*len = 0;

	efi_status = gRT->GetVariable(var, &owner,
				       NULL, len, NULL);
	if (efi_status != EFI_BUFFER_TOO_SMALL)
		return efi_status;

	*data = AllocateZeroPool(*len);
	if (!data)
		return EFI_OUT_OF_RESOURCES;
	
	efi_status = gRT->GetVariable(var, &owner,
				       attributes, len, *data);

	if (efi_status != EFI_SUCCESS) {
		FreePool(*data);
		*data = NULL;
	}
	return efi_status;
}

EFI_STATUS
get_variable(CHAR16 *var, UINT8 **data, UINTN *len, EFI_GUID owner)
{
	return get_variable_attr(var, data, len, owner, NULL);
}

EFI_STATUS
find_in_esl(UINT8 *Data, UINTN DataSize, UINT8 *key, UINTN keylen)
{
	EFI_SIGNATURE_LIST *CertList;

	certlist_for_each_certentry(CertList, Data, DataSize, DataSize) {
		if (CertList->SignatureSize != keylen + sizeof(EFI_GUID))
			continue;
		EFI_SIGNATURE_DATA *Cert;

		certentry_for_each_cert(Cert, CertList)
			if (CompareMem (Cert->SignatureData, key, keylen) == 0)
				return EFI_SUCCESS;
	}
	return EFI_NOT_FOUND;
}

EFI_STATUS
find_in_variable_esl(CHAR16* var, EFI_GUID owner, UINT8 *key, UINTN keylen)
{
	UINTN DataSize;
	UINT8 *Data;
	EFI_STATUS status;

	status = get_variable(var, &Data, &DataSize, owner);
	if (status != EFI_SUCCESS)
		return status;

	status = find_in_esl(Data, DataSize, key, keylen);

	FreePool(Data);

	return status;
}

int
variable_is_setupmode(void)
{
	/* set to 1 because we return true if SetupMode doesn't exist */
	UINT8 SetupMode = 1;
	UINTN DataSize = sizeof(SetupMode);
    EFI_GUID GV = EFI_GLOBAL_VARIABLE;

	gRT->GetVariable(L"SetupMode", &GV, NULL,
			  &DataSize, &SetupMode);

	return SetupMode;
}

int
variable_is_secureboot(void)
{
	/* return false if variable doesn't exist */
	UINT8 SecureBoot = 0;
	UINTN DataSize;
    EFI_GUID GV = EFI_GLOBAL_VARIABLE;

	DataSize = sizeof(SecureBoot);
	gRT->GetVariable(L"SecureBoot", &GV, NULL,
			  &DataSize, &SecureBoot);

	return SecureBoot;
}

EFI_STATUS
variable_enroll_hash(CHAR16 *var, EFI_GUID owner,
		     UINT8 hash[SHA256_DIGEST_SIZE])
{
	EFI_STATUS status;
    EFI_GUID SHAGUID = EFI_CERT_SHA256_GUID;
    EFI_GUID MOKOWNER = MOK_OWNER_GUID;
    EFI_GUID SIGDB = EFI_IMAGE_SECURITY_DATABASE_GUID;

	if (find_in_variable_esl(var, owner, hash, SHA256_DIGEST_SIZE)
	    == EFI_SUCCESS)
		/* hash already present */
		return EFI_ALREADY_STARTED;

	UINT8 sig[sizeof(EFI_SIGNATURE_LIST) + sizeof(EFI_SIGNATURE_DATA) - 1 + SHA256_DIGEST_SIZE];
	EFI_SIGNATURE_LIST *l = (void *)sig;
	EFI_SIGNATURE_DATA *d = (EFI_SIGNATURE_DATA *)((char *)sig + sizeof(EFI_SIGNATURE_LIST));
	SetMem(sig, 0, sizeof(sig));
	l->SignatureType = SHAGUID;
	l->SignatureListSize = sizeof(sig);
	l->SignatureSize = 16 +32; /* UEFI defined */
	CopyMem(&d->SignatureData, hash, SHA256_DIGEST_SIZE);
	d->SignatureOwner = MOKOWNER;

	if (CompareGuid(&owner, &SIGDB) == 0)
		status = SetSecureVariable(var, sig, sizeof(sig), owner,
					   EFI_VARIABLE_APPEND_WRITE, 0);
	else
		status = gRT->SetVariable(var, &owner,
					   EFI_VARIABLE_NON_VOLATILE
					   | EFI_VARIABLE_BOOTSERVICE_ACCESS
					   | EFI_VARIABLE_APPEND_WRITE,
					   sizeof(sig), sig);
	return status;
}
