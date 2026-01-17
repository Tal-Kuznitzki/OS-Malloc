#ifndef __CUSTOM_ALLOCATOR__
#define __CUSTOM_ALLOCATOR__

/*=============================================================================
* do no edit lines below!
=============================================================================*/
#include <stddef.h> //for size_t
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
//Part A - single thread memory allocator
void* customMalloc(size_t size);
void customFree(void* ptr);
void* customCalloc(size_t nmemb, size_t size);
void* customRealloc(void* ptr, size_t size);

//Part B - multi thread memory allocator
void* customMTMalloc(size_t size);
void customMTFree(void* ptr);
void* customMTCalloc(size_t nmemb, size_t size);
void* customMTRealloc(void* ptr, size_t size);

// Part B - helper functions for multi thread memory allocator
void heapCreate();
void heapKill();
/*=============================================================================
* do no edit lines above!
=============================================================================*/

/*=============================================================================
* defines
=============================================================================*/
#define SBRK_FAIL (void*)(-1)
#define ALIGN_TO_MULT_OF_4(x) (((((x) - 1) >> 2) << 2) + 4)

/*=============================================================================
* Block
=============================================================================*/
//suggestion for block usage - feel free to change this
typedef struct Block{
    size_t size;
    struct Block* next;
    bool free;
} Block;

typedef struct{
    char*  startOfZone;
    pthread_mutex_t zoneLock;
    size_t remainingSpace;
    Block* zoneBlockList;
    struct memZone* next;
} memZone;

extern Block* blockList;

void initZoneMT(int index);
Block* findBestFit(size_t size);
Block* findBestFitInZoneMT(memZone* zone, size_t size);
Block* requestSpace(Block* last, size_t size);
Block* getBlock(void* ptr);
Block* getAndValidateBlockReturnPrev(void* ptr);
Block* getAndValidateBlockReturnPrevMT(void* ptr, Block* blockListInSpecificZone );
memZone* create_new_zone();


#endif // CUSTOM_ALLOCATOR
