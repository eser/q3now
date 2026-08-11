#include <limits.h>
#include <stdlib.h>
#include <string.h>

static volatile int sanitizer_probe_sink;

int main( int argc, char **argv ) {
	if ( argc != 2 ) return 2;
	if ( !strcmp( argv[1], "address" ) ) {
		int *value = (int *)malloc( sizeof( *value ) );
		if ( !value ) return 3;
		*value = 7;
		free( value );
		sanitizer_probe_sink = *value;
		return sanitizer_probe_sink == 7 ? 0 : 4;
	}
	if ( !strcmp( argv[1], "undefined" ) ) {
		volatile int value = INT_MAX;
		sanitizer_probe_sink = value + 1;
		return sanitizer_probe_sink == INT_MIN ? 0 : 5;
	}
	return 2;
}
