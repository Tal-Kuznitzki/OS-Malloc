#define _DEFAULT_SOURCE
#include <unistd.h>
#include "customAllocator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Block* blockList = NULL;
memZone* zone_list_head;
int memZoneIndx =0 ;
pthread_mutex_t memZoneIndxLock;
pthread_mutex_t num_of_zones_lock;
int num_of_zones = 8;

memZone* create_new_zone(){
 //   printf("inside CREATE NEW ZONE \n");
    memZone* curr = zone_list_head;
    while (curr!=NULL){
        if (curr->next == NULL){
         //   printf("before brk");
            memZone* new_zone = sbrk(sizeof(memZone));
            if (new_zone == (void*)-1) {
                printf("<sbrk/brk error>: out of memory\n");
                exit(1);
            }

            new_zone->startOfZone = (char*)sbrk(4096);
            if (new_zone->startOfZone == SBRK_FAIL) {
                printf("<sbrk/brk error>: out of memory\n");
                exit(1);
            }

            if (pthread_mutex_init(&(new_zone->zoneLock), NULL) != 0) {
                perror("Mutex init failed");
                return NULL;
            }
            new_zone->remainingSpace = 4 * 1024;

            Block* initialBlock = (Block*)new_zone->startOfZone;
            initialBlock->size =ALIGN_TO_MULT_OF_4( (4 * 1024) - sizeof(Block)); // Payload size
            initialBlock->free = true;
            initialBlock->next = NULL;

            new_zone->zoneBlockList = initialBlock;
            new_zone->next = NULL;

            curr->next = new_zone;
            return new_zone;
        }
        else {
            curr = curr->next;
        }
    }
    return NULL;
}
Block* findBestFit(size_t size) {
    Block* current = blockList;
    Block* bestFit = NULL;

    while (current != NULL) {
        // We only care about free blocks that are large enough
        if (current->free && current->size >= size) {

            if (bestFit == NULL || current->size < bestFit->size) {
                bestFit = current;


                if (current->size == size) {
                    return current;
                }
            }
        }
        current = current->next;
    }
    return bestFit;
}




Block* findBestFitInZoneMT(memZone* zone, size_t size) {
    Block* current = zone->zoneBlockList;
    Block* bestFit = NULL;

    while (current != NULL) {

        if (current->free && current->size >= size) {


            if (bestFit == NULL || current->size < bestFit->size) {
                bestFit = current;


                if (current->size == size) {
                    return current;
                }
            }
        }
        current = current->next;
    }
    return bestFit;
}

Block* requestSpace(Block* last, size_t size) {
    Block* block;

    // Calculate total size needed: struct metadata + requested payload
    size_t totalSize = size + sizeof(Block);

    block = (Block*)sbrk(totalSize);


    if (block == SBRK_FAIL) {
        printf("<sbrk/brk error>: out of memory\n");
        exit(1);
    }


    block->size = size;
    block->next = NULL;
    block->free = false;

    if (last) {
        last->next = block;
    }

    return block;
}


Block* getBlock(void* ptr) {
    return (Block*)ptr - 1;
}


