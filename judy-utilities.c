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
	char thisLetter;
	char nextLetter;
    judyslot *cell;

	size_t current_word_len = strlen((const char *)buff);
	char current_word[current_word_len+1];
	strncpy((char *)&current_word, (const char *)buff, current_word_len);
	
	if (buff_used_size == 0) {
		cell = judy_strt(judy, NULL, 0);
		judy_key(judy, buff, buff_size);
		if (buff[0] == 0) {
			cell = judy_nxt(judy);
		}
		
		if (!cell) {
			return 0;
		}
		
		// Recursively search each branch of the trie
		do {
			judy_key(judy, buff, buff_size);
			nextLetter = buff[key_index];
			
            out_array[count] = nextLetter;
            count++;
			
			nextLetter += 1;
			buff[key_index] = nextLetter;
			buff[key_index+1] = '\0';
			
			cell = judy_strt(judy, buff, key_index+1);
			
		} while (cell);
	}
	else {
		key_index = buff_used_size-1;
		thisLetter = buff[key_index];
		int next_key_index = key_index + 1;

		cell = judy_strt(judy, buff, key_index+1);
		
		//assert(next_key_index < buff_size);
		
		do {
			judy_key(judy, buff, buff_size);
			
			if (buff[key_index] == thisLetter 
				&& strncmp((const char *)current_word, (const char *)buff, (size_t)key_index+1) == 0) {
				nextLetter = buff[next_key_index];
				
				if (nextLetter != '\0') {
					out_array[count] = nextLetter;
					count++;
				}

				nextLetter += 1;
				strncpy((char *)buff, (const char *)current_word, next_key_index);
				buff[next_key_index] = nextLetter;
				buff[next_key_index+1] = '\0';
				
				cell = judy_strt(judy, buff, next_key_index+1);
			}
			else {
				break;
			}
			
		} while (cell);
	}

	// Restore word in buff
	strncpy((char *)buff, (const char *)current_word, current_word_len);

	return count;
}
