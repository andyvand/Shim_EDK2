#ifndef __PASSWORD_CRYPT_H__
#define __PASSWORD_CRYPT_H__

#ifndef __attribute__
#define __attribute__(a)
#endif

enum HashMethod {
	TRADITIONAL_DES = 0,
	EXTEND_BSDI_DES,
	MD5_BASED,
	SHA256_BASED,
	SHA512_BASED,
	BLOWFISH_BASED
};

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
typedef struct {
	UINT16 method;
	UINT64 iter_count;
	UINT16 salt_size;
	UINT8  salt[32];
	UINT8  hash[128];
} __attribute__ ((packed)) PASSWORD_CRYPT;
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#define PASSWORD_CRYPT_SIZE sizeof(PASSWORD_CRYPT)

EFI_STATUS password_crypt (const char *password, UINT32 pw_length,
			   const PASSWORD_CRYPT *pw_hash, UINT8 *hash);
UINT16 get_hash_size (const UINT16 method);

#endif /* __PASSWORD_CRYPT_H__ */
