/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// q_shared.c -- stateless support routines that are included in each code dll
#include "q_shared.h"

#ifdef COM_Parse
#undef COM_Parse
#endif
#ifdef COM_ParseExt
#undef COM_ParseExt
#endif

float Com_Clamp( float min, float max, float value ) {
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char *last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}


/*
============
COM_GetExtension
============
*/
const char *COM_GetExtension( const char *name )
{
	const char *dot = strrchr(name, '.'), *slash;
	if (dot && ((slash = strrchr(name, '/')) == NULL || slash < dot))
		return dot + 1;
	else
		return "";
}


/*
============
COM_StripExtension
============
*/
void COM_StripExtension( const char *in, char *out, int destsize )
{
	const char *dot = strrchr(in, '.'), *slash;
	if (dot && ((slash = strrchr(in, '/')) == NULL || slash < dot))
		destsize = (destsize < dot-in+1 ? destsize : dot-in+1);

	if ( in == out && destsize > 1 )
		out[destsize-1] = '\0';
	else
		Q_strncpyz(out, in, destsize);
}


/*
============
COM_CompareExtension

string compare the end of the strings and return qtrue if strings match
============
*/
qboolean COM_CompareExtension(const char *in, const char *ext)
{
	int inlen = strlen(in);
	int extlen = strlen(ext);
	
	if(extlen <= inlen)
	{
		in += inlen - extlen;
		
		if(!Q_stricmp(in, ext))
			return qtrue;
	}
	
	return qfalse;
}


/*
==================
COM_DefaultExtension

if path doesn't have an extension, then append
 the specified one (which should include the .)
==================
*/
void COM_DefaultExtension( char *path, int maxSize, const char *extension )
{
	const char *dot = strrchr(path, '.'), *slash;
	if (dot && ((slash = strrchr(path, '/')) == NULL || slash < dot))
		return;

	qstring_t p_qs = QS_WrapExisting( path, maxSize );
	QS_Append( &p_qs, extension );
}


/*
==================
COM_GenerateHashValue

used in renderer and filesystem
==================
*/
// ASCII lowcase conversion table with '\\' turned to '/' and '.' to '\0'
static const byte hash_locase[ 256 ] =
{
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x00,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
	0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x5b,0x2f,0x5d,0x5e,0x5f,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
	0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
	0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
	0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
	0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
	0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
	0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
	0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
	0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
	0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
	0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
	0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
	0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
	0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
	0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};

// hash_locase maps '.' to '\0' (path extension = hash terminator)
// and '\\' to '/' (backslash = forward slash for path normalization)
// Note: _Static_assert on array subscripts is a GCC extension; Clang rejects it
#if !defined(__clang__)
_Static_assert( hash_locase['.']  == '\0', "hash_locase: '.' must map to '\\0' (terminates hash at extension)" );
_Static_assert( hash_locase['\\'] == '/',  "hash_locase: '\\\\' must map to '/' (path separator normalization)" );
_Static_assert( hash_locase['A']  == 'a',  "hash_locase: uppercase must map to lowercase" );
_Static_assert( hash_locase['a']  == 'a',  "hash_locase: lowercase must map to itself" );
#endif

unsigned long Com_GenerateHashValue( const char *fname, const unsigned int size )
{
	const byte *s = (byte*)fname;
	unsigned long hash = 0;
	int		c;


	while ( (c = hash_locase[(byte)*s++]) != '\0' ) {
		hash = hash * 101 + c;
	}
	
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);

	return hash;
}


/*
============
Com_Split
============
*/
int Com_Split( char *in, char **out, int outsz, int delim )
{
	int c;
	char **o = out, **end = out + outsz;
	// skip leading spaces
	if ( delim >= ' ' ) {
		while( (c = *in) != '\0' && c <= ' ' )
			in++; 
	}
	*out = in; out++;
	while( out < end ) {
		while( (c = *in) != '\0' && c != delim )
			in++; 
		*in = '\0';
		if ( !c ) {
			// don't count last null value
			if ( out[-1][0] == '\0' )
				out--;
			break;
		}
		in++;
		// skip leading spaces
		if ( delim >= ' ' ) {
			while( (c = *in) != '\0' && c <= ' ' )
				in++; 
		}
		*out = in; out++;
	}
	// sanitize last value
	while( (c = *in) != '\0' && c != delim )
		in++; 
	*in = '\0';
	c = out - o;
	// set remaining out pointers
	while( out < end ) {
		*out = in; out++;
	}
	return c;
}


