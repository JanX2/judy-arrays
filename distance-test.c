/*
 *  distance-test.c
 *  judy-arrays
 *
 *  Created by Jan on 21.01.11.
 *  Copyright 2011 geheimwerk.de. All rights reserved.
 *
 */

#include <stdio.h>
#include <strings.h>
#include "judy-arrays.c"


#define DICTIONARY	"/usr/share/dict/words"
#define TARGET		"goober"
#define MAX_COST	1

FILE *in, *out;
void *judy;

#ifndef MIN
	#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
		#define MIN(a,b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a < _b) ? _a : _b; })
	#else
		#define MIN(A,B)	((A) < (B) ? (A) : (B))
	#endif
#else
	#warning MIN is already defined, MIN(a, b) may not behave as expected.
#endif

// Return the minimum of a, b and c
int jxld_smallestInt(int a, int b, int c) {
	int min = a;
	if ( b < min )
		min = b;
	
	if ( c < min )
		min = c;
	
	return min;
}

// This recursive helper is used by the search function above. It assumes that
// the previousRow has been filled in already.
void searchRecursive(judyslot *cell, uchar *key_buffer, int key_buffer_size, int key_index, char prevLetter, char thisLetter, const char *word, int columns, int *penultimateRow, int *previousRow, void *results, int maxCost) {
	
	int currentRowLastIndex = columns - 1;
	int currentRow[columns];
	currentRow[0] = previousRow[0] + 1;
	
	int cost;
	int insertCost;
	int deleteCost;
	int replaceCost;
	
	int column;
	
	// Build one row for the letter, with a column for each letter in the target
	// word, plus one for the empty string at column 0
	for (column = 1; column < columns; column++) {
		
		insertCost = currentRow[column - 1] + 1;
		deleteCost = previousRow[column] + 1;
		
		if (word[column - 1] != thisLetter) {
			cost = 1;
		}
		else {
			cost = 0;
		}
		replaceCost = previousRow[column - 1] + cost;
		
		currentRow[column] = jxld_smallestInt(insertCost, deleteCost, replaceCost);
		
#ifndef DISABLE_DAMERAU_TRANSPOSITION
		// This conditional adds Damerau transposition to the Levenshtein distance
		if (column > 1 && penultimateRow != NULL
			&& word[column - 1] == prevLetter 
			&& word[column - 2] == thisLetter )
		{
			currentRow[column] = MIN(currentRow[column],
									 penultimateRow[column - 2] + cost );
		}
#endif
	}
	
	// If the last entry in the row indicates the optimal cost is less than the
	// maximum cost, and there is a word in this trie cell, then add it.
	if (currentRow[currentRowLastIndex] <= maxCost && *cell > 0) {
		/*[results addObject:[JXTrieResult resultWithWord:(NSString *)node.word 
											andDistance:currentRow[currentRowLastIndex]]];*/
		judy_key(judy, key_buffer, key_buffer_size);
		
		// The distance we calculated is only valid for this word, if the next level down is the end of the word. 
		// If the word continues the current distance doesn’t reflect that and we need to recurse deeper for the correct value.
		if (key_buffer[key_index+1] == '\0') {
			fprintf(out, "('%s', %d)\n", key_buffer, currentRow[currentRowLastIndex]);
		}	
	}
	
	int currentRowMinCost = currentRow[0];
	for (column = 1; column < columns; column++) {
		currentRowMinCost = MIN(currentRowMinCost, currentRow[column]);
	}
	
	// If any entries in the row are less than the maximum cost, then 
	// recursively search each branch of the trie
	if (currentRowMinCost <= maxCost) {
		char nextLetter;
		key_index += 1;
		
		if (key_index < key_buffer_size) {
			do {
				judy_key(judy, key_buffer, key_buffer_size);
				
				if (key_buffer[key_index-1] != thisLetter) {
					break;
				}
				else {
					nextLetter = key_buffer[key_index];
					
					searchRecursive(cell, key_buffer, key_buffer_size, key_index, thisLetter, nextLetter, word, columns, previousRow, currentRow, results, maxCost);
					
					key_buffer[key_index] = nextLetter+1;
					
					cell = judy_strt(judy, key_buffer, key_index+1);
				}
				
			} while (cell);
		}
		
	}
}

