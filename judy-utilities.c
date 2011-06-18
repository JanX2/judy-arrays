/*
 *  judy-utilities.c
 *  judy-arrays
 *
 *  Created by Jan on 09.02.11.
 *  Copyright 2011 geheimwerk.de. All rights reserved.
 *
 */

#include "judy-arrays.c"

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
