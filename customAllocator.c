#include "customAllocator.h"
#include <unistd.h>
#include <stdio.h> // For error printing if needed
#include <string.h> // For memcpy, memset
#include <stdlib.h> // For exit
// Global head of the linked list (declared extern in .h)
Block* blockList = NULL;
memZone Zones[8];
int memZoneIndx =0 ;
pthread_mutex_t memZoneIndxLock ;
/**
 * Helper: Find the Best Fit block.
 * Guidelines: Iterate through the list and find the free block that is
 * closest in size to 'size' but still larger or equal to it.
 * Source: [cite: 70, 84]
 */
Block* findBestFit(size_t size) {
    Block* current = blockList;
    Block* bestFit = NULL;

    while (current != NULL) {
        // We only care about free blocks that are large enough
        if (current->free && current->size >= size) {
            // If we haven't found a fit yet, or this one is tighter (smaller) than the previous best
            if (bestFit == NULL || current->size < bestFit->size) {
                bestFit = current;

                // Optimization: If we find an exact match, we can stop searching early
                if (current->size == size) {
                    return current;
                }
            }
        }
        current = current->next;
    }
    return bestFit;
}
/**
 * Helper: Find Best Fit specifically within a locked zone.
 * Returns: Pointer to the Block metadata if found, NULL otherwise.
 */


void initZoneMT(int index) {
    // 1. Get the raw memory (assuming you allocated it via sbrk/mmap)
    // Note: In your heapCreate, you need to actually Allocate this space.
    // Assuming Zones[index].startOfZone is already valid:

    // 2. Place the initial Block metadata at the very start of the zone
    Block* initialBlock = (Block*)Zones[index].startOfZone;

    // 3. Set up the block to cover the entire zone (minus metadata size)
    initialBlock->size = 0;
    initialBlock->free = true;
    initialBlock->next = NULL;

    // 4. Point the Zone's head to this block
    Zones[index].zoneBlockList = initialBlock;
}
Block* findBestFitInZoneMT(memZone* zone, size_t size) {
    Block* current = zone->zoneBlockList;
    Block* bestFit = NULL;

    while (current != NULL) {
        // We look for a FREE block that fits the requested size
        if (current->free && current->size >= size) {

            // Best Fit Logic: Pick the smallest sufficient block
            if (bestFit == NULL || current->size < bestFit->size) {
                bestFit = current;

                // Optimization: Exact match found
                if (current->size == size) {
                    return current;
                }
            }
        }
        current = current->next;
    }
    return bestFit;
}
/**
 * Helper: Request new space from the OS using sbrk.
 * Guidelines:
 * 1. Increment program break.
 * 2. Initialize the new Block struct.
 * 3. Handle allocation failures (SBRK_FAIL).
 * Source: [cite: 49, 69]
 */
Block* requestSpace(Block* last, size_t size) {
    Block* block;

    // Calculate total size needed: struct metadata + requested payload
    size_t totalSize = size + sizeof(Block);

    // Request memory from OS
    block = (Block*)sbrk(totalSize);

    // Check for sbrk failure
    if (block == SBRK_FAIL) {
        printf("<sbrk/brk error>: out of memory\n");
        exit(1);
    }

    // Initialize the new block metadata
    block->size = size;
    block->next = NULL;
    block->free = false;

    // Link the new block to the end of the list
    if (last) {
        last->next = block;
    }

    return block;
}

/**
 * Helper: Get the pointer to the Block metadata from the user pointer.
 * This is useful for free() and realloc().
 */
Block* getBlock(void* ptr) {
    return (Block*)ptr - 1;
}

/**
 * Main Allocation Function Skeleton
 */
void* customMalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // 1. Alignment: Ensure size is a multiple of 4 [cite: 121]
    size_t alignedSize = ALIGN_TO_MULT_OF_4(size);

    Block* block;

    // 2. Initial Case: List is empty
    if (blockList == NULL) {
        block = requestSpace(NULL, alignedSize);
        if (!block) {
            return NULL;
        }
        blockList = block; // Initialize the head
    } else {
        // 3. Search for a free block (Best Fit)
        Block* bestFit = findBestFit(alignedSize);

        if (bestFit) {
            // Found a reusable block
            block = bestFit;
            block->free = false;
            // TODO: Implement splitting logic here if the block is significantly larger
            // than requested (optional based on spec, but good for efficiency).


            size_t remainingSize = bestFit->size - alignedSize;

/*            if  ( bestFit->size > 2 *  ( alignedSize + sizeof(block)) ){
                // we want to know if we have enough space to split and add a node.

                //now we split to the best fit and the rest....

            }*/
            if (remainingSize >= sizeof(Block) + 4){ //basic size is 4 byte at minimum
                block->size =   alignedSize;  // sizeof(Block)  ;

                //Block* newNode = bestFit + (block->size) /sizeof(Block* )  ; //remainder
                Block* newBlock = (Block*) ((char*)block + sizeof(Block) + alignedSize);

                newBlock->size = remainingSize - sizeof(Block);
                newBlock->free = true;
                newBlock->next = block->next;
                //newNode->size = bestFit->size - block->size;
                //newNode->free = true;
               // newNode-> next = block->next;
                block->next = newBlock;
            }
        } else {
            // 4. No suitable block found, request space
            // We need to find the last block to link the new one
            Block* last = blockList;
            while (last->next != NULL) {
                last = last->next;
            }

            block = requestSpace(last, alignedSize);
            if (!block) {
                return NULL;
            }
        }
    }

    // 5. Return pointer to the data (byte after the struct)
    // Pointer arithmetic: (block + 1) moves the pointer by sizeof(Block) bytes.
    return (void*)(block + 1);
}
Block* getAndValidateBlockReturnPrev(void* ptr) {
    // 1. Basic safety check
    if (ptr == NULL) {
        return NULL;
    }

    // 2. Calculate where the block metadata *should* be
    //    We cast to Block* and subtract 1 to step back over the header
    Block* candidateBlock = (Block*)ptr - 1;

    // 3. Traverse the global list to verify this block actually exists
    Block* prev = blockList;
    while (prev->next != NULL) {
        if (prev->next == candidateBlock) {
            // Found it: The pointer is valid and points to the start of a block payload
            return prev;
        }
        prev = prev->next;
    }
    return NULL;
}
void customFree(void* ptr){
    if (ptr == NULL){
    printf("<free error>: passed null pointer\n");
        return;
    }

    Block* candidateBlock = (Block*)ptr - 1;
    if (blockList == candidateBlock){

        //check next
        if (blockList->next!=NULL && blockList->next->free == true){
            blockList->size += blockList->next->size+ sizeof(Block);
            blockList->next= blockList->next->next;
            blockList->free = true;
        }
        //check origin of the blockList
        if (blockList->next == NULL){

            if (brk(blockList)==SBRK_FAIL) {
                printf("<sbrk/brk error>: out of memory\n");
                exit(1);
            }
            blockList=NULL;
        }
        blockList->free=true;
        return;
        }
    Block* prev = getAndValidateBlockReturnPrev(ptr);
    if (prev == NULL) { //that means we didnt find the right one in the ll
        printf("<free error>: passed non-heap pointer\n");
        return;
    }
    //firsly, we want to free the curr block;
    prev->next->free=true;
    //check both sides
    if  ( (prev->free) && (prev->next->next!= NULL && prev->next->next->free) ){
        prev->next->size += prev->next->next->size + sizeof(Block);

        prev->size += prev->next->size + sizeof(Block);

        prev->next  = prev->next->next->next;
    }
    //check next
    else if( prev->next->next!= NULL && prev->next->next->free) {
        prev->next->size += prev->next->next->size + sizeof(Block);
        prev->next->next = prev->next->next->next;
    }
    //check prev
    else if (prev->free){
        prev->size += prev->next->size + sizeof(Block);
        prev->next = prev->next->next;
    }
    // now if it is the last block, we can free it and decrease brk
    if (prev->next->next == NULL){
        brk(prev->next);
        prev->next=NULL;
    }
    return;
}
void* customCalloc(size_t nmemb, size_t size){
    void* startptr = customMalloc(size*nmemb);
    char* chrptr =  (char*)startptr;
    for (int i = 0; i < size*nmemb ; ++i) {
        *chrptr = 0;
        chrptr++;
    }
    return startptr;
}

