/* main.c */
#include "customAllocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define RST   "\x1B[0m"

void test_part_a_basic() {
    printf("--- Test Part A: Basic Malloc/Free ---\n");
    
    // 1. Allocate int
    int* ptr1 = (int*)customMalloc(sizeof(int));
    if (!ptr1) { printf(RED "FAIL: customMalloc returned NULL\n" RST); return; }
    *ptr1 = 42;
    printf("Allocated int: %d\n", *ptr1);

    // 2. Allocate array
    int* arr = (int*)customCalloc(10, sizeof(int));
    if (!arr) { printf(RED "FAIL: customCalloc returned NULL\n" RST); return; }
    for(int i=0; i<10; i++) assert(arr[i] == 0);
    printf("Calloc verified (all zeros).\n");

    // 3. Free
    customFree(ptr1);

    printf("pass cfirst\n");
    customFree(arr);
    printf(GRN "PASS: Basic Malloc/Free\n" RST);
    return;
}

void test_part_a_coalescing() {
    printf("\n--- Test Part A: Coalescing (Merging Blocks) ---\n");
    // Alloc 3 blocks
    void* p1 = customMalloc(100);
    printf("malloc 1  good\n");
    void* p2 = customMalloc(100);
    void* p3 = customMalloc(100);
    printf("mallic good\n");
    // Free middle then first (should merge)
    customFree(p2);
    customFree(p1);
    printf("free goood \n");
    // Alloc a larger block that fits into merged space (200 + overhead)
    void* p4 = customMalloc(200);
    printf("another onE! good!! \n");
    // If coalescing works, p4 might reuse the space of p1
    if (p4 == p1) {
        printf(GRN "PASS: Blocks merged and reused address %p\n" RST, p4);
    } else {
        printf(RED "WARN: Block not reused (Address p1: %p, p4: %p)\n" RST, p1, p4);
    }
    customFree(p3);
    customFree(p4);
}

// Thread worker function
void* thread_func(void* arg) {
    int id = *(int*)arg;
    // printf("Thread %d starting allocation\n", id);
    
    for (int i = 0; i < 50; i++) {
        size_t size = (rand() % 128) + 1;
        void* ptr = customMTMalloc(size);
        if (ptr) {
            memset(ptr, 0xAA, size); // Write to memory to check race conditions
            sleep(1); // Simulate work
            customMTFree(ptr);
        } else {
            printf(RED "Thread %d Failed to allocate\n" RST, id);
        }
    }
    return NULL;
}

void test_part_b_multithreading() {
    printf("\n--- Test Part B: Multi-threading ---\n");
    heapCreate(); // Must init the zones first

    pthread_t threads[4];
    int thread_ids[4];

    for (int i = 0; i < 4; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_func, &thread_ids[i]) != 0) {
            perror("Thread create failed");
        }
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf(GRN "PASS: Multi-threaded allocations completed without crash.\n" RST);
    heapKill();
}

int main() {
    test_part_a_basic();
    test_part_a_coalescing();
    //test_part_b_multithreading();
    return 0;
}