void* customMalloc(size_t size) {
 //   printf("hello mallic\n");
    if (size == 0) {
       // printf("???????");
        return NULL;
    }
    size_t alignedSize = ALIGN_TO_MULT_OF_4(size);

    Block* block;


    if (blockList == NULL) {
       // printf("inside the if \n");
        block = requestSpace(NULL, alignedSize);
      //  printf("requesteed space good\n");
        if (!block) {
            return NULL;
        }
        blockList = block;
    } else {

        Block* bestFit = findBestFit(alignedSize);
       // printf("after bestfit\n");
        if (bestFit) {

            block = bestFit;
            block->free = false;
            size_t remainingSize = bestFit->size - alignedSize;

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
    return (void*)(block + 1);
}
Block* getAndValidateBlockReturnPrev(void* ptr) {
    if (ptr == NULL) {
        return NULL;
    }
    Block* candidateBlock = (Block*)ptr - 1;
  //  printf("inside getandval\n");

    Block* prev = blockList;
 //   printf("got prev\n");
    while (prev->next != NULL) {
 //       printf("inside prev->next\n");
        if (prev->next == candidateBlock) {
    //       printf("found candidate\n");
            return prev;
        }
        prev = prev->next;
    }
  //  printf("we end\n");
    return NULL;
}
Block* getAndValidateBlockReturnPrevMT(void* ptr, Block* blockListInSpecificZone ) {

    if (ptr == NULL) {
        return NULL;
    }

    Block* candidateBlock = (Block*)ptr - 1;
    Block* prev = blockListInSpecificZone;
    while (prev->next != NULL) {
        if (prev->next == candidateBlock) {
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
  //  printf("found canidate\n");
    if (blockList == candidateBlock){
   //    printf(" canidate is top\n");
        //check next
        if (blockList->next!=NULL && blockList->next->free == true){
      //      printf("next inside top\n");
            blockList->size += blockList->next->size+ sizeof(Block);
            blockList->next= blockList->next->next;
            blockList->free = true;
        }
        blockList->free=true;
        if (blockList->next == NULL){
        //    printf("chk orgn\n");
            if (brk(blockList)==BRK_FAIL) {
                printf("<sbrk/brk error>: out of memory\n");
                exit(1);
            }
            blockList=NULL;
        }

        return;
    }

 //  printf("before getand val canidate\n");
    Block* prev = getAndValidateBlockReturnPrev(ptr);
 //   printf("after getand val canidate\n");
    if (prev == NULL) { //that means we didnt find the right one in the ll
        printf("<free error>: passed non-heap pointer\n");
        return;
    }
    //firsly, we want to free the curr block;
    prev->next->free=true;
    //check both sides

   // printf("before big IF \n");

    // now if it is the last block, we can free it and decrease brk
    if (prev->next!= NULL && prev->next->next == NULL){
   //     printf("check last\n");
        if (prev->free){
      //      printf("check prev\n");
            prev->size += prev->next->size + sizeof(Block);
            prev->next = prev->next->next;
         //   printf("IN  prev\n");
            if (prev->next == NULL){
           //     printf("gem is right before BRK\n");
                if (brk(prev) == BRK_FAIL){
                    printf("<sbrk/brk error>: out of memory\n");
                    exit(1);
                }
                if (prev == blockList) {
                    blockList = NULL;
                }
                return;
            }
        }

        if (brk(prev->next) == BRK_FAIL) {
            printf("<sbrk/brk error>: out of memory\n");
            exit(1);
        }
        prev->next=NULL;
        if ((prev == blockList) && (prev->free == true)) {
       //     printf("prev==blocklist\n\n");
            blockList = NULL;
        }
    }

   else if  ( (prev->free) && (prev->next->next!= NULL && prev->next->next->free) ){
     //   printf("check both sides inside\n");
        prev->next->size += prev->next->next->size + sizeof(Block);

        prev->size += prev->next->size + sizeof(Block);

        prev->next  = prev->next->next->next;
    }
        //check next
    else if( prev->next->next!= NULL && prev->next->next->free) {
    //    printf("check next\n");
        prev->next->size += prev->next->next->size + sizeof(Block);
        prev->next->next = prev->next->next->next;
    }
        //check prev
    else if (prev->free){
    //    printf("check prev\n");
        prev->size += prev->next->size + sizeof(Block);
        prev->next = prev->next->next;
     //   printf("IN  prev\n");
        if (prev->next == NULL){
        //    printf("before BRK\n");
            if (brk(prev) == BRK_FAIL){
                printf("<sbrk/brk error>: out of memory\n");
                exit(1);
            }
            if (prev == blockList) {
                blockList = NULL;
            }
            return;
        }
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
        size_t sizeToFree = old_size - size ;
       // char start_ptr_to_free = old_size - size ;
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
         //   printf("@@@@@@\n");
            BlocktoFree->size= sizeToFree - sizeof(Block);
            BlocktoFree->next= curr->next;
            curr->next=(Block*)end_ptr_mem;
            curr->size=size;
            curr->free=false;
            customFree((void*)((Block*)end_ptr_mem + 1));
            return (void*)(curr+1) ;
        }
        else{
            Block* newBlock = customMalloc(size);
            memcpy(newBlock,ptr,size);
            customFree(ptr);
            return (void*)newBlock;
        }
    }
   // printf("DEBUG BAD IN REALLOC");
    return NULL;
}

void* customMTMalloc(size_t size) {
    size_t alignedSize = ALIGN_TO_MULT_OF_4(size);
    pthread_mutex_lock(&num_of_zones_lock);


    pthread_mutex_lock(&memZoneIndxLock);


    int localIndx = memZoneIndx % num_of_zones;
    memZoneIndx++;


    pthread_mutex_unlock(&memZoneIndxLock);


    memZone* curr = zone_list_head;
    memZone* chosen;
    for (int i = 0; i<localIndx+1 ; i++){
       // printf("section IS %d \n ",i);
        chosen = curr;
        curr = curr->next;
    }
   // printf("GOT CHOSEN ONE !!\n");
    pthread_mutex_lock((&chosen->zoneLock));
    if (chosen->remainingSpace < alignedSize + sizeof(Block)) { //there isn't enough space...
    //    printf("try to create new zone\n");
        memZone* new_zone = create_new_zone();
    //    printf("Create new ZONE good \n");
        if (new_zone == NULL){
            printf("OUT OF MEMORY WE ARE HERE");
            pthread_mutex_unlock(&num_of_zones_lock);
            return NULL;
        }
        num_of_zones++;
        //chosen = new_zone;

    }
  //  printf("not iside chosen remaining space\n");
    for (int i = 0; i < num_of_zones; ++i) {
       // int currZoneIndx = (localIndx + i) % num_of_zones;
    //    printf("iter number IS %d \n ",i);
        if (chosen->remainingSpace >= (alignedSize + sizeof(Block))){

        //    printf("before bestFit\n");
            Block *block = findBestFitInZoneMT(chosen, alignedSize);
       //     printf("after bestFit\n");
            if (block != NULL) {

                block->free = false;

                size_t remainingSize = block->size - alignedSize;

                if (remainingSize >= sizeof(Block) + 4) {

                    block->size = alignedSize;
                    Block *newBlock = (Block *) ((char *) block + sizeof(Block) + alignedSize);

                    newBlock->size = remainingSize - sizeof(Block);
                    newBlock->free = true;
                    newBlock->next = block->next;
                    block->next = newBlock;
                }

                chosen->remainingSpace -= (block->size + sizeof(Block));
                // Unlock and return User Pointer
                pthread_mutex_unlock(&chosen->zoneLock);
                pthread_mutex_unlock(&num_of_zones_lock);
                return (void *) (block + 1);

            }
        }

        if (chosen->next == NULL){
            pthread_mutex_unlock((&chosen->zoneLock));
            chosen = zone_list_head;
            pthread_mutex_lock((&chosen->zoneLock));
        }
        else {
            pthread_mutex_unlock((&chosen->zoneLock));
            chosen = chosen->next;
            pthread_mutex_lock((&chosen->zoneLock));
        }
    }
    pthread_mutex_unlock((&chosen->zoneLock));
    pthread_mutex_unlock(&num_of_zones_lock);
 //   printf("OUT OF MEMORY WE ARE HERE");
    return NULL;
}
void customMTFree(void* ptr){
    memZone* curr = zone_list_head;
    while (curr != NULL) {
        if (  !( ( (char*)curr->startOfZone <= (char*)ptr ) && ((char*)ptr < ( (char*)(curr->startOfZone+4*1024)) ) ) ){
            curr = curr->next;
            continue;
        }
        else{
            pthread_mutex_lock(&curr->zoneLock);
            if (ptr == NULL){
                printf("<freeMT error>: passed null pointer\n");
                pthread_mutex_unlock(&(curr->zoneLock));
                return;
            }
            Block* candidateBlock = (Block*)ptr - 1;
            if (curr->zoneBlockList == candidateBlock){
                curr->remainingSpace +=  ( curr->zoneBlockList->size + sizeof(Block) ) ;
                //check next
                if (curr->zoneBlockList->next!=NULL && curr->zoneBlockList->next->free == true){
                    curr->zoneBlockList->size += curr->zoneBlockList->next->size+ sizeof(Block);
                    curr->zoneBlockList->next= curr->zoneBlockList->next->next;
                    curr->zoneBlockList->free = true;
                }
                //check origin of the blockList
                if (curr->zoneBlockList->next == NULL){
/*                    if (brk(Zones[i].zoneBlockList)==SBRK_FAIL) {
                        printf("<sbrk/brk error>: out of memory\n");
                        exit(1);
                    }*/
                    //Zones[i].zoneBlockList=NULL;
                    curr->zoneBlockList->free=1;
                }
                curr->zoneBlockList->free=true;
                pthread_mutex_unlock(&curr->zoneLock);
                return;
            }
            Block* prev = getAndValidateBlockReturnPrevMT(ptr, curr->zoneBlockList);

            if (prev == NULL) { //that means we didnt find the right one in the ll
                printf("<free error>: passed non-heap pointer\n");
                pthread_mutex_unlock(&curr->zoneLock);
                return;
            }
            curr->remainingSpace +=  ( prev->next->size + sizeof(Block) ) ;
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
            else if (prev->free ){
                prev->size += prev->next->size + sizeof(Block);
                prev->next = prev->next->next;
            }
            // now if it is the last block, we can free it



            if (prev->next!= NULL && prev->next->next == NULL){
                prev->next->free=1; // ?
            }
            pthread_mutex_unlock(&curr->zoneLock);
            return;
        }}
}
void* customMTCalloc(size_t nmemb, size_t size){
    void* startptr = customMTMalloc(size*nmemb);
    char* chrptr =  (char*)startptr;
    for (int i = 0; i < size*nmemb ; ++i) {
        *chrptr = 0;
        chrptr++;
    }
    return startptr;
}
void* customMTRealloc(void* ptr, size_t size) {
    size = ALIGN_TO_MULT_OF_4(size);
    if (ptr == NULL) {
        return (void *) customMTMalloc(size);
    }
    memZone* curr_zone = zone_list_head;
    while (curr_zone != NULL)
    {
        if (  !( ( (char*)curr_zone->startOfZone <= (char*)ptr ) && ((char*)ptr < ( (char*)(curr_zone->startOfZone+4*1024)) ) ) ){
            curr_zone = curr_zone->next;
            continue;
        }
        else {
            pthread_mutex_lock(&curr_zone->zoneLock);
            Block *prev_block = getAndValidateBlockReturnPrevMT(ptr, curr_zone->zoneBlockList);
            pthread_mutex_unlock(&curr_zone->zoneLock);
            if ((prev_block == NULL) && (curr_zone->zoneBlockList != ((Block *) ptr - 1))) {
                printf("<realloc error>: passed non-heap pointer\n");
                return NULL;
            }
            Block *header = (Block *) ptr - 1;
            size_t old_size = header->size;

            if (size >= old_size) {
                Block *newBlock = customMTMalloc(size);
                if (!newBlock) return NULL;
                memcpy(newBlock, ptr, old_size);
                customMTFree(ptr);
                return (void *) newBlock;
            }
            if (size < old_size) {
                pthread_mutex_lock(&curr_zone->zoneLock);
                char *end_ptr_mem = (char *) ptr + size;
                size_t sizeToFree = old_size - size;

                Block *curr_block;
                if (curr_zone->zoneBlockList == ((Block *) ptr - 1)) {
                    curr_block = curr_zone->zoneBlockList;
                } else {
                    curr_block = prev_block->next;
                }
                Block *BlocktoFree = (Block *) end_ptr_mem;
                BlocktoFree->free = true;
                if (sizeToFree > sizeof(Block)) {
                    BlocktoFree->size = sizeToFree - sizeof(Block);
                    BlocktoFree->next = curr_block->next;
                    curr_block->next = (Block*)end_ptr_mem;
                    curr_block->size = size;
                    curr_block->free = false;
                    pthread_mutex_unlock(&curr_zone->zoneLock);
                    customMTFree((void *) ((Block *) end_ptr_mem + 1));
                    return (void *) (curr_block + 1);
                } else {
                    pthread_mutex_unlock(&curr_zone->zoneLock);
                    Block *newBlock = customMTMalloc(size);
                    pthread_mutex_lock(&curr_zone->zoneLock);
                    memcpy(newBlock, ptr, size);
                    pthread_mutex_unlock(&curr_zone->zoneLock);
                    customMTFree(ptr);
                    return (void *) newBlock;
                }
            }
        }
    }
   // printf("REALLOCMT ERRRRR\n");
    return NULL;
}
void heapCreate(){
    if  ( (pthread_mutex_init(&num_of_zones_lock,NULL)) != 0) {
        perror("Mutex init failed cry");
        return;
    }


    if  ( (pthread_mutex_init(&memZoneIndxLock,NULL)) != 0) {
        perror("Mutex init failed cry");
        return;
    }
    void* metadata = (void*)sbrk(sizeof(memZone));
    if (metadata == (void*)-1) {
        printf("<sbrk/brk error>: out of memory\n");
        exit(1);
    }
    zone_list_head = metadata;
    memZone* curr = zone_list_head;
    for (int i = 0; i < 8; ++i) {
        void* heapStart = (void*)sbrk(4 * 1024);
        if (pthread_mutex_init(&(curr->zoneLock), NULL) != 0) {
            perror("Mutex init failed");
            return;
        }
        curr->startOfZone = (char*)heapStart;
        curr->remainingSpace = 4 * 1024;

        Block* initialBlock = (Block*)curr->startOfZone;
        initialBlock->size =ALIGN_TO_MULT_OF_4( (4 * 1024) - sizeof(Block));
        initialBlock->free = true;
        initialBlock->next = NULL;

        curr->zoneBlockList = initialBlock;
        if (i<7){
            void* new = (void*) sbrk(sizeof(memZone));
            curr->next = new;
            curr = curr->next;
        }


    }
}
void heapKill(){
    while(zone_list_head != NULL) {
        //customMTFree( (void*)(Zones[i].startOfZone+1)  );
        pthread_mutex_destroy( &(zone_list_head->zoneLock) );
        zone_list_head->startOfZone = NULL;
        zone_list_head->remainingSpace = 4 *1024;
        zone_list_head->zoneBlockList = NULL;
        zone_list_head = zone_list_head->next;
    }
    pthread_mutex_destroy(&memZoneIndxLock);

}