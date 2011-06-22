/*
 *  judy-utilities.c
 *  judy-arrays
 *
 *  Created by Jan on 09.02.11.
 *  Copyright 2011 geheimwerk.de. All rights reserved.
 *
 */

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

	#define BOTTOM_UP_MAX_JUDY_STACK_LEVELS	6

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

	#define BOTTOM_UP_MAX_JUDY_STACK_LEVELS	3

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


/*
 buff_size has to be >= buff_used_size + 2
 out_array should be a pointer to a uchar array of size 256. 
*/

uint judy_key_chars_below_key(Judy *judy, uchar *buff, uint buff_used_size, uint buff_size, uchar *out_array) {
    uint count = 0;
	int key_index = 0;
	char this_letter;
	char next_letter;
    judyslot *cell;

	const uchar *orig_buff = buff;

    size_t temp_buff_size = (buff_used_size == 0) ? 1+2 : buff_used_size+2;
	uchar temp_buff_array[temp_buff_size]; // Allocate on stack
	uchar *temp_buff = temp_buff_array;
	memcpy(temp_buff, orig_buff, buff_used_size);
	
	if (buff_used_size == 0) {
		cell = judy_strt(judy, NULL, 0);
		judy_key(judy, temp_buff, temp_buff_size);
		if (temp_buff[0] == 0) {
			cell = judy_nxt(judy);
		}
		
		if (!cell) {
			return 0;
		}
		
		// Recursively search each branch of the trie
		do {
			judy_key(judy, temp_buff, temp_buff_size);
			next_letter = temp_buff[key_index];
			
			if (next_letter != '\0') {
				out_array[count] = next_letter;
				count++;
			}
			
			next_letter += 1;
			temp_buff[key_index] = next_letter;
			temp_buff[key_index+1] = '\0';
			
			cell = judy_strt(judy, temp_buff, key_index+1);
			
		} while (cell);
	}
	else {
		key_index = buff_used_size-1;
		this_letter = temp_buff[key_index];
		int next_key_index = key_index + 1;

		cell = judy_strt(judy, temp_buff, key_index+1);
		
		//assert(next_key_index < temp_buff_size);
		
		do {
			judy_key(judy, temp_buff, temp_buff_size);
			
			if (temp_buff[key_index] == this_letter 
				&& memcmp(orig_buff, temp_buff, (size_t)key_index+1) == 0) {
				next_letter = temp_buff[next_key_index];
				
				if (next_letter != '\0') {
					out_array[count] = next_letter;
					count++;
				}

				next_letter += 1;
				memcpy(temp_buff, orig_buff, next_key_index);
				temp_buff[next_key_index] = next_letter;
				temp_buff[next_key_index+1] = '\0';
				
				cell = judy_strt(judy, temp_buff, next_key_index+1);
			}
			else {
				break;
			}
			
		} while (cell);
	}

	return count;
}
