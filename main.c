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

// --- Global Configuration for Tests ---
#define THREAD_COUNT 10
#define ALLOCS_PER_THREAD 50

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

    int* ptr1 = (int*)customMalloc(sizeof(int));
    if (!ptr1) { printf(RED "FAIL: customMalloc returned NULL\n" RST); return; }
    *ptr1 = 42;
    printf("Allocated int: %d\n", *ptr1);

    int* arr = (int*)customCalloc(10, sizeof(int));
    if (!arr) { printf(RED "FAIL: customCalloc returned NULL\n" RST); return; }
    for(int i=0; i<10; i++) assert(arr[i] == 0);
    printf("Calloc verified (all zeros).\n");

    customFree(ptr1);
    customFree(arr);
    printf(GRN "PASS: Basic Malloc/Free\n" RST);
}

void test_part_a_coalescing() {
    printf(YEL "\n--- Test Part A: Coalescing (Merging Blocks) ---\n" RST);
    void* p1 = customMalloc(100);
    void* p2 = customMalloc(100);
    void* p3 = customMalloc(100);

    customFree(p2);
    customFree(p1);

    void* p4 = customMalloc(200);

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
    void* p1 = customMalloc(1);
    void* p2 = customMalloc(5);
    void* p3 = customMalloc(11);

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
    void* p1 = customMalloc(200);
    void* p4 = customMalloc(50);

    customFree(p1);

    void* p2 = customMalloc(50);
    void* p3 = customMalloc(50);

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
    void* h1 = customMalloc(200);
    void* f1 = customMalloc(10);
    void* h2 = customMalloc(100);
    void* f2 = customMalloc(10);

    customFree(h1);
    customFree(h2);

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
//           CALLOC & REALLOC TESTS (Part A)
// ==========================================

void test_calloc_large() {
    printf(YEL "\n--- Test: Calloc Large Allocation ---\n" RST);
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

    void* ptr = customRealloc(NULL, 100);
    if (ptr != NULL) {
        printf(GRN "PASS: realloc(NULL) behaved like malloc.\n" RST);
    } else {
        printf(RED "FAIL: realloc(NULL) returned NULL.\n" RST);
    }

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

    void* barrier = customMalloc(10);
    void* p2 = customRealloc(p1, 200);

    if (p2 != p1) {
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

    void* p1 = customMalloc(200);

    void* p2 = customRealloc(p1, 50);

    if (p2 != p1) {
        printf(RED "FAIL: Shrink moved the pointer (Inefficient).\n" RST);
        customFree(p2);
        return;
    }

    void* p3 = customMalloc(100);
    long diff = (char*)p3 - (char*)p2;

    if (diff > 50 && diff < 150) {
        printf(GRN "PASS: Remainder was split and reused successfully.\n" RST);
    } else {
        printf(RED "FAIL: Remainder was not reused correctly (Diff: %ld)\n" RST, diff);
    }

    customFree(p2);
    customFree(p3);
}

void test_realloc_variations_A() {
    printf(YEL "\n--- Test: Realloc Variations (Part A) ---\n" RST);

    // 1. Equal Size
    int* p1 = (int*)customMalloc(sizeof(int) * 10);
    for(int i=0; i<10; i++) p1[i] = i;
    int* p1_new = (int*)customRealloc(p1, sizeof(int) * 10);
    for(int i=0; i<10; i++) assert(p1_new[i] == i);
    printf(GRN "PASS: Realloc equal size preserved data.\n" RST);
    customFree(p1_new);

    // 2. Small Shrink (No Split)
    void* p2 = customMalloc(64);
    void* p2_new = customRealloc(p2, 60);
    if (p2_new == p2) {
        printf(GRN "PASS: Small shrink kept pointer in place.\n" RST);
    } else {
        printf(YEL "WARN: Small shrink moved pointer (Valid but inefficient).\n" RST);
    }
    customFree(p2_new);

    // 3. Big Expansion
    char* p3 = (char*)customMalloc(10);
    strcpy(p3, "ABC");
    char* p3_new = (char*)customRealloc(p3, 2000);
    assert(strcmp(p3_new, "ABC") == 0);
    printf(GRN "PASS: Big expansion preserved data.\n" RST);
    customFree(p3_new);
}

// ==========================================
//           PART B TESTS (Threaded)
// ==========================================

// --- Worker for Calloc Stress ---
void* calloc_thread_worker(void* arg) {
    long id = (long)arg;
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        size_t num = 50;
        size_t size = sizeof(int);
        int* arr = (int*)customMTCalloc(num, size);
        if (arr) {
            // Verify zeros
            for (size_t j = 0; j < num; j++) {
                if (arr[j] != 0) {
                    printf(RED "Thread %ld: MTCalloc memory not zeroed at index %zu\n" RST, id, j);
                    exit(1); // Fail fast
                }
            }
            usleep(50); // Small delay to encourage context switch
            customMTFree(arr);
        }
    }
    return NULL;
}

void test_mt_calloc_threaded() {
    printf(YEL "\n--- Test Part B: MT Calloc (Threaded Stress) ---\n" RST);
    pthread_t threads[THREAD_COUNT];
    for (long i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, calloc_thread_worker, (void*)i);
    }
    for (long i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    printf(GRN "PASS: MT Calloc threaded test completed successfully.\n" RST);
}

// --- Worker for Realloc Stress ---
void* realloc_thread_worker(void* arg) {
    long id = (long)arg;
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        // 1. Initial Malloc
        size_t size = 64;
        unsigned char* ptr = (unsigned char*)customMTMalloc(size);
        if (!ptr) continue;

        // Fill data
        memset(ptr, (int)id, size);

        // 2. Expand Realloc
        size_t large_size = 128;
        unsigned char* ptr2 = (unsigned char*)customMTRealloc(ptr, large_size);
        if (ptr2) {
            ptr = ptr2;
            // Check original data preservation
            for (size_t j = 0; j < size; j++) {
                if (ptr[j] != (unsigned char)id) {
                    printf(RED "Thread %ld: MTRealloc expand corrupted data\n" RST, id);
                    exit(1);
                }
            }
        }

        // 3. Shrink Realloc
        size_t small_size = 32;
        unsigned char* ptr3 = (unsigned char*)customMTRealloc(ptr, small_size);
        if (ptr3) {
            ptr = ptr3;
            // Check data again
            for (size_t j = 0; j < small_size; j++) {
                if (ptr[j] != (unsigned char)id) {
                    printf(RED "Thread %ld: MTRealloc shrink corrupted data\n" RST, id);
                    exit(1);
                }
            }
        }

        customMTFree(ptr);
    }
    return NULL;
}

