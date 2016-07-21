/*
 * shim - trivial UEFI first-stage bootloader
 *
 * Copyright 2013 Red Hat, Inc <pjones@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Significant portions of this code are derived from Tianocore
 * (http://tianocore.sf.net) and are Copyright 2009-2012 Intel
 * Corporation.
 */

#ifndef SHIM_UCS2_H
#define SHIM_UCS2_H

#ifndef __attribute__
#define __attribute__(a)
#endif /* __attribute__ */

static inline INTN
__attribute__((unused))
StrCaseCmp(CHAR16 *s0, CHAR16 *s1)
{
	CHAR16 c0, c1;
	while (1) {
		if (*s0 == L'\0' || *s1 == L'\0')
			return *s1 - *s0;
		c0 = (*s0 >= L'a' && *s0 <= L'z') ? *s0 - 32 : *s0;
		c1 = (*s1 >= L'a' && *s1 <= L'z') ? *s1 - 32 : *s1;
		if (c0 != c1)
			return c1 - c0;
		s0++;
		s1++;
	}
	return 0;
}

static inline INTN
__attribute__((unused))
StrnCaseCmp(CHAR16 *s0, CHAR16 *s1, int n)
{
	CHAR16 c0, c1;
	int x = 0;
	while (n > x++) {
		if (*s0 == L'\0' || *s1 == L'\0')
			return *s1 - *s0;
		c0 = (*s0 >= L'a' && *s0 <= L'z') ? *s0 - 32 : *s0;
		c1 = (*s1 >= L'a' && *s1 <= L'z') ? *s1 - 32 : *s1;
		if (c0 != c1)
			return c1 - c0;
		s0++;
		s1++;
	}
	return 0;
}

static inline UINTN
__attribute__((unused))
StrCSpn(const CHAR16 *s, const CHAR16 *reject)
{
	UINTN ret;

	for (ret = 0; s[ret] != L'\0'; ret++) {
		int i;
		for (i = 0; reject[i] != L'\0'; i++) {
			if (reject[i] == s[ret])
				return ret;
		}
	}
	return ret;
}

#endif /* SHIM_UCS2_H */