/*
==================
crc32_buffer
==================
*/
static const unsigned int crc32_table[256] = {
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
	0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	0x0EDB8832, 0x79DCB8A4, 0xE0D5E91B, 0x97D2D988,
	0x09B64C2B, 0x7EB17CBE, 0xE7B82D09, 0x90BF1D9F,
	0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
	0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
	0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
	0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA, 0x42B2986C, 0xDBBBB9D6, 0xACBCB9C0,
	0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
	0x21B4F6B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
	0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
	0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
	0x7F6A0DCA, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
	0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
	0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
	0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
	0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7822,
	0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
	0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
	0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
	0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5A08,
	0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
	0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
	0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
	0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
	0xDF60EFC3, 0xA8670955, 0x31AE17E3, 0x46A9D675,
	0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
	0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
	0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
	0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
	0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	0x86D3D2D4, 0xF1D4E242, 0x68DDB3F6, 0x1FDA8360,
	0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6B70, 0x66063BCA, 0x11010B5C,
	0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
	0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
	0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
	0xBAD03605, 0xCDD706FF, 0x54DE5729, 0x23D967BF,
	0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
	0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

unsigned int crc32_buffer( const byte *buf, unsigned int len ) {
	unsigned int crc = 0xFFFFFFFFu;

	while ( len-- ) {
		crc = crc32_table[( crc ^ *buf++ ) & 0xFF] ^ ( crc >> 8 );
	}

	return crc ^ 0xFFFFFFFFu;
}


/*
============================================================================

PARSING

============================================================================
*/

void COM_BeginParseSession( ComParser *parser, const char *name )
{
	parser->lines = 1;
	parser->tokenline = 0;
	Com_sprintf(parser->parsename, sizeof(parser->parsename), "%s", name);
}


int COM_GetCurrentParseLine( const ComParser *parser )
{
	if ( parser->tokenline )
	{
		return parser->tokenline;
	}

	return parser->lines;
}


const char *COM_Parse( ComParser *parser, const char **data_p )
{
	return COM_ParseExt( parser, data_p, qtrue );
}


void COM_ParseError( const ComParser *parser, const char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	vsnprintf (string, sizeof(string), format, argptr);
	va_end( argptr );

	Com_Printf( "ERROR: %s, line %d: %s\n", parser->parsename, COM_GetCurrentParseLine( parser ), string );
}


void COM_ParseWarning( const ComParser *parser, const char *format, ... )
{
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	vsnprintf (string, sizeof(string), format, argptr);
	va_end( argptr );

	Com_Printf( "WARNING: %s, line %d: %s\n", parser->parsename, COM_GetCurrentParseLine( parser ), string );
}


/*
==============
COM_Parse

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
static const char *SkipWhitespace( ComParser *parser, const char *data, qboolean *hasNewLines ) {
	int c;

	while( (c = *data) <= ' ') {
		if( !c ) {
			return NULL;
		}
		if( c == '\n' ) {
			parser->lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}


int COM_Compress( char *data_p ) {
	const char *in = data_p;
	char *out = data_p;
	int c;
	qboolean newline = qfalse, whitespace = qfalse;


	while ((c = *in) != '\0') {
		// skip double slash comments
		if ( c == '/' && in[1] == '/' ) {
			while (*in && *in != '\n') {
				in++;
			}
		// skip /* */ comments
		} else if ( c == '/' && in[1] == '*' ) {
			while ( *in && ( *in != '*' || in[1] != '/' ) ) 
				in++;
			if ( *in ) 
				in += 2;
			// record when we hit a newline
		} else if ( c == '\n' || c == '\r' ) {
			newline = qtrue;
			in++;
			// record when we hit whitespace
		} else if ( c == ' ' || c == '\t') {
			whitespace = qtrue;
			in++;
			// an actual token
		} else {
			// if we have a pending newline, emit it (and it counts as whitespace)
			if (newline) {
				*out++ = '\n';
				newline = qfalse;
				whitespace = qfalse;
			} else if (whitespace) {
				*out++ = ' ';
				whitespace = qfalse;
			}
			// copy quoted strings unmolested
			if (c == '"') {
				*out++ = c;
				in++;
				while (1) {
					c = *in;
					if (c && c != '"') {
						*out++ = c;
						in++;
					} else {
						break;
					}
				}
				if (c == '"') {
					*out++ = c;
					in++;
				}
			} else {
				*out++ = c;
				in++;
			}
		}
	}

	*out = '\0';

	return out - data_p;
}


