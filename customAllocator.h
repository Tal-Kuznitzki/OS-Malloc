#ifndef __CUSTOM_ALLOCATOR__
#define __CUSTOM_ALLOCATOR__

/*=============================================================================
* do no edit lines below!
=============================================================================*/
#include <stddef.h> //for size_t
#include <pthread.h>
#include <stdio.h>
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

typedef struct{
    char*  startOfZone;
    pthread_mutex_t zoneLock;
    size_t remainingSpace;
    Block* zoneBlockList;
} memZone;
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
extern Block* blockList;

#endif // CUSTOM_ALLOCATOR
