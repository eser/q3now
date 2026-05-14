// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 1999-2005 Id Software, Inc.
// SPDX-FileCopyrightText: 2024-present Wired Engine contributors

typedef unsigned short crc_t;
unsigned short CRC_ProcessString(unsigned char *data, int length);
#if 0
void CRC_ProcessByte(unsigned short *crcvalue, byte data);
void CRC_ContinueProcessString(unsigned short *crc, char *data, int length);
#endif
