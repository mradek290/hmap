
#ifndef HMAP_GUARD_IMPL
#define HMAP_GUARD_IMPL

#include "hmap.h"

unsigned __hmapGrowth( unsigned x ){
    return 1 + 3*x/2;
}

void* __hmapNodeData( __hmapNode* v ){
    return ((_Bool*)v) + sizeof(__hmapNode);
}

__hmapNode* __hmapAccessNode( __hmapPrebuffer* pb, unsigned n, unsigned esz ){
    
    void* adr = ((_Bool*)pb->dataoffset) + n * ( sizeof(__hmapNode) + esz );  
    return (__hmapNode*)adr;
}

__hmapNode** __hmapAccessRecycle( __hmapPrebuffer* pb, unsigned n ){

    void* adr = ((_Bool*)pb->dataoffset) + n * sizeof(void*);
    return (__hmapNode**)adr;
}

__hmapPrebuffer* __hmapAllocPrebuffer( unsigned n, unsigned esz ){

    unsigned node_sz = sizeof(__hmapNode) + esz;
    unsigned extra = 1 + sizeof(__hmapPrebuffer)/node_sz;
    unsigned mem_sz = (extra+n) * node_sz;

    __hmapPrebuffer* buffer = (__hmapPrebuffer*) malloc(mem_sz);

    buffer->size = 0;
    buffer->limit = n;
    buffer->dataoffset = ((_Bool*)buffer) + extra * node_sz;
    buffer->next = 0;
    memset( buffer->dataoffset, 0, n * node_sz );

    return buffer;
}

__hmapPrebuffer* __hmapAllocRecycleBuffer( unsigned n, __hmapPrebuffer* sc ){

    unsigned extra = 1 + sizeof(__hmapPrebuffer)/sizeof(void*);
    unsigned mem_sz = (extra+n) * sizeof(void*);

    __hmapPrebuffer* buffer = (__hmapPrebuffer*) malloc(mem_sz);
    buffer->size = 0;
    buffer->limit = n;
    buffer->dataoffset = ((_Bool*)buffer) + extra * sizeof(void*);
    buffer->next = (struct __hmapPrebuffer*) sc;

    return buffer;
}

unsigned __hmapQdepth( __hmapNode* a, __hmapNode* b ){
    unsigned da = a ? 1 + __hmapQdepth( (__hmapNode*) a->left, (__hmapNode*) a->right) : 0;
    unsigned db = b ? 1 + __hmapQdepth( (__hmapNode*) b->left, (__hmapNode*) b->right) : 0;
    return da > db ? da : db;
}

void __hmapRecycleNode( hmap* map, __hmapNode* nd ){

    nd->left = (struct __hmapNode*) nd;
    //nd->right = nd;
    map->erased++;

    __hmapPrebuffer* buffer = map->recycle;
    if( buffer->size == buffer->limit ){

        unsigned new_sz = __hmapGrowth(buffer->limit);
        buffer = __hmapAllocRecycleBuffer(new_sz,buffer);
        map->recycle = buffer;
    }

    __hmapNode** slot = __hmapAccessRecycle(buffer, buffer->size++);
    *slot = nd;
}

//------------------------------------------------

unsigned hmapDepth( hmap* map ){
    return __hmapQdepth(map->base_left,map->base_right);
}

unsigned hmapSize( hmap* map ){
    return map->total - map->erased;
}

hmap* hmapCreate( unsigned elmnt_sz, hmapExtractor ext, hmapComparator cmptr ){

    hmap* map = (hmap*) malloc( sizeof(hmap) );
    map->bits = ext;
    map->cmp = cmptr;
    map->element_sz = elmnt_sz;
    map->total = 0;
    map->chunk_sz = __hmapDefaultSize;
    map->erased = 0;
    map->base_left = 0;
    map->base_right = 0;
    map->first = __hmapAllocPrebuffer( __hmapDefaultSize, elmnt_sz );
    map->last = map->first;
    map->recycle = __hmapAllocRecycleBuffer(__hmapDefaultSize,0);

    return map;
}

_Bool hmapExist( hmap* map, const void* obj ){

    unsigned depth = 1;
    __hmapNode** current = map->bits(obj,0) ?
        &(map->base_left) :
        &(map->base_right);

    while( *current ){
        if( map->cmp( __hmapNodeData(*current), obj ) ){
            return 1;
        }else{
            current = map->bits( obj, depth++ ) ?
                (__hmapNode**) &((**current).left) :
                (__hmapNode**) &((**current).right);
        }
    }

    return 0;
}

