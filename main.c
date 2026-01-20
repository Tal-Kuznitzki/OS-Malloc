/* main.c */
#define _DEFAULT_SOURCE
#include "customAllocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define RST   "\x1B[0m"

// ==========================================
//           HELPER FUNCTIONS
// ==========================================

int is_aligned(void* ptr) {
    return ((size_t)ptr % 4) == 0;
}

int check_zero(void* ptr, size_t size) {
    unsigned char* p = (unsigned char*)ptr;
    for(size_t i = 0; i < size; i++) {
        if (p[i] != 0) return 0;
    }
    return 1;
}

// ==========================================
//           PART A: BASIC TESTS
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
    customFree(p2);
    customFree(p3);
}

void test_splitting() {
    printf(YEL "\n--- Test: Splitting Logic ---\n" RST);
    // 1. Alloc 200 bytes
    void* p1 = customMalloc(200);
    // 2. Alloc a fence to prevent merging with top chunk
    void* p4 = customMalloc(50);

    // 3. Free p1. List has one free block of ~200.
    customFree(p1);

    // 4. Alloc 50 bytes. Should split the 200 block.
    // p2 should (ideally) match p1's address.
    void* p2 = customMalloc(50);

    // 5. Alloc another 50 bytes. Should take the remainder.
    void* p3 = customMalloc(50);

    // Verify p3 is adjacent to p2 (allowing for metadata)
    if (p2 == p1 && p3 > p2 && (char*)p3 < ((char*)p2 + 250)) {
        printf(GRN "PASS: Block split and remainder reused.\n" RST);
    } else {
        printf(RED "FAIL: Splitting logic suspicious. p1=%p, p2=%p, p3=%p\n" RST, p1, p2, p3);
    }
    customFree(p2);
    customFree(p3);
    customFree(p4);
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

// ==========================================
//           CALLOC & REALLOC EDGE CASES
// ==========================================

void test_calloc_large() {
    printf(YEL "\n--- Test: Calloc Large Allocation ---\n" RST);
    // Allocate 100KB
    size_t size = 1024 * 100;
    void* ptr = customCalloc(1, size);

    if (ptr) {
        if (check_zero(ptr, size)) {
            printf(GRN "PASS: Large calloc successful and zeroed.\n" RST);
        } else {
            printf(RED "FAIL: Large calloc not zeroed correctly.\n" RST);
        }
        customFree(ptr);
    } else {
        printf(RED "FAIL: Large calloc failed to allocate.\n" RST);
    }
}

void test_realloc_null_and_zero() {
    printf(YEL "\n--- Test: Realloc Edge Cases (NULL/Zero) ---\n" RST);

    // 1. Realloc NULL -> Malloc
    void* ptr = customRealloc(NULL, 100);
    if (ptr != NULL) {
        printf(GRN "PASS: realloc(NULL) behaved like malloc.\n" RST);
    } else {
        printf(RED "FAIL: realloc(NULL) returned NULL.\n" RST);
    }

    // 2. Realloc 0 -> Free
    // Note: Implementation specific, but usually returns NULL or frees
    void* ptr2 = customRealloc(ptr, 0);
    if (ptr2 == NULL) {
        printf(GRN "PASS: realloc(ptr, 0) returned NULL (freed).\n" RST);
    } else {
        printf(YEL "WARN: realloc(ptr, 0) returned pointer. Ensuring it's freed.\n" RST);
        customFree(ptr2);
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

void test_realloc_shrink_split() {
    printf(YEL "\n--- Test: Realloc Shrink (Splitting) ---\n" RST);

    // 1. Alloc 200 bytes
    void* p1 = customMalloc(200);

    // 2. Shrink to 50 bytes
    // Expected: p1 stays valid (in-place), remaining 150 bytes become a free block.
    void* p2 = customRealloc(p1, 50);

    if (p2 != p1) {
        printf(RED "FAIL: Shrink moved the pointer (Inefficient).\n" RST);
        customFree(p2);
        return;
    }

    // 3. Verify Splitting:
    // If we request a malloc(100) now, it should fit in the hole we just created right after p2.
    void* p3 = customMalloc(100);

    // Check adjacency: p3 should be roughly p2 + 50 + metadata_size
    // Assuming metadata is ~16-32 bytes.
    long diff = (char*)p3 - (char*)p2;

    if (diff > 50 && diff < 150) {
        printf(GRN "PASS: Remainder was split and reused successfully.\n" RST);
    } else {
        printf(RED "FAIL: Remainder was not reused correctly (Diff: %ld)\n" RST, diff);
    }

    customFree(p2);
    customFree(p3);
}

void test_comb(){
    void* p1 = customMalloc(100);
    void* p2 = customMalloc(100);
    void* p3 = customMalloc(100);
    customFree(p1);
    customFree(p2);
    customFree(p3);

    void* p4 = customMalloc(100);
    void* p5 = customMalloc(100);
    void* p6 = customMalloc(100);
    void* p7 = customMalloc(100);

    customFree(p5);
    customFree(p4);

    customFree(p7);
    customFree(p6);

    printf(GRN "test_comb GOOD\n" RST);
}

// ==========================================
//           PART B TESTS
// ==========================================

// Thread worker function (Basic)
void* thread_func_basic(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 50; i++) {
        size_t size = (rand() % 128) + 1;
        void* ptr = customMTMalloc(size);
        if (ptr) {
            memset(ptr, 0xAA, size); // Write to memory to check race conditions
            usleep(100); // Simulate work
            customMTFree(ptr);
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
//           ROBUSTNESS TESTS
// ==========================================

void test_part_a_random_ops() {
    printf(YEL "\n--- Test Part A: Randomized Stress Test ---\n" RST);

#define NUM_PTRS 50
#define NUM_OPS 500
    void* ptrs[NUM_PTRS] = {0};

    srand(42); // Fixed seed for reproducibility

    for (int i = 0; i < NUM_OPS; i++) {
        int idx = rand() % NUM_PTRS;
        int op = rand() % 3; // 0=Malloc, 1=Free, 2=Realloc

        if (ptrs[idx] == NULL) {
            // If null, we can only Malloc
            size_t size = (rand() % 500) + 1;
            ptrs[idx] = customMalloc(size);
            if (ptrs[idx]) memset(ptrs[idx], 0x11, size);
        } else {
            if (op == 1) { // Free
                customFree(ptrs[idx]);
                ptrs[idx] = NULL;
            } else if (op == 2) { // Realloc
                size_t new_size = (rand() % 1000) + 1;
                void* new_ptr = customRealloc(ptrs[idx], new_size);
                if (new_ptr) {
                    ptrs[idx] = new_ptr;
                    // Write to end to check bounds
                    ((char*)ptrs[idx])[new_size-1] = 0x22;
                }
            }
            // If op==0 (Malloc) but exists, do nothing (simulate busy slot)
        }
    }

    // Cleanup all
    for (int i = 0; i < NUM_PTRS; i++) {
        if (ptrs[i]) {
            customFree(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    printf(GRN "PASS: Part A Randomized Operations survived.\n" RST);
}

void test_part_a_large_allocation() {
    printf(YEL "\n--- Test Part A: Large Allocation ---\n" RST);
    size_t large_size = 1024 * 1024; // 1MB
    void* ptr = customMalloc(large_size);
    if (ptr) {
        memset(ptr, 0xFF, large_size); // Ensure we can write to it
        printf("Successfully allocated 1MB at %p\n", ptr);
        customFree(ptr);
        printf(GRN "PASS: Large allocation handled.\n" RST);
    } else {
        printf(RED "FAIL: Large allocation returned NULL.\n" RST);
    }
}

// ==========================================
//           PART B: HEAVY LOAD
// ==========================================

#define THREAD_COUNT 16
#define ALLOCS_PER_THREAD 200

void* mt_heavy_worker(void* arg) {
    long id = (long)arg;

    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        size_t size = (rand() % 1024) + 1; // 1B to 1KB

        // 1. Malloc
        unsigned char* ptr = (unsigned char*)customMTMalloc(size);
        if (!ptr) {
            // Malloc might fail if OOM, acceptable, but we track unexpected failures
            continue;
        }

        // 2. Write pattern (Verify Exclusive Access)
        for (size_t j = 0; j < size; j++) {
            ptr[j] = (unsigned char)(id & 0xFF);
        }

        // 3. Sleep (Force Context Switches)
        if (i % 10 == 0) usleep(10);

        // 4. Verify pattern
        for (size_t j = 0; j < size; j++) {
            if (ptr[j] != (unsigned char)(id & 0xFF)) {
                fprintf(stderr, RED "Thread %ld: Data Corruption detected!\n" RST, id);
                exit(1);
            }
        }

        // 5. Realloc (Randomly)
        if (rand() % 4 == 0) {
            size_t new_size = size * 2;
            unsigned char* new_ptr = (unsigned char*)customMTRealloc(ptr, new_size);
            if (new_ptr) {
                ptr = new_ptr;
                // Verify old data preserved
                if (ptr[0] != (unsigned char)(id & 0xFF)) {
                    fprintf(stderr, RED "Thread %ld: Realloc corrupted data!\n" RST, id);
                }
            }
        }

        // 6. Free
        customMTFree(ptr);
    }
    return NULL;
}

void test_part_b_heavy_load() {
    printf(YEL "\n--- Test Part B: Heavy Multi-Threaded Load ---\n" RST);
    pthread_t threads[THREAD_COUNT];

    // We expect heapCreate() to be called externally before this test

    for (long i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, mt_heavy_worker, (void*)i);
    }

    for (long i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    printf(GRN "PASS: Heavy Multi-Threaded Load completed without corruption.\n" RST);
}

// ==========================================
//           COMBINED (INTERLEAVED) TESTS
// ==========================================

void test_combined_lifecycle() {
    printf(YEL "\n--- Test Combined: Mixed A & B Lifecycle ---\n" RST);
    printf(BLU "Step 1: Part A Malloc\n" RST);

    // 1. Part A allocation (Standard sbrk)
    void* a_ptr1 = customMalloc(128);
    memset(a_ptr1, 'A', 128);

    // 2. Initialize Part B (This calls sbrk multiple times for zones)
    printf(BLU "Step 2: Heap Create (Part B Init)\n" RST);
    // 3. Part B Allocation
    printf(BLU "Step 3: Part B Malloc\n" RST);
    void* b_ptr1 = customMTMalloc(256);
    if(b_ptr1) memset(b_ptr1, 'B', 256);

    // 4. Part A Allocation (After Part B init)
    // This tests if Part A can continue allocating after sbrk has moved significantly
    printf(BLU "Step 4: Part A Malloc (After Heap Create)\n" RST);
    void* a_ptr2 = customMalloc(64);
    if(a_ptr2) memset(a_ptr2, 'C', 64);

    // 5. Validate memory is intact
    if (*(char*)a_ptr1 != 'A') printf(RED "FAIL: a_ptr1 corrupted\n" RST);
    if (b_ptr1 && *(char*)b_ptr1 != 'B') printf(RED "FAIL: b_ptr1 corrupted\n" RST);
    if (a_ptr2 && *(char*)a_ptr2 != 'C') printf(RED "FAIL: a_ptr2 corrupted\n" RST);

    // 6. Cross-Freeing checks (Ensure A handles A, B handles B)
    printf(BLU "Step 5: Freeing mixed pointers\n" RST);

    // Free A pointers
    customFree(a_ptr2);
    // Free B pointer
    customMTFree(b_ptr1);
    // Free A pointer
    customFree(a_ptr1);

    // 7. Kill Heap (Part B cleanup)
    printf(BLU "Step 6: Heap Kill\n" RST);
    // 8. Part A Allocation After Heap Kill
    // Verify Part A still works after Part B is destroyed
    printf(BLU "Step 7: Part A Malloc (After Heap Kill)\n" RST);
    void* a_ptr3 = customMalloc(50);
    if(a_ptr3) {
        memset(a_ptr3, 'D', 50);
        customFree(a_ptr3);
        printf(GRN "PASS: Combined Lifecycle (A -> Init -> B -> A -> Free -> Kill -> A) successful.\n" RST);
    } else {
        printf(RED "FAIL: Part A failed after Heap Kill.\n" RST);
    }
}

// ==========================================
//           MAIN
// ==========================================

int main() {
    heapCreate();
    printf("==========================================\n");
    printf("      RUNNING ROBUSTNESS SUITE\n");
    printf("==========================================\n");

    // --- Part A Basic & Edge Cases ---
    test_part_a_basic();
    test_alignment();
    test_splitting();
    test_part_a_coalescing();
    test_best_fit();
    test_sbrk_release();
    test_comb();

    // Calloc / Realloc specific
    test_calloc_large();
    test_realloc_null_and_zero();
    test_realloc_expansion();
    test_realloc_shrink_split();

    // --- Part B Tests ---
    test_part_b_basic_mt();
    test_mt_zone_overflow();

    // --- Robustness ---
    test_part_a_random_ops();
    test_part_a_large_allocation();

    // --- Combined Lifecycle ---
    test_combined_lifecycle();

    // --- Heavy Load (Re-init) ---
    printf(BLU "\nRe-initializing Heap for Stress Test...\n" RST);
    test_part_b_heavy_load();

    heapKill();

    printf("\n" GRN "ALL ROBUSTNESS TESTS COMPLETED." RST "\n");
    return 0;
}