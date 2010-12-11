//	Judy arrays	22 NOV 2010
//	Author Karl Malbrain, malbrain@yahoo.com

//	Simplified judy arrays for strings
//	Adapted from the ideas of Douglas Baskins of HP.

//	Map a set of strings to corresponding memory cells (uints).
//	Each cell must be set to a non-zero value by the caller.

//	STANDALONE is defined to compile into a string sorter.

#define STANDALONE

//	functions:
//	judy_open:	open a new judy array returning a judy object.
//	judy_close:	close an open judy array, freeing all memory.
//	judy_data:	allocate data memory within judy array for external use.
//	judy_cell:	insert a string into the judy array, return cell pointer.
//	judy_strt:	retrieve the cell pointer greater than or equal to given key
//	judy_slot:	retrieve the cell pointer, or return NULL for a given key.
//	judy_key:	retrieve the string value for the most recent judy query.
//	judy_nxt:	retrieve the cell pointer for the next string in the array.
//	judy_prv:	retrieve the cell pointer for the prev string in the array.
//	judy_del:	delete the key and cell for the most recent judy query.

#include <stdlib.h>
#ifdef HAVE_MALLOC_H
	#include <malloc.h>
#endif
#include <memory.h>

typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned int uint;

#ifdef STANDALONE
#include <stdio.h>

uint MaxMem = 0;

