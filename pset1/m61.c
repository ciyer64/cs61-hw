#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

static struct m61_statistics stat61 = {
    0, 0, 0, 0, 0, 0, NULL, NULL
};

typedef struct meta61 {
    size_t size;
    char* pntr;
    struct meta61* prev;
    struct meta61* next;
} meta61;

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    size_t max_size = (size_t) -1;
    max_size -= sizeof(meta61);
    if (sz > max_size) {
        stat61.nfail++;
        stat61.fail_size += sz;
        return NULL;
    }
    if (sz==0) {
        return NULL;
    }
    //char* ptr = base_malloc(sz);
    meta61 *ptr = malloc(sz + sizeof(meta61));
    if (!ptr) {
        stat61.nfail++;
        stat61.fail_size += sz;
        return NULL;
    }
    else {
        stat61.nactive++;
        stat61.active_size += sz;
        stat61.ntotal++;
        stat61.total_size += sz;
        meta61 meta;		// declare struct?
        meta.size = sz;		// set size equal to size of memory to be stored
        memcpy(ptr,&meta, sizeof(meta61));		// copy to ptr the info from meta
        meta.pntr = (char*) (ptr + sizeof(meta61));	// sets stored pointer to point to stored data
        if (stat61.heap_min == NULL && stat61.heap_max == NULL) {	// if no max or min set
            stat61.heap_min = meta.pntr;				// set min
            stat61.heap_max = meta.pntr + sz;				// set max
        }
        else if (stat61.heap_min > meta.pntr) {				// if current min greater than ptr
            stat61.heap_min = meta.pntr;				// set min to ptr
        }
        else if (stat61.heap_max < meta.pntr + sz) {			// if current max less than ptr
            stat61.heap_max = meta.pntr + sz;				// set max to ptr
        }
    }
    // Your code here.
    return (void*) (ptr + sizeof(meta61));
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc and friends. If
///    `ptr == NULL`, does nothing. The free was called at location
///    `file`:`line`.

void m61_free(void *ptr, const char *file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    if (ptr == NULL) {
        return;
    }
    if (stat61.active_size == 0) {
        printf("MEMORY BUG %s:%d: invalid free of pointer %p, not in heap",file, line, ptr);
        abort();
    }
    stat61.nactive--;
    // base_free(ptr);

    meta61* mptr = ptr;
    meta61* meta = mptr - sizeof(meta61);
    stat61.active_size -= meta->size;
    free(meta);
}


/// m61_realloc(ptr, sz, file, line)
///    Reallocate the dynamic memory pointed to by `ptr` to hold at least
///    `sz` bytes, returning a pointer to the new block. If `ptr` is NULL,
///    behaves like `m61_malloc(sz, file, line)`. If `sz` is 0, behaves
///    like `m61_free(ptr, file, line)`. The allocation request was at
///    location `file`:`line`.

void* m61_realloc(void* ptr, size_t sz, const char* file, int line) {
    meta61* new_ptr = NULL;
    if (sz)
        new_ptr = m61_malloc(sz, file, line);
    if (ptr && new_ptr) {
	
        // Copy the data from `ptr` into `new_ptr`.
        // To do that, we must figure out the size of allocation `ptr`.
        // Your code here (to fix test012).
        meta61 *mptr = ptr;
        meta61 *meta = mptr - sizeof(meta61);	// "recreates" the struct so it can be used here
        meta61 new_meta;
        size_t old_sz = meta -> size;
        if (old_sz < sz)
            memcpy(new_ptr, ptr, old_sz);
        else
            memcpy(new_ptr, ptr, sz);

        //new_meta.size = meta->size;
        //new_ptr -= sizeof(meta61);
        //memcpy(new_ptr,&new_meta,sizeof(meta61));
        //new_ptr += sizeof(meta61);
    }
    m61_free(ptr, file, line);
    return (void*) new_ptr;
}


/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. The memory
///    is initialized to zero. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, int line) {
    // Your code here (to fix test014).
    size_t total_sz = nmemb*sz;
    void* ptr = NULL;
    if (total_sz >= nmemb) {
        ptr = m61_malloc(nmemb * sz, file, line);
    }
    if (ptr) {
        memset(ptr, 0, nmemb * sz);
    }
    else {
        stat61.nfail++;
        stat61.fail_size += sz;
    }
    return ptr;
}


/// m61_getstatistics(stats)
///    Store the current memory statistics in `*stats`.

void m61_getstatistics(struct m61_statistics* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(struct m61_statistics));
    // Your code here.
    *stats = stat61;
}


/// m61_printstatistics()
///    Print the current memory statistics.

void m61_printstatistics(void) {
    struct m61_statistics stats;
    m61_getstatistics(&stats);

    printf("malloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("malloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_printleakreport()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_printleakreport(void) {
    // Your code here.
}