const char *COM_ParseExt( ComParser *parser, const char **data_p, qboolean allowLineBreaks )
{
	int c = 0;
	qboolean hasNewLines = qfalse;
	const char *data = *data_p;
	int len = 0;
	parser->token[0] = '\0';
	parser->tokenline = 0;

	// make sure incoming data is valid
	if ( !data )
	{
		*data_p = NULL;
		return parser->token;
	}

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( parser, data, &hasNewLines );
		if ( !data )
		{
			*data_p = NULL;
			return parser->token;
		}
		if ( hasNewLines && !allowLineBreaks )
		{
			*data_p = data;
			return parser->token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			data += 2;
			while (*data && *data != '\n') {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' )
		{
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				if ( *data == '\n' )
				{
					parser->lines++;
				}
				data++;
			}
			if ( *data )
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	// token starts on this line
	parser->tokenline = parser->lines;

	// handle quoted strings
	if ( c == '"' )
	{
		data++;
		while ( 1 )
		{
			c = *data;
			if ( c == '"' || c == '\0' )
			{
				if ( c == '"' )
					data++;
				parser->token[ len ] = '\0';
				*data_p = data;
				return parser->token;
			}
			data++;
			if ( c == '\n' )
			{
				parser->lines++;
			}
			if ( len < ARRAY_LEN( parser->token )-1 )
			{
				parser->token[ len ] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < ARRAY_LEN( parser->token )-1 )
		{
			parser->token[ len ] = c;
			len++;
		}
		data++;
		c = *data;
	} while ( c > ' ' );

	parser->token[ len ] = '\0';

	*data_p = data;
	return parser->token;
}
	

/*
==============
COM_ParseComplex
==============
*/
char *COM_ParseComplex( ComParser *parser, const char **data_p, qboolean allowLineBreaks )
{
	static const byte is_separator[ 256 ] =
	{
	// \0 . . . . . . .\b\t\n . .\r . .
		1,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,
	//  . . . . . . . . . . . . . . . .
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	//    ! " # $ % & ' ( ) * + , - . /
		1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0, // excl. '-' '.' '/'
	//  0 1 2 3 4 5 6 7 8 9 : ; < = > ?
		0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,
	//  @ A B C D E F G H I J K L M N O
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	//  P Q R S T U V W X Y Z [ \ ] ^ _
		0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,0, // excl. '\\' '_'
	//  ` a b c d e f g h i j k l m n o
		1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	//  p q r s t u v w x y z { | } ~
		0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1
	};

	const byte *str = (byte*)*data_p;
	int c;
	int len = 0;
	int shift = 0; // token line shift relative to parser->lines
	parser->tokentype = TK_GENEGIC;

__reswitch:
	switch ( *str )
	{
	case '\0':
		parser->tokentype = TK_EOF;
		break;

	// whitespace
	case ' ':
	case '\t':
		str++;
		while ( (c = *str) == ' ' || c == '\t' )
			str++;
		goto __reswitch;

	// newlines
	case '\n':
	case '\r':
	parser->lines++;
		if ( *str == '\r' && str[1] == '\n' )
			str += 2; // CR+LF
		else
			str++;
		if ( !allowLineBreaks ) {
			parser->tokentype = TK_NEWLINE;
			break;
		}
		goto __reswitch;

	// comments, single slash
	case '/':
		// until end of line
		if ( str[1] == '/' ) {
			str += 2;
			while ( (c = *str) != '\0' && c != '\n' && c != '\r' )
				str++;
			goto __reswitch;
		}

		// comment
		if ( str[1] == '*' ) {
			str += 2;
			while ( (c = *str) != '\0' && ( c != '*' || str[1] != '/' ) ) {
				if ( c == '\n' || c == '\r' ) {
					parser->lines++;
					if ( c == '\r' && str[1] == '\n' ) // CR+LF?
						str++;
				}
				str++;
			}
			if ( c != '\0' && str[1] != '\0' ) {
				str += 2;
			} else {
				// FIXME: unterminated comment?
			}
			goto __reswitch;
		}

		// single slash
		parser->token[ len++ ] = *str++;
		break;

	// quoted string?
	case '"':
		str++; // skip leading '"'
		//parser->tokenline = parser->lines;
		while ( (c = *str) != '\0' && c != '"' ) {
			if ( c == '\n' || c == '\r' ) {
				parser->lines++; // FIXME: unterminated quoted string?
				shift++;
			}
			if ( len < MAX_TOKEN_CHARS-1 ) // overflow check
				parser->token[ len++ ] = c;
			str++;
		}
		if ( c != '\0' ) {
			str++; // skip ending '"'
		} else {
			// FIXME: unterminated quoted string?
		}
		parser->tokentype = TK_QUOTED;
		break;

	// single tokens:
	case '+': case '`':
	/*case '*':*/ case '~':
	case '{': case '}':
	case '[': case ']':
	case '?': case ',':
	case ':': case ';':
	case '%': case '^':
		parser->token[ len++ ] = *str++;
		break;

	case '*':
		parser->token[ len++ ] = *str++;
		parser->tokentype = TK_MATCH;
		break;

	case '(':
		parser->token[ len++ ] = *str++;
		parser->tokentype = TK_SCOPE_OPEN;
		break;

	case ')':
		parser->token[ len++ ] = *str++;
		parser->tokentype = TK_SCOPE_CLOSE;
		break;

	// !, !=
	case '!':
		parser->token[ len++ ] = *str++;
		if ( *str == '=' ) {
			parser->token[ len++ ] = *str++;
			parser->tokentype = TK_NEQ;
		}
		break;

	// =, ==
	case '=':
		parser->token[ len++ ] = *str++;
		if ( *str == '=' ) {
			parser->token[ len++ ] = *str++;
			parser->tokentype = TK_EQ;
		}
		break;

	// >, >=
	case '>':
		parser->token[ len++ ] = *str++;
		if ( *str == '=' ) {
			parser->token[ len++ ] = *str++;
			parser->tokentype = TK_GTE;
		} else {
			parser->tokentype = TK_GT;
		}
		break;

	//  <, <=
	case '<':
		parser->token[ len++ ] = *str++;
		if ( *str == '=' ) {
			parser->token[ len++ ] = *str++;
			parser->tokentype = TK_LTE;
		} else {
			parser->tokentype = TK_LT;
		}
		break;

	// |, ||
	case '|':
		parser->token[ len++ ] = *str++;
		if ( *str == '|' ) {
			parser->token[ len++ ] = *str++;
			parser->tokentype = TK_OR;
		}
		break;

	// &, &&
	case '&':
		parser->token[ len++ ] = *str++;
		if ( *str == '&' ) {
			parser->token[ len++ ] = *str++;
			parser->tokentype = TK_AND;
		}
		break;

	// rest of the charset
	default:
		parser->token[ len++ ] = *str++;
		while ( !is_separator[ (c = *str) ] ) {
			if ( len < MAX_TOKEN_CHARS-1 )
				parser->token[ len++ ] = c;
			str++;
		}
		parser->tokentype = TK_STRING;
		break;

	} // switch ( *str )

	parser->tokenline = parser->lines - shift;
	parser->token[ len ] = '\0';
	*data_p = ( char * )str;
	return parser->token;
}


/*
==================
COM_MatchToken
==================
*/
static void COM_MatchToken( ComParser *parser, const char **buf_p, const char *match ) {
	const char *token = COM_Parse( parser, buf_p );
	if ( strcmp( token, match ) ) {
		Com_Error( ERR_DROP, "MatchToken: %s != %s", token, match );
	}
}


/*
=================
SkipBracedSection

The next token should be an open brace or set depth to 1 if already parsed it.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
qboolean SkipBracedSection( ComParser *parser, const char **program, int depth ) {
	do {
		const char *token = COM_ParseExt( parser, program, qtrue );
		if( token[1] == 0 ) {
			if( token[0] == '{' ) {
				depth++;
			}
			else if( token[0] == '}' ) {
				depth--;
			}
		}
	} while( depth && *program );

	return ( depth == 0 );
}


/*
=================
SkipRestOfLine
=================
*/
void SkipRestOfLine( ComParser *parser, const char **data ) {
	const char *p = *data;

	if ( !*p )
		return;

	int c;
	while ( (c = *p) != '\0' ) {
		p++;
		if ( c == '\n' ) {
			parser->lines++;
			break;
		}
	}

	*data = p;
}


void Parse1DMatrix( ComParser *parser, const char **buf_p, int x, float *m ) {
	COM_MatchToken( parser, buf_p, "(" );

	for (int i = 0 ; i < x; i++) {
		const char *token = COM_Parse( parser, buf_p );
		m[i] = Q_atof( token );
	}

	COM_MatchToken( parser, buf_p, ")" );
}


void Parse2DMatrix( ComParser *parser, const char **buf_p, int y, int x, float *m ) {
	COM_MatchToken( parser, buf_p, "(" );

	for (int i = 0 ; i < y ; i++) {
		Parse1DMatrix( parser, buf_p, x, m + i * x );
	}

	COM_MatchToken( parser, buf_p, ")" );
}


void Parse3DMatrix( ComParser *parser, const char **buf_p, int z, int y, int x, float *m ) {
	COM_MatchToken( parser, buf_p, "(" );

	for (int i = 0 ; i < z ; i++) {
		Parse2DMatrix( parser, buf_p, y, x, m + i * x*y );
	}

	COM_MatchToken( parser, buf_p, ")" );
}


static int Hex( char c )
{
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	else
	if ( c >= 'A' && c <= 'F' ) {
		return 10 + c - 'A';
	}
	else
	if ( c >= 'a' && c <= 'f' ) {
		return 10 + c - 'a';
	}

	return -1;
}


/*
===================
Com_HexStrToInt
===================
*/
int Com_HexStrToInt( const char *str )
{
	if ( !str )
		return -1;

	// check for hex code
	if ( str[ 0 ] == '0' && str[ 1 ] == 'x' && str[ 2 ] != '\0' )
	{
		int i, digit, n = 0, len = strlen( str );

		for( i = 2; i < len; i++ )
		{
			n *= 16;

			digit = Hex( str[ i ] );

			if ( digit < 0 )
				return -1;

			n += digit;
		}

		return n;
	}

	return -1;
}


qboolean Com_GetHashColor( const char *str, byte *color )
{
	int hex[6];

	color[0] = color[1] = color[2] = 0;

	if ( *str++ != '#' ) {
		return qfalse;
	}

	int len = (int)strlen( str );
	if ( len <= 0 || len > 6 ) {
		return qfalse;
	}

	for ( int i = 0; i < len; i++ ) {
		hex[i] = Hex( str[i] );
		if ( hex[i] < 0 ) {
			return qfalse;
		}
	}

	switch ( len ) {
		case 3: // #rgb
			color[0] = hex[0] << 4 | hex[0];
			color[1] = hex[1] << 4 | hex[1];
			color[2] = hex[2] << 4 | hex[2];
			break;
		case 6: // #rrggbb
			color[0] = hex[0] << 4 | hex[1];
			color[1] = hex[2] << 4 | hex[3];
			color[2] = hex[4] << 4 | hex[5];
			break;
		default: // unsupported format
			return qfalse;
	}

	return qtrue;
}


/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

const byte locase[ 256 ] = {
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
	0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
	0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
	0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x5b,0x5c,0x5d,0x5e,0x5f,
	0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
	0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
	0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
	0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
	0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
	0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
	0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
	0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
	0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
	0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
	0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,
	0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
	0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,
	0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
	0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
	0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
	0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
	0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
	0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
	0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};


int Q_isprint( int c )
{
	if ( c >= 0x20 && c <= 0x7E )
		return ( 1 );
	return ( 0 );
}


int Q_islower( int c )
{
	if (c >= 'a' && c <= 'z')
		return ( 1 );
	return ( 0 );
}


int Q_isupper( int c )
{
	if (c >= 'A' && c <= 'Z')
		return ( 1 );
	return ( 0 );
}


int Q_isalpha( int c )
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ( 1 );
	return ( 0 );
}


qboolean Q_isanumber( const char *s )
{
	if( *s == '\0' )
        return qfalse;

	char *p;
	strtod( s, &p );

    return *p == '\0';
}


qboolean Q_isintegral( float f )
{
    return (int)f == f;
}


/*
=============
Q_strncpyz
 
Safe strncpy that ensures a trailing zero
=============
*/
void Q_strncpyz( char *dest, const char *src, int destsize ) 
{
	if ( !dest ) 
	{
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL dest" );
	}

	if ( !src ) 
	{
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL src" );
	}

	if ( destsize < 1 )
	{
		Com_Error(ERR_FATAL,"Q_strncpyz: destsize < 1" );
	}
#if 1 
	// do not fill whole remaining buffer with zeros
	// this is obvious behavior change but actually it may affect only buggy QVMs
	// which passes overlapping or short buffers to cvar reading routines
	// what is rather good than bad because it will no longer cause overwrites, maybe
	while ( --destsize > 0 && (*dest++ = *src++) != '\0' )
		;
	*dest = '\0';
#else
	strncpy( dest, src, destsize-1 );
	dest[ destsize-1 ] = '\0';
#endif
}


/*
=============
Q_strncpy

allows src and dest to be overlapped for QVM compatibility purposes
=============
*/
char *Q_strncpy( char *dest, const char *src, int destsize )
{
	int src_len = (int)strlen( src );

	if ( src_len > destsize ) {
#ifdef _DEBUG
		Com_Printf( S_COLOR_YELLOW "Q_strncpy: overlapped (src >= dst) buffers\n" );
#endif
		src_len = destsize;
	}

	memmove( dest, src, src_len );
	memset( dest + src_len, '\0', destsize - src_len );

	return dest;
}

/*
=============
Q_stricmpn
=============
*/
int Q_stricmpn( const char *s1, const char *s2, int n ) {
	// bk001129 - moved in 1.17 fix not in id codebase
        if ( s1 == NULL ) {
           if ( s2 == NULL )
             return 0;
           else
             return -1;
        }
        else if ( s2==NULL )
          return 1;


	int c1, c2;
	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}
		
		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z') {
				c1 -= ('a' - 'A');
			}
			if (c2 >= 'a' && c2 <= 'z') {
				c2 -= ('a' - 'A');
			}
			if (c1 != c2) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while (c1);
	
	return 0;		// strings are equal
}