void judy_abort (char *msg) __attribute__ ((noreturn)); // Tell static analyser that this function will not return
void judy_abort (char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

#if !defined(WIN32)
void vfree (void *what, uint size)
{
	free (what);
}
#elif defined(WIN32)
#include <windows.h>

void *valloc (uint size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
}

void vfree (void *what, uint size)
{
	VirtualFree(what, 0, MEM_RELEASE);
}
#endif
#else
#include "xlink.h"		// this can be whatever file you want, or delete it
#endif

#define JUDY_seg	65536

enum JUDY_types {
	JUDY_radix,		// inner and outer radix fan-out
	JUDY_8,			// linear list nodes of designated size
	JUDY_16,
	JUDY_32,
	JUDY_64,
	JUDY_128,
	JUDY_256,
	JUDY_span,		// up to 28 tail bytes of key contiguously stored
};

typedef struct {
	void *seg;			// next used allocator
	uint next;			// next available offset
} JudySeg;

typedef struct {
	uint next;			// judy object
	ushort off;			// offset within key
	ushort slot;		// slot within object
} JudyStack;

typedef struct {
	uint root[1];		// root of judy array
	JudySeg *seg;		// current judy allocator
	void **judy8;		// reuse judy8 blocks
	void **judy16;		// reuse judy16 blocks
	void **judy32;		// reuse judy32 blocks
	void **judy64;		// reuse judy64 blocks
	void **judy128;		// reuse judy128 blocks
	void **judy256;		// reuse judy256 blocks
	void **judyradix;	// reuse judyradix blocks
	uint level;			// current height of stack
	uint max;			// max height of stack
	JudyStack stack[1];	// current cursor
} Judy;

#define MAX_judy	256
#define JUDY_max	JUDY_256

//	open judy object

void *judy_open (uint max)
{
JudySeg *seg;
Judy *judy;
uint amt;

	if( ((seg = valloc(JUDY_seg))) )
		seg->next = JUDY_seg;
	else
		judy_abort ("No virtual memory");

	amt = sizeof(Judy) + max * sizeof(JudyStack);

	if( amt & 0x7 )
		amt |= 0x7, amt++;

	seg->next -= amt;
	judy = (Judy *)((uchar *)seg + seg->next);
	memset(judy, 0, amt);
 	judy->seg = seg;
	judy->max = max;
	return judy;
}

void judy_close (Judy *judy)
{
JudySeg *seg, *nxt = judy->seg;

	while( (seg = nxt) )
		nxt = seg->seg, vfree (seg, JUDY_seg);
}

//	allocate judy node

void *judy_alloc (Judy *judy, int type)
{
uint amt = 0;
void **block;
JudySeg *seg;

	switch( type ) {
	case JUDY_8:
		amt = 8;

		if( (block = judy->judy8) ) {
			judy->judy8 = *block;
			memset (block, 0, amt);
			return (void *)block;
		}
		break;

	case JUDY_16:
		amt = 16;

		if( (block = judy->judy16) ) {
			judy->judy16 = *block;
			memset (block, 0, amt);
			return (void *)block;
		}
		break;

	case JUDY_span:
	case JUDY_32:
		amt = 32;

		if( (block = judy->judy32) ) {
			judy->judy32 = *block;
			memset (block, 0, amt);
			return (void *)block;
		}
		break;

	case JUDY_64:
		amt = 64;

		if( (block = judy->judy64) ) {
			judy->judy64 = *block;
			memset (block, 0, amt);
			return (void *)block;
		}
		break;

	case JUDY_128:
		amt = 128;

		if( (block = judy->judy128) ) {
			judy->judy128 = *block;
			memset (block, 0, amt);
			return (void *)block;
		}
		break;

	case JUDY_256:
		amt = 256;

		if( (block = judy->judy256) ) {
			judy->judy256 = *block;
			memset (block, 0, amt);
			return (void *)block;
		}
		break;

	case JUDY_radix:
		amt = 16 * sizeof(uint *);

		if( (block = judy->judyradix) ) {
			judy->judyradix = *block;
			memset (block, 0, amt);
			return (void *)block;
		}
		break;
	}

	if( !judy->seg || judy->seg->next < amt + sizeof(*seg) ) {
		if( (seg = valloc (JUDY_seg)) )
			seg->next = JUDY_seg, seg->seg = judy->seg, judy->seg = seg;
		else
			judy_abort("Out of virtual memory");
#ifdef STANDALONE
		MaxMem += JUDY_seg;
#endif
	}

	judy->seg->next -= amt;

	block = (void **)((uchar *)judy->seg + judy->seg->next);
	memset (block, 0, amt);
	return (void *)block;
}

void *judy_data (Judy *judy, uint amt)
{
JudySeg *seg;
void *block;

	if( amt & 0x7 )
		amt |= 0x7, amt += 1;

	if( !judy->seg || judy->seg->next < amt + sizeof(*seg) ) {
		if( (seg = valloc (JUDY_seg)) )
			seg->next = JUDY_seg, seg->seg = judy->seg, judy->seg = seg;
		else
			judy_abort("Out of virtual memory");
#ifdef STANDALONE
		MaxMem += JUDY_seg;
#endif
	}

	judy->seg->next -= amt;

	block = (void *)((uchar *)judy->seg + judy->seg->next);
	memset (block, 0, amt);
	return block;
}
void judy_free (Judy *judy, void *block, int type)
{
	switch( type ) {
	case JUDY_8:
		*((void **)(block)) = judy->judy8;
		judy->judy8 = (void **)block;
		return;
	case JUDY_16:
		*((void **)(block)) = judy->judy16;
		judy->judy16 = (void **)block;
		return;
	case JUDY_span:
	case JUDY_32:
		*((void **)(block)) = judy->judy32;
		judy->judy32 = (void **)block;
		return;
	case JUDY_64:
		*((void **)(block)) = judy->judy64;
		judy->judy64 = (void **)block;
		return;
	case JUDY_128:
		*((void **)(block)) = judy->judy128;
		judy->judy128 = (void **)block;
		return;
	case JUDY_256:
		*((void **)(block)) = judy->judy256;
		judy->judy256 = (void **)block;
		return;
	case JUDY_radix:
		*((void **)(block)) = judy->judyradix;
		judy->judyradix = (void **)block;
		return;
	}
}
		
//	assemble key from current path

uint judy_key (Judy *judy, uchar *buff, uint max)
{
int slot, cnt, size, off;
uint len = 0, idx = 0;
uchar *base;
int keysize;

	// leave room for zero terminator

	max--;

	while( len < max && ++idx <= judy->level ) {
		slot = judy->stack[idx].slot;
		size = 0;
		switch( judy->stack[idx].next & 0x7 ) {
		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			keysize = 4 - (judy->stack[idx].off & 3);
			base = (uchar *)(judy->stack[idx].next & 0xfffffff8);
			//cnt = size / (sizeof(uint) + keysize);
			off = keysize;
			while( off-- && len < max )
				buff[len++] = base[slot * keysize + off];
			continue;
		case JUDY_radix:
			if( !slot )
				break;
			buff[len++] = slot;
			continue;
		case JUDY_span:
			base = (uchar *)(judy->stack[idx].next & 0xfffffff8);
			cnt = 32 - sizeof(uint);

			for( slot = 0; slot < cnt && base[slot]; slot++ )
			  if( len < max )
				buff[len++] = base[slot];
			continue;
		}
	}
	buff[len] = 0;
	return len;
}

//	find slot & setup cursor

uint *judy_slot (Judy *judy, uchar *buff, uint max)
{
int slot, size, keysize;
uint next = *judy->root;
uint value, prev, test;
uint off = 0/*, start*/;
ushort *judyushort;
uchar *judyuchar;
uint *judyuint;
uchar *base;
uint *table;
uint *node;
int cnt;

	judy->level = 0;

	while( next ) {
		if( judy->level < judy->max )
			judy->level++;

		judy->stack[judy->level].off = off;
		judy->stack[judy->level].next = next;
		size = 0;

		switch( next & 0x7 ) {

		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			keysize = 4 - (off & 3);
			node = (uint *)((next & 0xfffffff8) + size);
			cnt = size / (sizeof(uint) + keysize);
			//start = off;
			value = 0;
			prev = 0;

			do {
				value <<= 8;
				if( off < max )
					value |= buff[off];
			} while( ++off & 0x3 );

			//  find slot > key

			switch( keysize ) {
			case 4:			// 4 byte keys
				judyuint = (uint *)(next & 0xfffffff8);
				for( slot = 0; slot < cnt; slot++ )
					if( judyuint[slot] > value )
						break;
					else
						prev = judyuint[slot];
				break;

			case 3:			// 3 byte keys
				judyuchar = (uchar *)(next & 0xfffffff8);
				for( slot = 0; slot < cnt; slot++ ) {
					test = *judyuchar++;
					test |= *judyuchar++ << 8;
					test |= *judyuchar++ << 16;
					if( test > value )
						break;
					else
						prev = test;
				}
				break;

			case 2:			// 2 byte keys
				judyushort = (ushort *)(next & 0xfffffff8);
				for( slot = 0; slot < cnt; slot++ )
					if( judyushort[slot] > value )
						break;
					else
						prev = judyushort[slot];
				break;

			case 1:			// 1 byte keys
				judyuchar = (uchar *)(next & 0xfffffff8);
				for( slot = 0; slot < cnt; slot++ )
					if( judyuchar[slot] > value )
						break;
					else
						prev = judyuchar[slot];
				break;
			}

			if( slot && prev == value ) {
				judy->stack[judy->level].slot = slot - 1;

				// is this a leaf?

				if( !(value & 0xff) )
					return &node[-slot];

				next = node[-slot];
				continue;
			}

			return 0;

		case JUDY_radix:
			table = (uint  *)(next & 0xfffffff8); // outer radix

			if( off < max )
				slot = buff[off];
			else
				slot = 0;

			//	put radix slot on judy stack

			judy->stack[judy->level].slot = slot;

			if( (next = table[slot >> 4]) )
				table = (uint  *)(next & 0xfffffff8); // inner radix
			else
				return 0;

			if( !slot )	// leaf?
				return &table[slot & 0xf];

			next = table[slot & 0xf];
			off += 1;
			break;

		case JUDY_span:
			node = (uint *)((next & 0xfffffff8) + 32);
			base = (uchar *)(next & 0xfffffff8);
			cnt = test = 32 - sizeof(uint);
			if( test > max - off )
				test = max - off;
			value = strncmp((const char *)base, (const char *)(buff + off), test);
				
			if( !value && test < cnt && !base[test] ) // leaf?
				return &node[-1];

			if( !value && test == cnt ) {
				next = node[-1];
				off += cnt;
				continue;
			}
			return 0;
		}
	}

	return 0;
}

//	promote full nodes to next larger size

uint *judy_promote (Judy *judy, uint *next, int idx, uint value, int keysize, int size)
{
uint *node = (uint *)((*next & 0xfffffff8) + size);
uchar *base = (uchar *)(*next & 0xfffffff8);
int oldcnt, newcnt, slot;
uchar *newbase;
uint *newnode;
uint *result;
uint type;

	oldcnt = size / (sizeof(uint) + keysize);
	newcnt = (2 * size) / (sizeof(uint) + keysize);

	// promote node to next larger size

	type = (*next & 0x7) + 1;
	newbase = judy_alloc (judy, type);
	newnode = (uint *)(newbase + size * 2);
	*next = (uint)newbase | type;

	//	open up slot at idx

	memcpy(newbase + (newcnt - oldcnt - 1) * keysize, base, idx * keysize);	// copy keys

	for( slot = 0; slot < idx; slot++ )
		newnode[-(slot + newcnt - oldcnt)] = node[-(slot + 1)];	// copy ptr

	//	fill in new node

	memcpy(newbase + (idx + newcnt - oldcnt - 1) * keysize, &value, keysize);	// copy key
	result = &newnode[-(idx + newcnt - oldcnt)];

	//	copy rest of old node

	memcpy(newbase + (idx + newcnt - oldcnt) * keysize, base + (idx * keysize), (oldcnt - slot) * keysize);	// copy keys

	for( ; slot < oldcnt; slot++ )
		newnode[-(slot + newcnt - oldcnt + 1)] = node[-(slot + 1)];	// copy ptr

	judy->stack[judy->level].next = *next;
	judy->stack[judy->level].slot = idx + newcnt - oldcnt - 1;
	judy_free (judy, (void **)base, type - 1);
	return result;
}

//	construct new node for JUDY_radix entry
//	make node with slot - start entries
//	moving key over one offset

void judy_radix (Judy *judy, uint *radix, uchar *old, int start, int slot, int keysize, uchar key)
{
int cnt = slot - start, newcnt;
uint type = JUDY_8 - 1;
uint *node, *oldnode;
int idx, size = 4;
uchar *base;
uint *table;

	//	if necessary, setup inner radix node

	if( (!(table = (uint *)(radix[key >> 4] & 0xfffffff8))) ) {
		table = judy_alloc (judy, JUDY_radix);
		radix[key >> 4] = (uint)table | JUDY_radix;
	}

	oldnode = (uint *)(old + MAX_judy);

	// is this slot a leaf?

	if( !key || !keysize ) {
		table[key & 0xf] = oldnode[-start-1];
		return;
	}

	//	calculate new node big enough to contain slots

	do {
		type++;
		size <<= 1;
		newcnt = size / (sizeof(uint) + keysize);
	} while( cnt > newcnt && size < MAX_judy );

	//	store new node pointer in inner table

	base = judy_alloc (judy, type);
	node = (uint *)(base + size);
	table[key & 0xf] = (uint)base | type;

	//	allocate node and copy old contents
	//	shorten keys by 1 byte during copy

	for( idx = 0; idx < cnt; idx++ ) {
		memcpy (base + (newcnt - idx - 1) * keysize, old + (start + cnt - idx - 1) * (keysize + 1), keysize);
		node[-(newcnt - idx)] = oldnode[-(start + cnt - idx)];
	}
}
			
//	decompose full node to radix nodes

void judy_splitnode (Judy *judy, uint *next, uint size, uint keysize, uint type)
{
int cnt, slot, start = 0;
uint key = 0x100, nxt;
uint *newradix;
uchar *base;
//uint *node;

	base = (uchar  *)(*next & 0xfffffff8);
	cnt = size / (sizeof(uint) + keysize);
	//node = (uint *)(base + size);

	//	allocate outer judy_radix node

	newradix = judy_alloc (judy, JUDY_radix);
	*next = (uint)newradix | JUDY_radix;

	for( slot = 0; slot < cnt; slot++ ) {
		nxt = base[slot * keysize + keysize - 1];

		if( key > 0xff )
			key = nxt;
		if( nxt == key )
			continue;

		//	decompose portion of old node into radix nodes

		judy_radix (judy, newradix, base, start, slot, keysize - 1, key);
		start = slot;
		key = nxt;
	}

	judy_radix (judy, newradix, base, start, slot, keysize - 1, key);
	judy_free (judy, (void **)base, type);
}

//	return first leaf

uint *judy_first (Judy *judy, uint next, uint off)
{
uint *table, *inner;
uint keysize, size;
int slot, cnt;
uchar *base;
uint *node;

	while( next ) {
		if( judy->level < judy->max )
			judy->level++;

		judy->stack[judy->level].off = off;
		judy->stack[judy->level].next = next;
		size = 0;
		switch( next & 0x7 ) {
		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			keysize = 4 - (off & 3);
			node = (uint *)((next & 0xfffffff8) + size);
			base = (uchar *)(next & 0xfffffff8);
			cnt = size / (sizeof(uint) + keysize);

			for( slot = 0; slot < cnt; slot++ )
				if( node[-slot-1] )
					break;

			judy->stack[judy->level].slot = slot;
			if( !base[slot * keysize] )
				return &node[-slot-1];

			next = node[-slot - 1];
			off = (off | 3) + 1;
			continue;
		case JUDY_radix:
			table = (uint *)(next & 0xfffffff8);
			for( slot = 0; slot < 256; slot++ )
			  if( (inner = (uint *)(table[slot >> 4] & 0xfffffff8)) ) {
				if( (next = inner[slot & 0xf]) ) {
				  judy->stack[judy->level].slot = slot;
				  if( !slot )
					return &inner[slot & 0xf];
				  else
					break;
				}
			  } else
				slot |= 0xf;
			off++;
			continue;
		case JUDY_span:
			node = (uint *)((next & 0xfffffff8) + 32);
			base = (uchar *)(next & 0xfffffff8);
			cnt = 32 - sizeof(uint);	// number of bytes
			if( !base[cnt - 1] )	// leaf node?
				return &node[-1];
			next = node[-1];
			off += cnt;
			continue;
		}
	}
	return 0;
}

//	return last leaf cell pointer

uint *judy_last (Judy *judy, uint next, uint off)
{
uint *table, *inner;
uint keysize, size;
int slot, cnt;
uchar *base;
uint *node;

	while( next ) {
		if( judy->level < judy->max )
			judy->level++;

		judy->stack[judy->level].off = off;
		judy->stack[judy->level].next = next;
		size = 0;
		switch( next & 0x7 ) {
		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			keysize = 4 - (off & 3);
			slot = size / (sizeof(uint) + keysize);
			base = (uchar *)(next & 0xfffffff8);
			node = (uint *)((next & 0xfffffff8) + size);
			if( !base[--slot * keysize] )
				return &node[-slot-1];
			next = node[-slot-1];
			off += keysize;
			continue;
		case JUDY_radix:
			table = (uint *)(next & 0xfffffff8);
			for( slot = 256; slot--; ) {
			  if( (inner = (uint *)(table[slot >> 4] & 0xfffffff8)) ) {
				if( (next = inner[slot & 0xf]) )
				  if( !slot )
					return &inner[0];
				  else
					break;
			  } else
				slot &= 0xf0;
			}
			off++;
			continue;
		case JUDY_span:
			node = (uint *)((next & 0xfffffff8) + 32);
			base = (uchar *)(next & 0xfffffff8);
			cnt = 32 - sizeof(uint);	// number of bytes
			if( !base[cnt - 1] )	// leaf node?
				return &node[-1];
			next = node[-1];
			off += cnt;
			continue;
		}
	}
	return 0;
}

//	judy_nxt: return next entry

uint *judy_nxt (Judy *judy)
{
int slot, size, cnt;
uint *table, *inner;
//uint idx = 0;
uint keysize;
uchar *base;
uint *node;
uint next;
uint off;

	while( judy->level ) {
		next = judy->stack[judy->level].next;
		slot = judy->stack[judy->level].slot;
		off = judy->stack[judy->level].off;
		keysize = 4 - (off & 3);
		size = 0;

		switch( next & 0x7 ) {
		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			cnt = size / (sizeof(uint) + keysize);
			node = (uint *)((next & 0xfffffff8) + size);
			base = (uchar *)(next & 0xfffffff8);
			if( ++slot < cnt )
				if( !base[slot * keysize] ) {
					judy->stack[judy->level].slot = slot;
					return &node[-slot - 1];
				} else {
					judy->stack[judy->level].slot = slot;
					return judy_first (judy, node[-slot-1], (off | 3) + 1);
				}
			judy->level--;
			continue;

		case JUDY_radix:
			table = (uint *)(next & 0xfffffff8);

			while( ++slot < 256 )
			  if( (inner = (uint *)(table[slot >> 4] & 0xfffffff8)) ) {
				if( inner[slot & 0xf] ) {
				  judy->stack[judy->level].slot = slot;
				  return judy_first(judy, inner[slot & 0xf], off + 1);
				}
			  } else
				slot |= 0xf;

			judy->level--;
			continue;
		case JUDY_span:
			judy->level--;
			continue;
		}
	}
	return 0;
}

//	judy_prv: return ptr to previous entry

uint *judy_prv (Judy *judy)
{
int slot, size, keysize;
uint *table, *inner;
//uint idx = 0;
uchar *base;
uint *node;
uint next;
uint off;

	while( judy->level ) {
		next = judy->stack[judy->level].next;
		slot = judy->stack[judy->level].slot;
		off = judy->stack[judy->level--].off;
		size = 0;

		switch( next & 0x7 ) {
		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			node = (uint *)((next & 0xfffffff8) + size);
			if( !slot || !node[-slot] )
				continue;

			base = (uchar *)(next & 0xfffffff8);
			keysize = 4 - (off & 3);

			if( base[(slot - 1) * keysize] )
				return judy_last (judy, node[-slot], (off | 3) + 1);

			return &node[-slot];

		case JUDY_radix:
			table = (uint *)(next & 0xfffffff8);

			while( slot-- )
			  if( (inner = (uint *)(table[slot >> 4] & 0xfffffff8)) )
				if( inner[slot & 0xf] )
				  if( slot )
				    return judy_last(judy, inner[slot & 0xf], off + 1);
				  else
					return &inner[0];

			continue;

		case JUDY_span:
			continue;
		}
	}
	return 0;
}

//	judy_del: delete string from judy array

void judy_del (Judy *judy)
{
int slot, off, size, type;
uint *table, *inner;
uint next, *node;
int keysize, cnt;
uchar *base;

	while( judy->level ) {
		next = judy->stack[judy->level].next;
		slot = judy->stack[judy->level].slot;
		off = judy->stack[judy->level--].off;
		size = 0;

		switch( type = next & 0x7 ) {
		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			keysize = 4 - (off & 3);
			cnt = size / (sizeof(uint) + keysize);
			node = (uint *)((next & 0xfffffff8) + size);
			base = (uchar *)(next & 0xfffffff8);
			while( slot ) {
				node[-slot-1] = node[-slot];
				memcpy (base + slot * keysize, base + (slot - 1) * keysize, keysize);
				slot--;
			}
			node[-1] = 0;
			memset (base, 0, keysize);
			if( node[-cnt] )
				return;
			judy_free (judy, base, type);
			continue;

		case JUDY_radix:
			table = (uint  *)(next & 0xfffffff8);
			inner = (uint *)(table[slot >> 4] & 0xfffffff8);
			inner[slot & 0xf] = 0;

			for( cnt = 0; cnt < 16; cnt++ )
				if( inner[cnt] )
					return;

			judy_free (judy, inner, JUDY_radix);
			table[slot >> 4] = 0;

			for( cnt = 0; cnt < 16; cnt++ )
				if( table[cnt] )
					return;

			judy_free (judy, table, JUDY_radix);
			continue;

		case JUDY_span:
			base = (uchar *)(next & 0xfffffff8);
			judy_free (judy, base, type);
			continue;
		}
	}

	//	tree is now empty

	*judy->root = 0;
}

//	return first key greater than or equal to given key

uint *judy_strt (Judy *judy, uchar *buff, uint max)
{
uint *cell;

	if( (cell = judy_slot (judy, buff, max)) )
		return cell;

	return judy_nxt (judy);
}

//	split open span node

void judy_splitspan (Judy *judy, uint *next, uchar *base)
{
uint cnt = 32 - sizeof(uint);	// number of bytes
uint *node = (uint *)(base + 32);
uchar *newbase;
uint off = 0;

	do {
		newbase = judy_alloc (judy, JUDY_8);
		*next = (uint)newbase | JUDY_8;

		*newbase++ = base[off + 3];
		*newbase++ = base[off + 2];
		*newbase++ = base[off + 1];
		*newbase++ = base[off + 0];
		next = (uint *)newbase;

		off += 4;
		cnt -= 4;
	} while( cnt && base[off - 1] );

	*next = node[-1];
	judy_free (judy, base, JUDY_span);
}

//	judy_cell: add string to judy array

uint *judy_cell (Judy *judy, uchar *buff, uint max)
{
int size, idx, slot, cnt;
uchar *base, *judyuchar;
uint *next = judy->root;
uint test, prev, value;
uint off = 0, start;
ushort *judyushort;
uint *judyuint;
uint keysize;
uint *table;
uint *node;

	while( *next ) {
		size = 0;

		switch( *next & 0x7 ) {

		case JUDY_256:
			size += 128;
		case JUDY_128:
			size += 64;
		case JUDY_64:
			size += 32;
		case JUDY_32:
			size += 16;
		case JUDY_16:
			size += 8;
		case JUDY_8:
			size += 8;
			keysize = 4 - (off & 3);
			cnt = size / (sizeof(uint) + keysize);
			base = (uchar *)(*next & 0xfffffff8);
			node = (uint *)((*next & 0xfffffff8) + size);
			start = off;
			value = 0;
			prev = 0;

			do {
				value <<= 8;
				if( off < max )
					value |= buff[off];
			} while( ++off & 0x3 );

			//  find slot > key

			switch( keysize ) {
			case 4:			// 4 byte keys
				judyuint = (uint *)base;
				for( slot = 0; slot < cnt; slot++ )
					if( judyuint[slot] > value )
						break;
					else
						prev = judyuint[slot];

				break;

			case 3:			// 3 byte keys
				judyuchar = (uchar *)base;
				for( slot = 0; slot < cnt; slot++ ) {
					test = *judyuchar++;
					test |= *judyuchar++ << 8;
					test |= *judyuchar++ << 16;
					if( test > value )
						break;
					else
						prev = test;
				}

				break;

			case 2:			// 2 byte keys
				judyushort = (ushort *)base;
				for( slot = 0; slot < cnt; slot++ )
					if( judyushort[slot] > value )
						break;
					else
						prev = judyushort[slot];
				break;

			case 1:			// 1 byte keys
				judyuchar = base;
				for( slot = 0; slot < cnt; slot++ )
					if( judyuchar[slot] > value )
						break;
					else
						prev = judyuchar[slot];
				break;
			}

			if( slot && prev == value ) {
				next = &node[-slot];

				// is this a leaf?

				if( !(value & 0xff) )
					return next;

				continue;
			}

			//	if this node is not full
			//	open up slot

			if( !node[-1] ) {
		 	  memmove(base, base + keysize, (slot - 1) * keysize);	// copy keys
			  memcpy(base + (slot - 1) * keysize, &value , keysize);	// copy new key
			  for( idx = 1; idx < slot; idx++ )
				node[-idx] = node[-idx - 1];	// copy ptr

			  node[-slot] = 0;

			  if( !(value & 0xff) )
			  	return &node[-slot];

			  next = &node[-slot];
			  continue;
			}

			if( size < MAX_judy ) {
			  next = judy_promote (judy, next, slot, value, keysize, size);

			  if( !(value & 0xff) )
				return next;

			  continue;
			}

			//	split full node into JUDY_radix nodes
			//  loop to reprocess

			judy_splitnode (judy, next, size, keysize, JUDY_max);
			off = start;
			continue;
		
		case JUDY_radix:
			table = (uint  *)(*next & 0xfffffff8); // outer radix

			if( off < max )
				slot = buff[off++];
			else
				slot = 0;

			// allocate inner radix if empty

			if( !table[slot >> 4] )
				table[slot >> 4] = (uint)judy_alloc (judy, JUDY_radix) | JUDY_radix;

			table = (uint *)(table[slot >> 4] & 0xfffffff8);
			next = &table[slot & 0xf];

			if( !slot ) // leaf?
				return next;
			continue;

		case JUDY_span:
			base = (uchar *)(*next & 0xfffffff8);
			node = (uint *)((*next & 0xfffffff8) + 32);
			cnt = 32 - sizeof(uint);	// number of bytes
			test = cnt;

			if( test > max - off )
				test = max - off;

			value = strncmp((const char *)base, (const char *)(buff + off), test);

			if( !value && test < cnt && !base[test] ) // leaf?
				return &node[-1];

			if( !value && test == cnt ) {
				next = &node[-1];
				off += cnt;
				continue;
			}

			//	bust up JUDY_span node and produce JUDY_8 nodes
			//	then loop to reprocess insert

			judy_splitspan (judy, next, base);
			continue;
		}
	}

	// place JUDY_8 node under JUDY_radix node(s)

	if( off & 3 && off <= max ) {
		base = judy_alloc (judy, JUDY_8);
		keysize = 4 - (off & 3);
		node = (uint  *)(base + 8);
		*next = (uint)base | JUDY_8;

		//	fill in slot 0 with bytes of key

		while( keysize )
			if( off + --keysize < max )
				*base++ = buff[off + keysize];
			else
				base++;
		next = &node[-1];
		off |= 3;
		off++;
	}

	//	produce span nodes to consume rest of key

	while( off <= max ) {
		base = judy_alloc (judy, JUDY_span);
		*next = (uint)base | JUDY_span;
		node = (uint  *)(base + 32);
		cnt = test = 32 - sizeof(uint);	// maximum bytes
		if( test > max - off )
			test = max - off;
		memcpy (base, buff + off, test);
		next = &node[-1];
		off += test;
		if( !base[cnt-1] )	// done on leaf
			break;
	}
	return next;
}

#ifdef STANDALONE
int main (int argc, char **argv)
{
uchar buff[1024];
FILE *in, *out;
//uint max = 0;
void *judy;
uint *cell;
uint len;
uint idx;

	if( argc > 1 )
		in = fopen (argv[1], "r");
	else
		in = stdin;

	if( argc > 2 )
		out = fopen (argv[2], "w");
	else
		out = stdout;

	if( !in )
		fprintf (stderr, "unable to open input file\n");

	if( !out )
		fprintf (stderr, "unable to open output file\n");

	judy = judy_open (512);

	while( fgets((char *)buff, sizeof(buff), in) ) {
		len = strlen((const char *)buff);
		buff[--len] = 0;
		if( len && buff[len - 1] == 0xd ) // Detect and remove Windows CR
			buff[--len] = 0;
		*judy_cell (judy, buff, len) += 1;		// count instances of string
	}

	cell = judy_strt (judy, NULL, 0);

	do {
		/*len = */judy_key(judy, buff, sizeof(buff));
		for( idx = 0; idx < *cell; idx++ )		// spit out duplicates
			fprintf(out, "%s\n", buff);
	} while( (cell = judy_nxt (judy)) );

	fprintf(stderr, "%d memory used\n", MaxMem);

#if 0
	// test deletion to empty tree

	rewind (in);

	while( fgets(buff, sizeof(buff), in) ) {
		len = strlen(buff);
		buff[--len] = 0;
		if( len && buff[len - 1] == 0xd )
			buff[--len] = 0;
		if( judy_slot (judy, buff, len) )
			judy_del (judy);
	}
#endif
	judy_close(judy);
	return 0;
}
#endif
