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
	defined(__amd64__) || \
	defined(_WIN64) || \
	defined(__sparc64__) || \
	defined(__arch64__) || \
	defined(__powerpc64__) || \
	defined (__s390x__) 

	#define BOTTOM_UP_SIZE	9
	#define BOTTOM_UP_LAST	8
	#define BOTTOM_UP_ALL_ZEROS	0x00

	#if defined (__APPLE__)
		#include <libkern/OSByteOrder.h>
		#define judyvalue_reverse_bytes(A)	OSSwapHostToLittleInt64(A)
	#elif (BYTE_ORDER == BIG_ENDIAN)
		#warning "Big endian 64-bit implementation untested."
		inline judyvalue judyvalue_reverse_bytes(judyvalue val) {
			return	((val<<56) & 0xFF00000000000000) |
					((val<<40) & 0x00FF000000000000) |
					((val<<24) & 0x0000FF0000000000) |
					((val<< 8) & 0x000000FF00000000) |
					((val>> 8) & 0x00000000FF000000) |
					((val>>24) & 0x0000000000FF0000) |
					((val>>40) & 0x000000000000FF00) |
					((val>>56) & 0x00000000000000FF))
		}
	#endif

#else
	#define BOTTOM_UP_SIZE	5
	#define BOTTOM_UP_LAST	4
	#define BOTTOM_UP_ALL_ZEROS	0xF0

	#if defined (__APPLE__)
		#include <libkern/OSByteOrder.h>
		#define judyvalue_reverse_bytes(A)	OSSwapHostToLittleInt32(A)
	#elif (BYTE_ORDER == BIG_ENDIAN)
		inline judyvalue judyvalue_reverse_bytes(judyvalue val) {
			return	((val<<24) & 0xFF000000) |
					((val<< 8) & 0x00FF0000) |
					((val>> 8) & 0x0000FF00) |
					((val>>24) & 0x000000FF);
		}
	#endif

#endif

#if (BYTE_ORDER == BIG_ENDIAN)
	#define judyvalueBottomUpBytes(A)	judyvalue_reverse_bytes(A)
#else
	#define judyvalueBottomUpBytes(A)	A
#endif


void judyvalue_native_to_bottom_up(judyvalue index, uchar *buff) {
	judyvalue *judyvalue_in_buff = (judyvalue *)buff;
	*judyvalue_in_buff = judyvalueBottomUpBytes(index);
	
	uchar *zero_toggles = &(buff[BOTTOM_UP_LAST]);
	*zero_toggles = 0xFF;
	
	for (int i = 0; i < sizeof(judyvalue); i++) {
		if (buff[i] == 0x00) {
			*zero_toggles ^= (0x01 << i);
			buff[i] = 0x01;
		}
	}
}

judyvalue judyvalue_bottom_up_to_native(uchar *buff) {
	judyvalue index;
	
	uchar *zero_toggles = &(buff[BOTTOM_UP_LAST]);
	
	if (*zero_toggles == BOTTOM_UP_ALL_ZEROS) {
		return 0;
	}
	else if (*zero_toggles != 0xFF) {
		for (int i = 0; i < sizeof(judyvalue); i++) {
			if ((*zero_toggles & (0x01 << i)) == 0x00) {
				buff[i] = 0x00;
			}
		}
	}
	
	index = judyvalueBottomUpBytes(*((judyvalue *)buff));
	return index;
}

int main(int argc, char **argv) {
	uchar buff[1024];
	FILE *in, *out;
	
	judyvalue index;					// array index
	judyvalue value;					// array element value
	judyslot *cell;						// pointer to array element value
	
	void *judy;							// pointer to Judy array
	
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
	judyvalue test = 0;
	
	do {
		index = test;
		judyvalue_native_to_bottom_up(index, (uchar *)buff);
		index = judyvalue_bottom_up_to_native(buff);
		if (index != test) {
			printf("Encoding error: %"PRIjudyvalue "\n", test);
		}
		test++;
	} while (test != 0);
#endif
	
	judy = judy_open(512);
	
	while( fgets((char *)buff, sizeof(buff), in) ) {
		if (sscanf((char *)buff, "%"PRIjudyvalue " %"PRIjudyvalue, &index, &value)) {
			judyvalue_native_to_bottom_up(index, (uchar *)buff);
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
	while (cell != NULL)
	{
		judy_key(judy, (uchar *)buff, sizeof(buff));
		index = judyvalue_bottom_up_to_native(buff);
		
		value = *cell;
		if (value == -1) value = 0;
		printf("%"PRIjudyvalue " %"PRIjudyvalue "\n", index, value);

		cell = judy_nxt(judy);
#define SYMMETRY_TEST	0
#if SYMMETRY_TEST
		cell = judy_prv(judy); // This will work if judy_prv() and judy_nxt() are symmetric. 
		cell = judy_nxt(judy);
#endif
	}

	printf("\n");

	cell = judy_end(judy);
	while (cell != NULL)
	{
		judy_key(judy, (uchar *)buff, sizeof(buff));
		index = judyvalue_bottom_up_to_native(buff);
		
		value = *cell;
		if (value == -1) value = 0;
		printf("%"PRIjudyvalue " %"PRIjudyvalue "\n", index, value);
		
		cell = judy_del(judy);
		//cell = judy_prv(judy);
	}

	return 0;
}