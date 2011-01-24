/*
 *  judy-levenshtein.c
 *  judy-arrays
 *
 *  Created by Jan on 24.01.11.
 *  Copyright 2011 geheimwerk.de. All rights reserved.
 *
 */

#include <assert.h>

#include "judy-arrays.c"


#ifndef MIN
	#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
		#define MIN(a,b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a < _b) ? _a : _b; })
	#else
		#define MIN(A,B)	((A) < (B) ? (A) : (B))
	#endif
//#else
//	#warning MIN is already defined, MIN(a, b) may not behave as expected.
#endif

typedef struct _search_data_struct {
	void *judy;
	void (*resultCallback)(FILE *out, const char *word, int distance);
	uchar *key_buffer;
	int key_buffer_size;
	const char *word;
	int columns;
	void *results;
	int maxCost;
} search_data_struct;

// Return the minimum of a, b and c
int jxld_smallestInt(int a, int b, int c) {
	int min = a;
	if ( b < min )
		min = b;
	
	if ( c < min )
		min = c;
	
	return min;
}

void processResult(FILE *out, const char *word, int distance) {
	fprintf(out, "('%s', %d)\n", word, distance);
}

// This recursive helper is used by the search function below. 
// It assumes that the previousRow has been filled in already.
void searchRecursive(judyslot *cell, search_data_struct *d, int key_index, char prevLetter, char thisLetter, int *penultimateRow, int *previousRow) {
	
	const char *word = d->word;
	int columns = d->columns;
	
	int currentRowLastIndex = columns - 1;
#if __STDC_VERSION__ >= 199901L
	int currentRow[columns];
#else
	int *currentRow = calloc(columns, sizeof(int));
#endif
	
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
	if (currentRow[currentRowLastIndex] <= d->maxCost && *cell > 0) {
		/*[results addObject:[JXTrieResult resultWithWord:(NSString *)node.word 
		 andDistance:currentRow[currentRowLastIndex]]];*/
		judy_key(d->judy, d->key_buffer, d->key_buffer_size);
		
		// The distance we calculated is only valid for this word, if the next level down is the end of the word. 
		// If the word continues, the current distance doesn’t reflect that and we need to recurse deeper for the correct value.
		if (d->key_buffer[key_index+1] == '\0') {
			d->resultCallback((FILE *)d->results, (const char *)d->key_buffer, currentRow[currentRowLastIndex]);
		}	
	}
	
	int currentRowMinCost = currentRow[0];
	for (column = 1; column < columns; column++) {
		currentRowMinCost = MIN(currentRowMinCost, currentRow[column]);
	}
	
	// If any entries in the row are less than the maximum cost, then 
	// recursively search each branch of the trie
	if (currentRowMinCost <= d->maxCost) {
		char nextLetter;
		key_index += 1;
		
		assert(key_index < d->key_buffer_size);
		
		do {
			judy_key(d->judy, d->key_buffer, d->key_buffer_size);
			
			if (d->key_buffer[key_index-1] != thisLetter) {
				break;
			}
			else {
				nextLetter = d->key_buffer[key_index];
				
				searchRecursive(cell, d, key_index, thisLetter, nextLetter, previousRow, currentRow);
				
				d->key_buffer[key_index] = nextLetter+1;
				
				cell = judy_strt(d->judy, d->key_buffer, key_index+1);
			}
			
		} while (cell);
		
	}
	
#if __STDC_VERSION__ >= 199901L
#else
	free(currentRow);
#endif
	
}

void search(void *judy, const char *word, unsigned int maxCost, void *results, void (*resultCallback)(FILE *out, const char *word, int distance)) {
	int word_length = strlen(word);
	
	// Build first row
	int currentRowSize = word_length + 1;
	
#if __STDC_VERSION__ >= 199901L
	int currentRow[currentRowSize];
#else
	int *currentRow = calloc(currentRowSize, sizeof(int));
#endif
	
	for (int k = 0; k < currentRowSize; k++) {
		currentRow[k] = k;
	}
	
	// Prepare key_buffer
	int key_buffer_size = word_length + maxCost + 1;
	
#if __STDC_VERSION__ >= 199901L
	uchar key_buffer[key_buffer_size];
#else
	uchar *key_buffer = calloc(key_buffer_size, sizeof(uchar));
#endif
	
	// Prepare unchanging data struct
	search_data_struct d;
	d.judy = judy;
	d.resultCallback = resultCallback;
	d.key_buffer = key_buffer;
	d.key_buffer_size = key_buffer_size;
	d.word = word;
	d.columns = word_length+1;
	d.results = results;
	d.maxCost = maxCost;
	
	int key_index = 0;
	
	char letter;
	judyslot *cell;
	cell = judy_strt(judy, NULL, 0);
	judy_key(judy, key_buffer, key_buffer_size);
	if (key_buffer[0] == 0) {
		cell = judy_nxt(judy);
	}
	
	if (cell) {
		// Recursively search each branch of the trie
		do {
			judy_key(judy, key_buffer, key_buffer_size);
			letter = key_buffer[key_index];
			
			searchRecursive(cell, &d, key_index, 0, letter, NULL, currentRow);
			
			key_buffer[key_index] = letter+1;
			
			cell = judy_strt(judy, key_buffer, key_index+1);
			
		} while (cell);
	}
	
#if __STDC_VERSION__ >= 199901L
#else
	free(currentRow);
	free(key_buffer);
#endif
}