int Q_stricmp( const char *s1, const char *s2 )
{
	if ( s1 == NULL )
	{
		if ( s2 == NULL )
			return 0;
		else
			return -1;
	}
	else if ( s2 == NULL )
		return 1;

	unsigned char c1, c2;
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if ( c1 != c2 ) 
		{

			if ( c1 <= 'Z' && c1 >= 'A' )
				c1 += ('a' - 'A');

			if ( c2 <= 'Z' && c2 >= 'A' )
				c2 += ('a' - 'A');

			if ( c1 != c2 ) 
				return c1 < c2 ? -1 : 1;
		}
	}
	while ( c1 != '\0' );

	return 0;
}


char *Q_strlwr( char *s1 ) {
	char *s = s1;
	while ( *s ) {
		*s = locase[(byte)*s];
		s++;
	}
	return s1;
}


char *Q_strupr( char *s1 ) {
	char *s = s1;
	while ( *s ) {
		if ( *s >= 'a' && *s <= 'z' )
			*s = *s - 'a' + 'A';
		s++;
	}
	return s1;
}



char *Q_stradd( char *dst, const char *src )
{
	char c;
	while ( (c = *src++) != '\0' )
		*dst++ = c;
	*dst = '\0';
	return dst;
}


/*
* Find the first occurrence of find in s.
*/
const char *Q_stristr( const char *s, const char *find)
{
  char c;
  if ((c = *find++) != 0)
  {
    if (c >= 'a' && c <= 'z')
    {
      c -= ('a' - 'A');
    }
    size_t len = strlen(find);
    do
    {
      char sc;
      do
      {
        if ((sc = *s++) == 0)
          return NULL;
        if (sc >= 'a' && sc <= 'z')
        {
          sc -= ('a' - 'A');
        }
      } while (sc != c);
    } while (Q_stricmpn(s, find, len) != 0);
    s--;
  }
  return s;
}


