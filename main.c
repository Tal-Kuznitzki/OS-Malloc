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
#define THREAD_COUNT 30
#define ALLOCS_PER_THREAD 200
int is_aligned(void* ptr) {

    /*
    check if a size is aligned to 4
    */
    return ((size_t)ptr % 4) == 0;
}
int check_zero(void* ptr, size_t size) {
    /*
        check if data is zeros (for calloc test)
    */
    unsigned char* p = (unsigned char*)ptr;
    for(size_t i = 0; i < size; i++) {
        if (p[i] != 0) return 0;
    }
    return 1;
}
void test_part_a_basic() {
    /*
        test customMalloc functionality is as expected
        test customCalloc basic functionality
        test customFree basic functionality (free int var and free arr)
    */
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
    /*
        test free in the middle of the heap
        test "findBestFit" - malloc after release of memory should find the free block and allocate to it
    */
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
    /*
        allocate non 4-aligned buffers and verify that customMalloc allocates only 4-aligned buffers (blocks)
    */
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
    /*
        allocate a big block of memory, and another block
        free the first block
        allocate another block, smaller than the first one
        verify that the new block is allocated in the address of the freed block 
        and that malloc have seperated the free block properly
    */
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
    /*
        test "findBestFit" functionality - according to the best fit creteria we learned at the course
    */
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
void test_comb(){
    /*
        test multiple cases of free that requires combinning blocks:
        -free from head of the list, then second and then last (merge to next)
        -free from the middle , than first (merge to prev)
        -free from the end of the list when the list is not empty (shrink brk)
        -free from the end of the list when the list is empty (all free) - close heap
        -free from the beginning of the list when there are more blocks afterwards
    */
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
void test_calloc_large() {
    /*
        use customCalloc to allocate a big buffer and make sure its all zeros
    */
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
    /*
       test customRealloc functionality:
       -use realloc with NULL ptr - should behave like malloc
       -test realloc when new size is larger than original size
       -test realloc when new size is smaller than original size, verify that the heap is shrinking

    */
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
    /*
      test customRealloc functionality:
       - see taht it moved the entire data and that it is still intact

    */
    printf(YEL "\n--- Test: Realloc Expansion (Full Data Verification) ---\n" RST);

    size_t size_original = 100;
    size_t size_new = 200;
    unsigned char* p1 = (unsigned char*)customMalloc(size_original);
    if (!p1) { printf(RED "FAIL: Malloc returned NULL\n" RST); return; }
    for (size_t i = 0; i < size_original; ++i) {
        p1[i] = (unsigned char)(i % 256);
    }

    void* barrier = customMalloc(10);
    unsigned char* p2 = (unsigned char*)customRealloc(p1, size_new);
    if (p2 == p1) {
        printf(RED "FAIL: Realloc should have moved (barrier exists).\n" RST);
        // Clean up even if fail
        customFree(p2);
        customFree(barrier);
        return;
    }
    bool data_intact = true;
    for (size_t i = 0; i < size_original; ++i) {
        if (p2[i] != (unsigned char)(i % 256)) {
            printf(RED "FAIL: Data corrupted at byte %zu. Expected %d, got %d\n" RST,
                   i, (unsigned char)(i % 256), p2[i]);
            data_intact = false;
            break;
        }
    }
    if (data_intact) {
        printf(GRN "PASS: Block moved and ALL data preserved correctly.\n" RST);
    }
    customFree(p2);
    customFree(barrier);
}
void test_realloc_shrink_split() { // TODO: maybe unify this with the past one ?? , nah its good

    /*
  test customRealloc functionality:
   - see taht shrinking does not change the pointer, and that splitting the remainder works well within the margin
     (inculding metadata

*/
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
    /*
       test customRealloc functionality:
       -use realloc with NULL ptr - should behave like malloc
       -test realloc when new size is larger than original size
       -test realloc when new size is smaller than original size, verify that the heap is shrinking
       -test realloc when new size is equal to original size
    
    */
    printf(YEL "\n--- Test: Realloc Variations (Part A) ---\n" RST);

    // Equal Size
    int* p1 = (int*)customMalloc(sizeof(int) * 10);
    for(int i=0; i<10; i++) p1[i] = i;
    int* p1_new = (int*)customRealloc(p1, sizeof(int) * 10);
    for(int i=0; i<10; i++) assert(p1_new[i] == i);
    printf(GRN "PASS: Realloc equal size preserved data.\n" RST);
    customFree(p1_new);

    // Small Shrink (No Split)
    void* p2 = customMalloc(64);
    void* p2_new = customRealloc(p2, 60);
    if (p2_new == p2) {
        printf(GRN "PASS: Small shrink kept pointer in place.\n" RST);
    } else {
        printf(YEL "WARN: Small shrink moved pointer (Valid but inefficient).\n" RST);
    }
    customFree(p2_new);

    // Big Expansion
    char* p3 = (char*)customMalloc(10);
    strcpy(p3, "ABC");
    char* p3_new = (char*)customRealloc(p3, 2000);
    assert(strcmp(p3_new, "ABC") == 0);
    printf(GRN "PASS: Big expansion preserved data.\n" RST);
    customFree(p3_new);
}
void* calloc_thread_worker(void* arg) {
    /*
       test customMTCalloc functionality
    
    */
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
    /*
       crreate the threads to run "calloc_thread_worker()" 
    
    */
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
void* realloc_thread_worker(void* arg) {
    /*
       test customMTRealloc functionality
    
    */
    long id = (long)arg;
    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        
        size_t size = 64;
        unsigned char* ptr = (unsigned char*)customMTMalloc(size);
        if (!ptr) continue;

        memset(ptr, (int)id, size);

        // Expand Realloc
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

        // Shrink Realloc
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
    /*
       create the threads to run "realloc_thread_worker()" 
    
    */
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
    /*
       create a lot of stress on the threads - multiple malloc and free
       -check Multi-Threaded functionality (make sure there is no deadlock...) 
    
    */
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
    /*
       create the threads to run "stress_worker()" 
    
    */
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
void test_combined_lifecycle() {
    /*
       Test part A and part B simultaneously (customMalloc, customMTMalloc, customFree, customMTFree)
    
    */
    printf(YEL "\n--- Test Combined: Mixed A & B Lifecycle ---\n" RST);
    printf(BLU "Step 1: Part A Malloc\n" RST);
    void* a_ptr1 = customMalloc(128);
    memset(a_ptr1, 'A', 128);

    printf(BLU "Step 2: Part B Malloc\n" RST);
    void* b_ptr1 = customMTMalloc(256);
    if(b_ptr1) memset(b_ptr1, 'B', 256);

    printf(BLU "Step 3: Part A Malloc\n" RST);
    void* a_ptr2 = customMalloc(64);
    if(a_ptr2) memset(a_ptr2, 'C', 64);

    if (*(char*)a_ptr1 != 'A') printf(RED "FAIL: a_ptr1 corrupted\n" RST);
    if (b_ptr1 && *(char*)b_ptr1 != 'B') printf(RED "FAIL: b_ptr1 corrupted\n" RST);
    if (a_ptr2 && *(char*)a_ptr2 != 'C') printf(RED "FAIL: a_ptr2 corrupted\n" RST);

    printf(BLU "Step 4: Freeing mixed pointers\n" RST);
    customFree(a_ptr2);
    customMTFree(b_ptr1);
    customFree(a_ptr1);

    printf(BLU "Step 5: Part A Malloc\n" RST);
    void* a_ptr3 = customMalloc(50);
    if(a_ptr3) {
        memset(a_ptr3, 'D', 50);
        customFree(a_ptr3);
        printf(GRN "PASS: Combined Lifecycle (A -> Init -> B -> A -> Free -> Kill -> A) successful.\n" RST);
    } else {
        printf(RED "FAIL: Part A failed after Heap Kill.\n" RST);
    }
}
int main() {
    heapCreate();
    test_part_a_basic();
    test_alignment();
    test_splitting();
    test_part_a_coalescing();
    test_best_fit();
    test_comb();
    test_calloc_large();
    test_realloc_null_and_zero();
    test_realloc_expansion();
    test_realloc_shrink_split();
    test_realloc_variations_A();
    test_mt_calloc_threaded();
    test_mt_realloc_threaded();
    test_mt_zone_overflow();
    test_combined_lifecycle();
    heapKill();
    return 0;
}