void* hmapGet( hmap* map, const void* obj ){

    unsigned depth = 1;
    __hmapNode** current = map->bits(obj,0) ?
        &(map->base_left) :
        &(map->base_right);

    while(1){
        if( *current ){

            void* data = __hmapNodeData(*current);
            if( map->cmp(data,obj) ){
                return data;
            }else{
                current = map->bits( obj, depth++ ) ?
                    (__hmapNode**) &((**current).left) :
                    (__hmapNode**) &((**current).right);
            }

        }else{

            __hmapPrebuffer* rec = map->recycle;
            if( rec->size > 0 ){
                
                __hmapNode* new_node = *(__hmapAccessRecycle(rec, --(rec->size)));
                new_node->left = 0;
                new_node->right = 0;
                *current = new_node;
                map->erased--;

                if( rec->size == 0 && (rec->next) ){
                    map->recycle = (__hmapPrebuffer*) rec->next;
                    free(rec);
                }

                return __hmapNodeData(new_node);

            }else{

                __hmapPrebuffer* buffer = map->last;
                __hmapNode* new_node = __hmapAccessNode(
                    buffer,
                    buffer->size++,
                    map->element_sz
                );

                *current = new_node;
                map->total++;

                if( buffer->size == buffer->limit ){

                    unsigned new_chunk = __hmapGrowth(map->chunk_sz);
                    __hmapPrebuffer* new_buffer = __hmapAllocPrebuffer( new_chunk, map->element_sz );
                    buffer->next = (struct __hmapPrebuffer*) new_buffer;
                    map->last = new_buffer;
                    map->chunk_sz = new_chunk;
                }

                return __hmapNodeData(new_node);
            }
        }
    }
}

void hmapLoop( hmap* map, hmapLambda fn, void* ex ){

    __hmapPrebuffer* b0 = map->first;
    unsigned sz = map->element_sz;
    while( b0 ){

        __hmapPrebuffer* bc = b0;
        b0 = (__hmapPrebuffer*) b0->next;

        for( unsigned i = 0; i < bc->size; ++i ){
            __hmapNode* nd = __hmapAccessNode( bc, i, sz);
            void* data = __hmapNodeData(nd);
            if( nd->left != (struct __hmapNode*) nd ){
                if( !(fn(data,ex)) ) return;
            }
        }
    }
}

void hmapFree( hmap* map, hmapLambda fn, void* ex ){

    if( fn ){

        __hmapPrebuffer* b0 = map->first;
        unsigned sz = map->element_sz;
        while( b0 ){

            __hmapPrebuffer* bc = b0;
            b0 = (__hmapPrebuffer*) b0->next;

            for( unsigned i = 0; i < bc->size; ++i ){
                __hmapNode* nd = __hmapAccessNode( bc, i, sz);
                void* data = __hmapNodeData(nd);
                if( nd->left != (struct __hmapNode*) nd ){
                    if( !(fn(data,ex)) ) return;
                }
            }

            free(bc);
        }

    }else{

        __hmapPrebuffer* b0 = map->first;
        while( b0 ){
            __hmapPrebuffer* bc = b0;
            b0 = (__hmapPrebuffer*) b0->next;
            free(bc);
        }
    }

    __hmapPrebuffer* b0 = map->recycle;
    while( b0 ){
        __hmapPrebuffer* t = b0;
        b0 = (__hmapPrebuffer*) b0->next;
        free(t);
    }

    free(map);
}

void* hmapErase( hmap* map, const void* obj ){

    unsigned depth = 1;
    __hmapNode** uplink = map->bits(obj,0) ?
        &(map->base_left) :
        &(map->base_right);

    while( *uplink ){
        if( map->cmp( __hmapNodeData(*uplink), obj ) ){
            break;
        }else{
            uplink = map->bits( obj, depth++ ) ?
                (__hmapNode**) &((**uplink).left) :
                (__hmapNode**) &((**uplink).right); 
        }
    }

    if( !*uplink ) return 0;
    __hmapNode* target = *uplink;

    if( !target->left && !target->right ){
        *uplink = 0;
        __hmapRecycleNode(map,target);
        return __hmapNodeData(target);
    }

    __hmapNode** leaf;
    if( !target->right ) leaf = (__hmapNode**) &(target->left);
    if( !target->left  ) leaf = (__hmapNode**) &(target->right);

    while(1){
        __hmapNode* nd = *leaf;
        if( !nd->left && !nd->right ) break;
        leaf = nd->left ? 
            (__hmapNode**) &(nd->left) : 
            (__hmapNode**) &(nd->right);
    }

    __hmapNode* substitute = *leaf;
    *uplink = substitute;
    *leaf = 0;

    substitute->left = target->left;
    substitute->right = target->right;

    __hmapRecycleNode( map, target );
    return __hmapNodeData(target);
}

#endif