void* customRealloc(void* ptr, size_t size){
    size = ALIGN_TO_MULT_OF_4(size);

    if (ptr==NULL){
        return (void*)customMalloc(size);
    }
    Block* prev = getAndValidateBlockReturnPrev(ptr);
    if (  (prev==NULL) && ( blockList!=( (Block*)ptr - 1) )  ){
        printf("<realloc error>: passed non-heap pointer\n");
        return NULL;
    }
    Block* header = (Block*)ptr - 1;
    size_t old_size = header->size;

    if (size>=old_size){
        Block* newBlock = customMalloc(size);
        memcpy(newBlock,ptr,old_size);
        customFree(ptr);
        return (void*)newBlock;
    }
    if (size<old_size){

        char* end_ptr_mem =(char*)ptr+size ;
        size_t sizeToFree = old_size -size ;

        Block* curr;
        if ( blockList==( (Block*)ptr - 1) ) {
            curr = blockList;
        }
        else{
           curr=prev->next;
        }
        Block* BlocktoFree =(Block*)end_ptr_mem;
        BlocktoFree->free=true;
        if (sizeToFree>sizeof(Block)){
            BlocktoFree->size= sizeToFree - sizeof(Block);
            BlocktoFree->next= curr->next;
            curr->next=end_ptr_mem;
            curr->size=size;
            curr->free=false;
            customFree(end_ptr_mem+1);
            return (void*)(curr+1) ;
        }
        else{
            Block* newBlock = customMalloc(size);
            memcpy(newBlock,ptr,size);
            customFree(ptr);
            return (void*)newBlock;
        }



    }




}

