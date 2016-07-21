/** @file  
  Internal include file for BaseCryptLib.

Copyright (c) 2010 - 2015, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __INTERNAL_CRYPT_LIB_H__
#define __INTERNAL_CRYPT_LIB_H__

#ifdef _MSC_VER
#define _WINDOWS_ 1
#define _WINNT_ 1
#define _WINBASE_ 1
#define _WINDEF_ 1
#define _MINWINBASE_ 1
#define _MINWINDEF_ 1
#define _MINWINBASE_ 1
#define _PROCESSENV_ 1
#define _APISETDEBUG_ 1
#define _APISETHANDLE_ 1
#define _APISETFILE_ 1
#define _APISETUTIL_ 1
#define _ERRHANDLING_H_ 1
#define _FIBERS_H_ 1
#define _HEAPAPI_H_ 1
#define _IO_APISET_H_ 1
#define _MEMORYAPI_H_ 1
#define _NAMEDPIPE_H_ 1
#define _PROCESSTHREADSAPI_H_ 1
#define _PROFILEAPI_H_ 1
#define _SYNCHAPI_H_ 1
#define _SYSINFOAPI_H_ 1
#endif /* _MSC_VER */

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseCryptLib.h>

#include "OpenSslSupport.h"

#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define OBJ_get0_data(o) ((o)->data)
#define OBJ_length(o) ((o)->length)
#endif /* OPENSSL_VERSION_NUMBER < 0x10100000L */

#endif /* __INTERNAL_CRYPT_LIB_H__ */

