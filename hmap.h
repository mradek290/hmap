
#ifndef HMAP_GUARD_HEADER
#define HMAP_GUARD_HEADER

#include <stdlib.h>
#include <string.h>

#define __hmapDefaultSize 0x10

typedef _Bool (*hmapExtractor)( const void*, unsigned );
typedef _Bool (*hmapComparator)( const void*, const void* );
typedef _Bool (*hmapLambda)( void*, void* );

typedef struct {
    struct __hmapNode* left;
    struct __hmapNode* right;
} __hmapNode;

typedef struct {
    unsigned size;
    unsigned limit;
    void* dataoffset;
    struct __hmapPrebuffer* next;
} __hmapPrebuffer;

typedef struct {

    hmapExtractor bits;
    hmapComparator cmp;
    unsigned element_sz;

    unsigned total;
    unsigned chunk_sz;
    unsigned erased;

    __hmapNode* base_left;
    __hmapNode* base_right;
    __hmapPrebuffer* first;
    __hmapPrebuffer* last;
    __hmapPrebuffer* recycle;

} hmap;

hmap* hmapCreate( unsigned, hmapExtractor, hmapComparator );
_Bool hmapExist( hmap*, const void* );
void* hmapGet( hmap*, const void* );
void* hmapErase( hmap*, const void* );
void  hmapLoop( hmap*, hmapLambda, void* );
void  hmapFree( hmap*, hmapLambda, void* );

unsigned hmapSize( hmap* );
unsigned hmapDepth( hmap* );

#endif
