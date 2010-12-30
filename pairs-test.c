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

#if defined(__LP64__) || \
	defined(__x86_64__) || \
	defined(__sparc64__) || \
	defined(__arch64__) || \
	defined(__powerpc64__) || \
	defined (__s390x__) 

	#define BOTTOM_UP_SIZE	9
	#define BOTTOM_UP_LAST	8
	#define BOTTOM_UP_ALL_ZEROS	0x00

	#if (BYTE_ORDER == BIG_ENDIAN)
		#error "Big endian 64-bit implementation incomplete."
	#endif

#else
	#define BOTTOM_UP_SIZE	5
	#define BOTTOM_UP_LAST	4
	#define BOTTOM_UP_ALL_ZEROS	0xF0

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

#endif

#if (BYTE_ORDER == BIG_ENDIAN)
	#define uintBottomUpBytes(A)	uintReverseBytes(A)
#else
	#define uintBottomUpBytes(A)	A
#endif


void uintNativeToBottomUp(uint index, uchar *buff) {
	uint *uint_in_buff = (uint *)buff;
	*uint_in_buff = uintBottomUpBytes(index);
	buff[BOTTOM_UP_LAST] = 0xFF;
	
	for (int i = 0; i < sizeof(uint); i++) {
		if (buff[i] == 0x00) {
			buff[BOTTOM_UP_LAST] ^= (0x01 << i);
			buff[i] = 0x01;
		}
	}
}

uint uintBottomUpToNative(uchar *buff) {
	uint index;
	
	if (buff[BOTTOM_UP_LAST] == BOTTOM_UP_ALL_ZEROS) {
		return 0;
	}
	else if (buff[BOTTOM_UP_LAST] != 0xFF) {
		for (int i = 0; i < sizeof(uint); i++) {
			if ((buff[BOTTOM_UP_LAST] & (0x01 << i)) == 0x00) {
				buff[i] = 0x00;
			}
		}
	}
	
	index = uintBottomUpBytes(*((uint *)buff));
	return index;
}

int main(int argc, char **argv) {
	uchar buff[1024];
	FILE *in, *out;
	
	judyvalue index;					// array index
	judyvalue value;					// array element value
	judyslot *cell;						// pointer to array element value
	
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
	
	
#if 0
	uint test = 0;
	
	do {
		index = test;
		uintNativeToBottomUp(index, (uchar *)buff);
		index = uintBottomUpToNative(buff);
		if (index != test) {
			printf("Encoding error: %"PRIuint "\n", test);
		}
		test++;
	} while (test != 0);
#endif
	
	judy = judy_open(512);
	
	while( fgets((char *)buff, sizeof(buff), in) ) {
		if (sscanf((char *)buff, "%"PRIuint " %"PRIuint, &index, &value)) {
			uintNativeToBottomUp(index, (uchar *)buff);
			cell = judy_cell(judy, buff, BOTTOM_UP_SIZE);
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
	cell = judy_nxt(judy);
	while (cell != NULL)
	{
		judy_key(judy, (uchar *)buff, sizeof(buff));
		index = uintBottomUpToNative(buff);
		
		value = *cell;
		if (value == -1) value = 0;
		printf("%"PRIjudyvalue " %"PRIjudyvalue "\n", index, value);

		cell = judy_nxt(judy);
#define SYMMETRY_TEST	1
#if SYMMETRY_TEST
		cell = judy_prv(judy); // This should work if judy_prv() and judy_nxt() are symmetric, 
		cell = judy_nxt(judy); // but it doesn’t and they currently aren’t
		// Only disabling the two lines above currently results in correctly working code
#endif
	}

	printf("\n");

	cell = judy_end(judy);
	while (cell != NULL)
	{
		judy_key(judy, (uchar *)buff, sizeof(buff));
		index = uintBottomUpToNative(buff);
		
		value = *cell;
		if (value == -1) value = 0;
		printf("%"PRIjudyvalue " %"PRIjudyvalue "\n", index, value);
		
		//judy_del(judy);
		cell = judy_prv(judy);
	}

	return 0;
}