int Q_replace( const char *str1, const char *str2, char *src, int max_len )
{
	char *match = strstr( src, str1 );

	if ( !match )
		return 0;

	int count = 0;
	int len1 = strlen( str1 );
	int len2 = strlen( str2 );
	int d = len2 - len1;

    if ( d > 0 ) // expand and replace mode
    {
        const char *max = src + max_len;
        src += strlen( src );

        do
        {
            // expand source string
			const char *s1 = src;
            src += d;
            if ( src >= max )
                return count;
            char *dst = src;

            const char *s0 = match + len1;

            while ( s1 >= s0 )
                *dst-- = *s1--;

			// replace match
            const char *s2 = str2;
			while ( *s2 ) {
                *match++ = *s2++;
			}
            match = strstr( match, str1 );

            count++;
        }
        while ( match );

        return count;
    }
    else
    if ( d < 0 ) // shrink and replace mode
    {
        do
        {
            // shrink source string
            const char *s1 = match + len1;
            char *dst = match + len2;
            while ( (*dst++ = *s1++) != '\0' );

			//replace match
            const char *s2 = str2;
			while ( *s2 ) {
				*match++ = *s2++;
			}

            match = strstr( match, str1 );

            count++;
        }
        while ( match );

        return count;
    }
    else
    do  // just replace match
    {
        const char *s2 = str2;
		while ( *s2 ) {
			*match++ = *s2++;
		}

        match = strstr( match, str1 );
        count++;
	}
    while ( match );

	return count;
}


