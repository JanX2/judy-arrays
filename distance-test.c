/*
 *  distance-test.c
 *  judy-arrays
 *
 *  Created by Jan on 21.01.11.
 *  Copyright 2011 geheimwerk.de. All rights reserved.
 *
 *  License: same as for judy-arrays.c
 */

#include <stdio.h>
#include <strings.h>

#include "judy-levenshtein.c"


#define DICTIONARY	"/usr/share/dict/words"
#define TARGET		"goober"
#define MAX_COST	1

int main(int argc, char **argv) {
	void *judy;
	FILE *in, *out;

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
		if (len > 1) {								// We only want lines containing more than just the '\n'
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
	search(judy, (const char *)target, maxCost, out, processResult);
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