void* customMTMalloc(size_t size) {
    size_t alignedSize = ALIGN_TO_MULT_OF_4(size);

    pthread_mutex_lock(&memZoneIndxLock);
    int localIndx = memZoneIndx % 8;
    memZoneIndx++;
    pthread_mutex_unlock(&memZoneIndxLock);
    for (int i = 0; i < 8; ++i) {
        int currZoneIndx = (localIndx + i) % 8;
        if (Zones[currZoneIndx].remainingSpace < size + sizeof(Block)) { //there is enough space...
            continue;
        } else { // there is enough space :)
            pthread_mutex_lock((&Zones[currZoneIndx].zoneLock));
            Block *block = findBestFitInZoneMT(&Zones[currZoneIndx], alignedSize);
            if (block != NULL) {
                // --- FOUND A BLOCK ---

                // Mark as used immediately
                block->free = false;

                // --- SPLITTING LOGIC (From Part A) ---
                size_t remainingSize = block->size - alignedSize;

                // Check if we have enough space for a Header + min 4 bytes payload
                if (remainingSize >= sizeof(Block) + 4) {
                    // A. Update the size of the allocated block
                    block->size = alignedSize;

                    // B. Calculate address of the new neighbor block
                    //    Use (char*) to ensure byte-precise pointer arithmetic
                    Block *newBlock = (Block *) ((char *) block + sizeof(Block) + alignedSize);

                    // C. Initialize the new split block
                    newBlock->size = remainingSize - sizeof(Block);
                    newBlock->free = true;
                    newBlock->next = block->next; // Point to whatever block->next was

                    // D. Link current block to the new split block
                    block->next = newBlock;
                }

                // Update free space stats for the zone (Total Free - Allocated Payload - Header Overhead)
                // Note: If we split, we only "lost" the alignedSize + metadata of the first part.
                // If we didn't split, we "lost" the whole block->size + metadata.
                Zones[currZoneIndx].remainingSpace -= (block->size + sizeof(Block));


                // Unlock and return User Pointer
                pthread_mutex_unlock(&zone->zoneLock);
                return (void *) (block + 1);
            }
        }
    }
    printf("OUT OF MEMORY WE ARE HERE");
    return NULL;
}
void customMTFree(void* ptr){
    //TODO ADD LOCKS !!!!!!!!!
    for (int i = 0; i < 8 ; ++i) {
        if (  !( ( Zones[i].startOfZone <= ptr) && (ptr < (Zones[i].startOfZone+4*1024) ) ) ){
           continue;
        }
        else{
            pthread_mutex_lock(&Zones[i].zoneLock);
            if (ptr == NULL){
                printf("<freeMT error>: passed null pointer\n");
                return;
            }

            Block* candidateBlock = (Block*)ptr - 1;
            if (Zones[i].zoneBlockList == candidateBlock){

                //check next
                if (Zones[i].zoneBlockList->next!=NULL && blockList->next->free == true){
                    Zones[i].zoneBlockList->size += blockList->next->size+ sizeof(Block);
                    Zones[i].zoneBlockList->next= blockList->next->next;
                    Zones[i].zoneBlockList->free = true;
                }
                //check origin of the blockList
                if (Zones[i].zoneBlockList->next == NULL){

                    if (brk(blockList)==SBRK_FAIL) {
                        printf("<sbrk/brk error>: out of memory\n");
                        exit(1);
                    }
                    Zones[i].zoneBlockList=NULL;
                }
                Zones[i].zoneBlockList->free=true;
                return;
            }
            Block* prev = getAndValidateBlockReturnPrev(ptr); // TODO: CUSAMAK
            if (prev == NULL) { //that means we didnt find the right one in the ll
                printf("<free error>: passed non-heap pointer\n");
                return;
            }
            //firsly, we want to free the curr block;
            prev->next->free=true;
            //check both sides
            if  ( (prev->free) && (prev->next->next!= NULL && prev->next->next->free) ){
                prev->next->size += prev->next->next->size + sizeof(Block);

                prev->size += prev->next->size + sizeof(Block);

                prev->next  = prev->next->next->next;
            }
                //check next
            else if( prev->next->next!= NULL && prev->next->next->free) {
                prev->next->size += prev->next->next->size + sizeof(Block);
                prev->next->next = prev->next->next->next;
            }
                //check prev
            else if (prev->free){
                prev->size += prev->next->size + sizeof(Block);
                prev->next = prev->next->next;
            }
            // now if it is the last block, we can free it and decrease brk
            if (prev->next->next == NULL){
                brk(prev->next);
                prev->next=NULL;
            }
            pthread_mutex_unlock(&Zones[i].zoneLock);
            return;
        }



    }
    }



void* customMTCalloc(size_t nmemb, size_t size){}
void* customMTRealloc(void* ptr, size_t size){}


void heapCreate(){
    if  (pthread_mutex_init(&memZoneIndxLock), NULL) != 0){
        perror("Mutex init failed cry");
        return;
    }
    for (int i = 0; i < 8; ++i) {
        if (pthread_mutex_init(&(Zones[i].zoneLock), NULL) != 0) {
            perror("Mutex init failed");
            return;
        }
        Zones[i].startOfZone = sbrk(0) + 4*i*1024;
        Zones[i].remainingSpace = 4*1024;
    }
}
void heapKill(){
    for (int i = 0; i < 8; ++i) {
        pthread_mutex_destroy( &(Zones[i].zoneLock) );
        customMTFree(Zones[i].startOfZone);
    }
}


}