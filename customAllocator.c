#include "customAllocator.h"
#include <unistd.h>
#include <stdio.h> // For error printing if needed
#include <string.h> // For memcpy, memset
#include <stdlib.h> // For exit
// Global head of the linked list (declared extern in .h)
Block* blockList = NULL;

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
    // 0. Handle 0 size request (Implementation choice, usually return NULL or generic)
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