int Q_PrintStrlen( const char *string ) {
	if( !string ) {
		return 0;
	}

	int len = 0;
	const char *p = string;
	while( *p ) {
		if( Q_IsColorString( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}


char *Q_CleanStr( char *string ) {
	char *s = string;
	char *d = string;
	int c;
	while ((c = *s) != 0 ) {
		if ( Q_IsColorString( s ) ) {
			s++;
		}		
		else if ( c >= 0x20 && c <= 0x7E ) {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}


int Q_CountChar(const char *string, char tocount)
{
	int count = 0;

	for(; *string; string++)
	{
		if(*string == tocount)
			count++;
	}
	
	return count;
}

/*
=============
Q_UTF8_NextCodepoint

Reads the next Unicode codepoint from a UTF-8 encoded string.
Returns the codepoint value (or -1 on invalid sequence).
Stores the number of bytes consumed in *bytesRead (1-4).
=============
*/
int Q_UTF8_NextCodepoint( const char *str, int *bytesRead )
{
    const unsigned char *s = (const unsigned char *)str;

    if ( !s || !s[0] ) {
        if ( bytesRead ) *bytesRead = 0;
        return 0;
    }

    int cp;
    int expect;
    if ( s[0] < 0x80 ) {
        // ASCII
        if ( bytesRead ) *bytesRead = 1;
        return s[0];
    }
    else if ( ( s[0] & 0xE0 ) == 0xC0 ) {
        cp = s[0] & 0x1F;
        expect = 2;
    }
    else if ( ( s[0] & 0xF0 ) == 0xE0 ) {
        cp = s[0] & 0x0F;
        expect = 3;
    }
    else if ( ( s[0] & 0xF8 ) == 0xF0 ) {
        cp = s[0] & 0x07;
        expect = 4;
    }
    else {
        // invalid lead byte
        if ( bytesRead ) *bytesRead = 1;
        return -1;
    }

    for ( int i = 1; i < expect; i++ ) {
        if ( ( s[i] & 0xC0 ) != 0x80 ) {
            // truncated or invalid continuation
            if ( bytesRead ) *bytesRead = i;
            return -1;
        }
        cp = ( cp << 6 ) | ( s[i] & 0x3F );
    }

    // reject overlong encodings
    if ( ( expect == 2 && cp < 0x80 ) ||
         ( expect == 3 && cp < 0x800 ) ||
         ( expect == 4 && cp < 0x10000 ) ) {
        if ( bytesRead ) *bytesRead = expect;
        return -1;
    }

    // reject surrogates and out-of-range
    if ( cp >= 0xD800 && cp <= 0xDFFF ) {
        if ( bytesRead ) *bytesRead = expect;
        return -1;
    }
    if ( cp > 0x10FFFF ) {
        if ( bytesRead ) *bytesRead = expect;
        return -1;
    }

    if ( bytesRead ) *bytesRead = expect;
    return cp;
}

/*
=============
Q_UTF8_Strlen

Returns the number of Unicode codepoints in a UTF-8 string.
Invalid sequences are counted as one codepoint each.
=============
*/
int Q_UTF8_Strlen( const char *str )
{
    int count = 0;
    int bytes;

    while ( str && *str ) {
        Q_UTF8_NextCodepoint( str, &bytes );
        if ( bytes <= 0 ) bytes = 1; // skip invalid byte
        str += bytes;
        count++;
    }

    return count;
}

/*
=============
Q_UTF8_Encode

Encodes a Unicode codepoint into UTF-8 bytes.
Writes 1-4 bytes to out (must have room for at least 4 bytes).
Returns the number of bytes written, or 0 on invalid codepoint.
Does NOT null-terminate.
=============
*/
int Q_UTF8_Encode( int codepoint, char *out )
{
    unsigned char *o = (unsigned char *)out;

    if ( codepoint < 0 || ( codepoint >= 0xD800 && codepoint <= 0xDFFF ) || codepoint > 0x10FFFF ) {
        return 0;
    }

    if ( codepoint < 0x80 ) {
        o[0] = (unsigned char)codepoint;
        return 1;
    }

    if ( codepoint < 0x800 ) {
        o[0] = 0xC0 | ( codepoint >> 6 );
        o[1] = 0x80 | ( codepoint & 0x3F );
        return 2;
    }

    if ( codepoint < 0x10000 ) {
        o[0] = 0xE0 | ( codepoint >> 12 );
        o[1] = 0x80 | ( ( codepoint >> 6 ) & 0x3F );
        o[2] = 0x80 | ( codepoint & 0x3F );
        return 3;
    }

    o[0] = 0xF0 | ( codepoint >> 18 );
    o[1] = 0x80 | ( ( codepoint >> 12 ) & 0x3F );
    o[2] = 0x80 | ( ( codepoint >> 6 ) & 0x3F );
    o[3] = 0x80 | ( codepoint & 0x3F );
    return 4;
}

/*
=============
Q_UTF8_Advance

Advances the string pointer by n codepoints.
Returns pointer to the new position, or pointer to
the null terminator if the string is shorter than n codepoints.
=============
*/
const char *Q_UTF8_Advance( const char *str, int n )
{
    int bytes;

    while ( str && *str && n > 0 ) {
        Q_UTF8_NextCodepoint( str, &bytes );
        if ( bytes <= 0 ) bytes = 1;
        str += bytes;
        n--;
    }

    return str;
}


#if	defined(_DEBUG) && defined(_WIN32)
#include <windows.h>
#endif

int QDECL Com_sprintf( char *dest, int size, const char *fmt, ...)
{
	if ( !dest )
	{
		Com_Error( ERR_FATAL, "Com_sprintf: NULL dest" );
#if	defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		return 0;
	}

	va_list argptr;
	va_start( argptr, fmt );
	int len = vsnprintf( dest, size, fmt, argptr );
	va_end( argptr );

	if ( len < 0 ) 
	{
		// encoding error
        dest[0] = '\0';
		Com_Error( ERR_FATAL, "Com_sprintf: encoding error" );
#if	defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		return 0;
	}

	if ( len >= size ) 
	{
		Com_Printf( S_COLOR_YELLOW "Com_sprintf: overflow of %i in %i\n", len, size );
#if	defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		len = size - 1;
	}

	return len;
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
#define VA_BUFCOUNT 4
#define VA_BUFSIZE  32000

const char *QDECL va( const char *format, ... )
{
	static char	string[VA_BUFCOUNT][VA_BUFSIZE];	// in case va is called by nested functions
	static int	index = 0;
	char *buf = string[ index ];
	index = ( index + 1 ) & ( VA_BUFCOUNT - 1 );

	va_list argptr;
	va_start( argptr, format );
	vsnprintf( buf, sizeof(string[0]), format, argptr );
	va_end( argptr );

	return buf;
}


/*
============
Com_TruncateLongString

Assumes buffer is at least TRUNCATE_LENGTH big
============
*/
void Com_TruncateLongString( char *buffer, const char *s )
{
	int length = strlen( s );

	if( length <= TRUNCATE_LENGTH ) {
		Q_strncpyz( buffer, s, TRUNCATE_LENGTH );
		return;
	}

	Q_strncpyz( buffer, s, ( TRUNCATE_LENGTH / 2 ) - 3 );

	qstring_t buf_qs = QS_WrapExisting( buffer, TRUNCATE_LENGTH );
	QS_Append( &buf_qs, " ... " );
	QS_Append( &buf_qs, s + length - ( TRUNCATE_LENGTH / 2 ) + 3 );
}


/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

static qboolean Q_strkey( const char *str, const char *key, int key_len )
{
	for ( int i = 0; i < key_len; i++ )
	{
		if ( locase[ (byte)str[i] ] != locase[ (byte)key[i] ] )
		{
			return qfalse;
		}
	}

	return qtrue;
}


/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
#define INFO_VFK_BUFCOUNT 4

const char *Info_ValueForKey( const char *s, const char *key )
{
	static	char value[INFO_VFK_BUFCOUNT][BIG_INFO_VALUE];	// use multiple buffers so compares
															// work without stomping on each other
	static	int	valueindex = 0;

	if ( !s || !key || !*key )
		return "";

	int klen = (int)strlen( key );

	if ( *s == '\\' )
		s++;

	while (1)
	{
		const char *pkey = s;
		while ( *s != '\\' )
		{
			if ( *s == '\0' )
				return "";
			++s;
		}
		int len = (int)(s - pkey);
		s++; // skip '\\'

		const char *v = s;
		while ( *s != '\\' && *s !='\0' )
			s++;

		if ( len == klen && Q_strkey( pkey, key, klen ) )
		{
			char *o2 = value[ valueindex ];
			char *o = o2;
			valueindex = ( valueindex + 1 ) & ( INFO_VFK_BUFCOUNT - 1 );

			if ( (int)(s - v) >= BIG_INFO_VALUE )
			{
				Com_Error( ERR_DROP, "Info_ValueForKey: oversize infostring" );
			}
			else
			{
				while ( v < s )
					*o++ = *v++;
			}
			*o = '\0';
			return o2;
		}

		if ( *s == '\0' )
			break;

		s++;
	}

	return "";
}


/*
===================
Info_Tokenize

Tokenizes all key/value pairs from specified infostring
NOT suitable for big infostrings
===================
*/
void Info_Tokenize( InfoTokens *tokens, const char *s )
{
	char *o = tokens->buffer;

	tokens->count = 0;
	*o = '\0';

	for ( ;; )
	{
		while ( *s == '\\' ) // skip leading/trailing separators
			s++;

		if ( *s == '\0' )
			break;

		tokens->keys[ tokens->count ] = o;
		while ( *s != '\\' )
		{
			if ( *s == '\0' )
			{
				*o = '\0'; // terminate key
				tokens->values[ tokens->count++ ] = o;
				return;
			}
			*o++ = *s++;
		}
		*o++ = '\0'; // terminate key
		s++; // skip '\\'

		tokens->values[ tokens->count++ ] = o;
		while ( *s != '\\' && *s != '\0' )
		{
			*o++ = *s++;
		}
		*o++ = '\0';
	}
}


/*
===================
Info_ValueForKeyToken

Fast lookup from tokenized infostring
===================
*/
const char *Info_ValueForKeyToken( const InfoTokens *tokens, const char *key )
{
	for ( int i = 0; i < tokens->count; i++ )
	{
		if ( Q_stricmp( tokens->keys[ i ], key ) == 0 )
		{
			return tokens->values[ i ];
		}
	}

	return "";
}


/*
===================
Info_NextPair

Used to iterate through all the key/value pairs in an info string

Callers MUST pass key and value buffers of at least BIG_INFO_KEY /
BIG_INFO_VALUE bytes - this function bounds the writes to those sizes
to avoid overflowing if the input contains an unusually long token,
which used to crash /systeminfo /serverinfo /clientinfo /dumpuser when
a single token exceeded the legacy 512-byte buffer size.
===================
*/
const char *Info_NextPair( const char *s, char *key, char *value ) {
	if ( *s == '\\' ) {
		s++;
	}

	key[0] = '\0';
	value[0] = '\0';

	char *o = key;
	int l = 0;
	while ( *s != '\\' ) {
		if ( !*s ) {
			*o = '\0';
			return s;
		}
		if ( l >= BIG_INFO_KEY - 1 ) {
			// truncate the key safely; consume the rest of the token
			// from the input so the parser stays in sync
			while ( *s && *s != '\\' ) {
				s++;
			}
			break;
		}
		*o++ = *s++;
		l++;
	}
	*o = '\0';
	if ( *s == '\\' ) {
		s++;
	}

	o = value;
	l = 0;
	while ( *s != '\\' && *s ) {
		if ( l >= BIG_INFO_VALUE - 1 ) {
			while ( *s && *s != '\\' ) {
				s++;
			}
			break;
		}
		*o++ = *s++;
		l++;
	}
	*o = '\0';

	return s;
}


/*
===================
Info_RemoveKey

return removed character count
===================
*/
int Info_RemoveKey( char *s, const char *key )
{
	int key_len = (int)strlen( key );
	int ret = 0;

	while ( 1 ) {
		char *start = s;
		if ( *s == '\\' ) {
			++s;
		}
		const char *pkey = s;
		while ( *s != '\\' ) {
			if ( *s == '\0' ) {
				if ( s != start ) {
					// remove any trailing empty keys
					*start = '\0';
					ret += (int)(s - start);
				}
				return ret;
			}
			++s;
		}
		int len = (int)(s - pkey);
		++s; // skip '\\'

		while ( *s != '\\' && *s != '\0' ) {
			++s;
		}

		if ( len == key_len && Q_strkey( pkey, key, key_len ) ) {
			memmove( start, s, strlen( s ) + 1 ); // remove this part
			ret += (int)(s - start);
			s = start;
		}

		if ( *s == '\0' ) {
			break;
		}

	}

	return ret;
}


/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate( const char *s )
{
	for ( ;; )
	{
		switch ( *s++ )
		{
		case '\0':
			return qtrue;
		case '\"':
		case ';':
			return qfalse;
		default:
			if ( !Q_isprint( *(s - 1) ) )
				return qfalse;
			break;
		}
	}
}


/*
==================
Info_ValidateKeyValue

Some characters are illegal in key values because they
can mess up the server's parsing
==================
*/
qboolean Info_ValidateKeyValue( const char *s )
{
	for ( ;; )
	{
		switch ( *s++ )
		{
		case '\0':
			return qtrue;
		case '\\':
		case '\"':
		case ';':
			return qfalse;
		default:
			break;
		}
	}
}


/*
==================
Info_SetValueForKey_s

Changes or adds a key/value pair
==================
*/
qboolean Info_SetValueForKey_s( char *s, int slen, const char *key, const char *value ) {
	char newi[BIG_INFO_STRING+2];
	int len1 = (int)strlen( s );

	if ( len1 >= slen ) {
		Com_Printf( S_COLOR_YELLOW "Info_SetValueForKey(%s): oversize infostring\n", key );
		return qfalse;
	}

	if ( !key || !Info_ValidateKeyValue( key ) || *key == '\0' ) {
		Com_Printf( S_COLOR_YELLOW "Invalid key name: '%s'\n", key );
		return qfalse;
	}

	if ( value && !Info_ValidateKeyValue( value ) ) {
		Com_Printf( S_COLOR_YELLOW "Invalid value name: '%s'\n", value );
		return qfalse;
	}

	len1 -= Info_RemoveKey( s, key );
	if ( value == NULL || *value == '\0' ) {
		return qtrue;
	}

	int len2 = Com_sprintf( newi, sizeof( newi ), "\\%s\\%s", key, value );

	if ( len1 + len2 >= slen ) {
		Com_Printf( S_COLOR_YELLOW "Info string length exceeded for key '%s'\n", key );
		return qfalse;
	}

	strcpy( s + len1, newi );
	return qtrue;
}


//====================================================================


/*
==================
Com_SkipCharset
==================
*/
const char *Com_SkipCharset( const char *s, const char *sep )
{
	const char	*p = s;

	while( p )
	{
		if( strchr( sep, *p ) )
			p++;
		else
			break;
	}

	return p;
}


/*
==================
Com_SkipTokens
==================
*/
const char *Com_SkipTokens( const char *s, int numTokens, const char *sep )
{
	int		sepCount = 0;
	const char	*p = s;

	while( sepCount < numTokens )
	{
		if( strchr( sep, *p++ ) )
		{
			sepCount++;
			while( strchr( sep, *p ) )
				p++;
		}
		else if( *p == '\0' )
			break;
	}

	if( sepCount == numTokens )
		return p;
	else
		return s;
}