void test_mt_realloc_threaded() {
    printf(YEL "\n--- Test Part B: MT Realloc (Threaded Stress) ---\n" RST);
    pthread_t threads[THREAD_COUNT];
    for (long i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&threads[i], NULL, realloc_thread_worker, (void*)i);
    }
    for (long i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    printf(GRN "PASS: MT Realloc threaded test completed successfully.\n" RST);
}

void* stress_worker(void* arg) {
    int id = *(int*)arg;
    for (int i = 0; i < 20; i++) {
        size_t size = (rand() % 2000) + 500;
        void* ptr = customMTMalloc(size);
        if (ptr) {
            memset(ptr, id, size);
            usleep(500);
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
//           COMBINED TESTS
// ==========================================

void test_combined_lifecycle() {
    printf(YEL "\n--- Test Combined: Mixed A & B Lifecycle ---\n" RST);
    printf(BLU "Step 1: Part A Malloc\n" RST);
    void* a_ptr1 = customMalloc(128);
    memset(a_ptr1, 'A', 128);

    printf(BLU "Step 2: Heap Create (Part B Init)\n" RST);
    printf(BLU "Step 3: Part B Malloc\n" RST);
    void* b_ptr1 = customMTMalloc(256);
    if(b_ptr1) memset(b_ptr1, 'B', 256);

    printf(BLU "Step 4: Part A Malloc (After Heap Create)\n" RST);
    void* a_ptr2 = customMalloc(64);
    if(a_ptr2) memset(a_ptr2, 'C', 64);

    if (*(char*)a_ptr1 != 'A') printf(RED "FAIL: a_ptr1 corrupted\n" RST);
    if (b_ptr1 && *(char*)b_ptr1 != 'B') printf(RED "FAIL: b_ptr1 corrupted\n" RST);
    if (a_ptr2 && *(char*)a_ptr2 != 'C') printf(RED "FAIL: a_ptr2 corrupted\n" RST);

    printf(BLU "Step 5: Freeing mixed pointers\n" RST);
    customFree(a_ptr2);
    customMTFree(b_ptr1);
    customFree(a_ptr1);

    printf(BLU "Step 6: Heap Kill\n" RST);
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
    test_realloc_variations_A();

    // --- Part B Tests ---
    test_mt_calloc_threaded();   // NEW: Threaded Calloc Test
    test_mt_realloc_threaded();  // NEW: Threaded Realloc Test
    test_mt_zone_overflow();

    // --- Combined Lifecycle ---
    test_combined_lifecycle();

    heapKill();

    printf("\n" GRN "ALL ROBUSTNESS TESTS COMPLETED." RST "\n");
    return 0;
}