void search(const char *word, unsigned int maxCost) {
	int word_length = strlen(word);
	
	// build first row
	int currentRowSize = word_length + 1;
	int currentRow[currentRowSize];
	for (int k = 0; k < currentRowSize; k++) {
		currentRow[k] = k;
	}
	
	int key_buffer_size = word_length + maxCost + 1;
	uchar *key_buffer = calloc(key_buffer_size, sizeof(uchar));
	int key_index = 0;
	
	char letter;
	judyslot *cell;
	cell = judy_strt(judy, NULL, 0);
	judy_key(judy, key_buffer, key_buffer_size);
	if (key_buffer[0] == 0) {
		cell = judy_nxt(judy);
	}
	
	if (cell) {
		// recursively search each branch of the trie
		do {
			judy_key(judy, key_buffer, key_buffer_size);
			letter = key_buffer[key_index];
			
			searchRecursive(cell, key_buffer, key_buffer_size, key_index, 0, letter, word, word_length+1, NULL, currentRow, NULL, maxCost);
			
			key_buffer[key_index] = letter+1;
			
			cell = judy_strt(judy, key_buffer, key_index+1);

		} while (cell);
	}
	
	free(key_buffer);
}

int main(int argc, char **argv) {
	const char *target;
	unsigned int maxCost;
	const char *dictionary;
	
	if (argc < 3) {
		fprintf(stderr, "usage: %s [<search string> <maximum distance>] [<dictionary path>]\n",
				argv[0]);
		target = TARGET;
		maxCost = MAX_COST;
	}
	else {
		target = argv[1];
		sscanf(argv[2], "%u", &maxCost);
	}
	
	if (argc >= 3) {
		dictionary = argv[3];
	}
	else {
		dictionary = DICTIONARY;
	}
	
	in = fopen(dictionary, "r");
	out = stdout;
	
	if (!in) {
		fprintf(stderr, "unable to open input file\n");
	}
	
	if (!out) {
		fprintf(stderr, "unable to open output file\n");
	}
	

	uchar buff[1024];
	judyslot max = 0;
	uint len;
	
	// CHANGEME: Can we adapt the judy stack size to the input size if it is known?
	judy = judy_open(512);
	
	while ( fgets((char *)buff, sizeof(buff), in) ) {
		len = strlen((const char *)buff);
		if (len) {
			buff[len] = 0;							// Remove '\n'
			len--;
			if ( len && buff[len - 1] == 0x0d ) {	// Detect and remove Windows CR
				buff[len] = 0;
				len--;
			}
			*(judy_cell(judy, buff, len)) += 1;		// count instances of string
			max++;
		}
	}

	fprintf(out, "Read %" PRIjudyvalue " words. \n", max);

#if 1
	search((const char *)target, maxCost);
#else
	judyslot *cell;
	uint idx;

	cell = judy_strt(judy, NULL, 0);
	judy_key(judy, buff, sizeof(buff));
	if (buff[0] == 0) {
		cell = judy_nxt(judy);
	}
	
	if (cell) do {
		judy_key(judy, buff, sizeof(buff));
		for (idx = 0; idx < *cell; idx++) {	// spit out duplicates
			fprintf(out, "%s\n", buff);
		}
	} while ( (cell = judy_nxt(judy)) );
	
	// test deletion all the way to an empty tree
	
	if ( (cell = judy_prv(judy)) ) {
		do {
			max -= *cell;
		} while ( (cell = judy_del(judy)) );
	}
	
#endif
	judy_close(judy);
	return 0;
}