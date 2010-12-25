/*
 *  pairs-test.c
 *  judy-arrays
 *
 *  Created by Jan on 18.12.10.
 *  Copyright 2010 geheimwerk.de. All rights reserved.
 *
 */

#include <stdio.h>
#include "judy-arrays.c"

#ifdef __LP64__
	#if (BYTE_ORDER == BIG_ENDIAN)
	#error "Big endian 64-bit implementation incomplete."
	#else
		#define uintNativeToBottomUp(A)	A
		#define uintBottomUpToNative(A)	A
	#endif
#else
	#if (BYTE_ORDER == BIG_ENDIAN)
	uint uintReverseBytes(uint in) {
		char *c_in = (char *)&in;
		uint out;
		char *c_out = (char *)&out;
		
		c_out[0] = c_in[3];
		c_out[1] = c_in[2];
		c_out[2] = c_in[1];
		c_out[3] = c_in[0];
		
		return out;
	}
	#endif

	#if (BYTE_ORDER == BIG_ENDIAN)
		#define uintNativeToBottomUp(A)	uintReverseBytes(A)
		#define uintBottomUpToNative(A)	uintReverseBytes(A)
	#else
		#define uintNativeToBottomUp(A)	A
		#define uintBottomUpToNative(A)	A
	#endif
#endif

int main(int argc, char **argv) {
	uchar buff[1024];
	FILE *in, *out;
	
	uint index;							// array index
	uint value;							// array element value
	uint *cell;							// pointer to array element value
	
	void *judy;							// poiner to Judy array
	
	if( argc > 1 )
		in = fopen(argv[1], "r");
	else
		in = stdin;
	
	if( argc > 2 )
		out = fopen(argv[2], "w");
	else
		out = stdout;
	
	if( !in )
		fprintf(stderr, "unable to open input file\n");
	
	if( !out )
		fprintf(stderr, "unable to open output file\n");
	
	judy = judy_open(512);
	
	while( fgets((char *)buff, sizeof(buff), in) ) {
		if (sscanf((char *)buff, "%"PRIuint " %"PRIuint, &index, &value)) {
			index = uintNativeToBottomUp(index);
			cell = judy_cell(judy, (uchar *)&index, sizeof(index));
			if (value) {
				*cell = value;                 // store new value
			} else {
				*cell = -1;
			}
		}
	}
	// Next, visit all the stored indexes in sorted order, first ascending,
	// then descending, and delete each index during the descending pass.
	
	index = 0;
	cell = judy_strt(judy, NULL, 0);
	while (cell != NULL)
	{
		judy_key(judy, (uchar *)buff, sizeof(buff));
		index = uintBottomUpToNative((uint)*buff);
		
		value = *cell;
		if (value == -1) {
			value = 0;
		}
		printf("%"PRIuint " %"PRIuint "\n", index, value);

		cell = judy_nxt(judy);
	}

	printf("\n");

	return 0;
}