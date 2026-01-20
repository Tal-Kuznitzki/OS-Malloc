/* main.c */
#define _DEFAULT_SOURCE
#include "customAllocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define RST   "\x1B[0m"

// ==========================================
//           HELPER FUNCTIONS
// ==========================================

int is_aligned(void* ptr) {
    return ((size_t)ptr % 4) == 0;
}

// ==========================================
//           PART A TESTS
// ==========================================

void test_part_a_basic() {
    printf(YEL "--- Test Part A: Basic Malloc/Free ---\n" RST);

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
    customFree(arr);
    printf(GRN "PASS: Basic Malloc/Free\n" RST);
}

void test_part_a_coalescing() {
    printf(YEL "\n--- Test Part A: Coalescing (Merging Blocks) ---\n" RST);
    // Alloc 3 blocks
    void* p1 = customMalloc(100);
    void* p2 = customMalloc(100);
    void* p3 = customMalloc(100);

    // Free middle then first (should merge)
    customFree(p2);
    customFree(p1);

    // Alloc a larger block that fits into merged space (200 + overhead)
    void* p4 = customMalloc(200);

    // If coalescing works, p4 might reuse the space of p1
    if (p4 == p1) {
        printf(GRN "PASS: Blocks merged and reused address %p\n" RST, p4);
    } else {
        printf(RED "WARN: Block not reused (Address p1: %p, p4: %p)\n" RST, p1, p4);
    }
    customFree(p3);
    customFree(p4);
}

void test_alignment() {
    printf(YEL "\n--- Test: Alignment ---\n" RST);
    void* p1 = customMalloc(1);  // Should round to 4
    void* p2 = customMalloc(5);  // Should round to 8
    void* p3 = customMalloc(11); // Should round to 12

    if (is_aligned(p1) && is_aligned(p2) && is_aligned(p3)) {
        printf(GRN "PASS: All pointers aligned to 4 bytes.\n" RST);
    } else {
        printf(RED "FAIL: Alignment violation. p1=%p, p2=%p, p3=%p\n" RST, p1, p2, p3);
    }
    customFree(p1);
    printf("Freed p1\n");
    customFree(p2);
    printf("Freed p2\n");
    customFree(p3);
    printf("Freed p3\n");
}

void test_splitting() {
    printf(YEL "\n--- Test: Splitting Logic ---\n" RST);
    // 1. Alloc 200 bytes
    void* p1 = customMalloc(200);
    // 2. Free it. List has one free block of ~200.
    customFree(p1);

    // 3. Alloc 50 bytes. Should split the 200 block.
    // p2 should (ideally) match p1's address.
    void* p2 = customMalloc(50);

    // 4. Alloc another 50 bytes. Should take the remainder.
    void* p3 = customMalloc(50);

    // Verify p3 is adjacent to p2 (allowing for metadata)
    if (p2 == p1 && p3 > p2 && (char*)p3 < ((char*)p2 + 250)) {
        printf(GRN "PASS: Block split and remainder reused.\n" RST);
    } else {
        printf(RED "FAIL: Splitting logic suspicious. p1=%p, p2=%p, p3=%p\n" RST, p1, p2, p3);
    }
    customFree(p2);
    customFree(p3);
}

void test_best_fit() {
    printf(YEL "\n--- Test: Best Fit Strategy ---\n" RST);
    // Setup holes: [200 free] [allocated] [100 free]
    void* h1 = customMalloc(200);
    void* f1 = customMalloc(10);
    void* h2 = customMalloc(100);
    void* f2 = customMalloc(10);

    customFree(h1); // Free 200
    customFree(h2); // Free 100

    // Request 80.
    // Best fit for 80 is h2 (100). First fit would be h1 (200).
    void* p = customMalloc(80);

    if (p == h2) {
        printf(GRN "PASS: Best Fit selected closer match (100 vs 200).\n" RST);
    } else if (p == h1) {
        printf(RED "FAIL: Selected First Fit (200) instead of Best Fit (100).\n" RST);
    } else {
        printf(RED "FAIL: Allocated new block instead of reusing.\n" RST);
    }

    customFree(f1);
    customFree(f2);
    customFree(p);
}

void test_sbrk_release() {
    printf(YEL "\n--- Test: Memory Release (sbrk decrease) ---\n" RST);
    void* start_brk = sbrk(0);

    void* p = customMalloc(4000);
    void* mid_brk = sbrk(0);

    if (mid_brk <= start_brk) {
        printf(RED "FAIL: Heap did not grow.\n" RST);
        return;
    }

    customFree(p);
    void* end_brk = sbrk(0);

    if (end_brk < mid_brk) {
        printf(GRN "PASS: Program break decreased after freeing last block.\n" RST);
    } else {
        printf(RED "FAIL: Memory leak at end of heap (brk did not decrease).\n" RST);
    }
}

