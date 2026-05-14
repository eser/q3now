// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

#ifndef CRYPTO_H
#define CRYPTO_H

// q_shared.h must be included by the translation unit before this header.

#define COM_SHA256_DIGEST_LEN 32
#define COM_SHA256_HEX_LEN ( COM_SHA256_DIGEST_LEN * 2 )

void Com_SHA256( const byte *data, unsigned int len, byte out[ COM_SHA256_DIGEST_LEN ] );
void Com_HMAC_SHA256( const byte *key, unsigned int keyLen, const byte *data, unsigned int dataLen, byte out[ COM_SHA256_DIGEST_LEN ] );
void Com_HMAC_SHA256_Hex( const char *key, const char *data, char outHex[ COM_SHA256_HEX_LEN + 1 ] );
void Com_RandomHexString( char *out, int hexLen );

#endif