void test_realloc_expansion() {
    printf(YEL "\n--- Test: Realloc Expansion ---\n" RST);
    void* p1 = customMalloc(100);
    memset(p1, 0xAA, 100);

    // Block expansion by placing a barrier
    void* barrier = customMalloc(10);

    // Try to resize p1 to 200. Cannot grow in place. Must move.
    void* p2 = customRealloc(p1, 200);

    if (p2 != p1) {
        // Verify data copy
        if (((unsigned char*)p2)[0] == 0xAA && ((unsigned char*)p2)[99] == 0xAA) {
            printf(GRN "PASS: Block moved and data preserved.\n" RST);
        } else {
            printf(RED "FAIL: Data corrupted during realloc.\n" RST);
        }
    } else {
        printf(RED "FAIL: Realloc should have moved (barrier exists).\n" RST);
    }

    customFree(p2);
    customFree(barrier);
}

// ==========================================
//           PART B TESTS
// ==========================================

// Thread worker function (Basic)
void* thread_func_basic(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 50; i++) {
        size_t size = (rand() % 128) + 1;
        printf("before malloc\n");
        void* ptr = customMTMalloc(size);
        printf("after malloc\n");
        if (ptr) {
            memset(ptr, 0xAA, size); // Write to memory to check race conditions
            usleep(100); // Simulate work
            printf("before free\n");
            customMTFree(ptr);
            printf("after free\n");
        } else {
            printf(RED "Thread %d Failed to allocate\n" RST, id);
        }
    }
    return NULL;
}

void test_part_b_basic_mt() {
    printf(YEL "\n--- Test Part B: Basic Multi-threading ---\n" RST);
    // Note: heapCreate() is called in main() before this.

    pthread_t threads[4];
    int thread_ids[4];

    for (int i = 0; i < 4; i++) {
        thread_ids[i] = i;
        if (pthread_create(&threads[i], NULL, thread_func_basic, &thread_ids[i]) != 0) {
            perror("Thread create failed");
        }
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    printf(GRN "PASS: Basic Multi-threaded allocations completed.\n" RST);
}

// Thread worker function (Stress/Overflow)
void* stress_worker(void* arg) {
    int id = *(int*)arg;
    // Each thread tries to allocate larger chunks to force zone exhaustion
    for (int i = 0; i < 20; i++) {
        size_t size = (rand() % 2000) + 500; // 500B to 2.5KB
        void* ptr = customMTMalloc(size);
        if (ptr) {
            memset(ptr, id, size); // Ownership check
            usleep(500);

            // Check content matches
            unsigned char* check = (unsigned char*)ptr;
            if (check[0] != id || check[size-1] != id) {
                printf(RED "Thread %d: MEMORY CORRUPTION DETECTED!\n" RST, id);
            }
            customMTFree(ptr);
        }
    }
    return NULL;
}

void test_mt_zone_overflow() {
    printf(YEL "\n--- Test Part B: Zone Overflow (Creating New Zones) ---\n" RST);
    // We launch more threads than zones (or heavy usage) to force expansion

    pthread_t threads[10];
    int ids[10];

    for(int i=0; i<10; i++) {
        ids[i] = i+1;
        pthread_create(&threads[i], NULL, stress_worker, &ids[i]);
    }

    for(int i=0; i<10; i++) {
        pthread_join(threads[i], NULL);
    }

    printf(GRN "PASS: MT Stress test completed.\n" RST);
}

// ==========================================
//           MAIN
// ==========================================

int main() {
    printf("==========================================\n");
    printf("      RUNNING PART A TESTS (Single Thread)\n");
    printf("==========================================\n");

    test_part_a_basic();
    test_alignment();
    test_splitting();
    test_part_a_coalescing();
    test_best_fit();
    test_realloc_expansion();
    test_sbrk_release();

    printf("\n==========================================\n");
    printf("      RUNNING PART B TESTS (Multi Thread)\n");
    printf("==========================================\n");

    // Initialize Heap for MT tests ONCE
    heapCreate();
    printf("AFTER HEAP CREATE\n");
    test_part_b_basic_mt();
    test_mt_zone_overflow();

    // Cleanup Heap ONCE
    heapKill();

    printf("\n" GRN "ALL TESTS COMPLETED." RST "\n");
    return 0;
}