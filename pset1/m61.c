#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#define FREE ((char*) 0xF213E3ED)
#define ALLOC ((char*) 0xA110C47D)

static struct m61_statistics stat61 = {
    0, 0, 0, 0, 0, 0, NULL, NULL
};

typedef struct meta61 {
    size_t size;
    char* pntr;
    struct meta61* prev;
    struct meta61* next;
    char* state;
} meta61;


meta61* head = NULL;

// from C patterns page of cs61 WIKI
void insert_head(meta61* n) {
    n->next = head;
    n->prev = NULL;
    if (head)
        head->prev = n;
    head = n;
}

void remove_node(meta61* n) {
    if (n->next)
        n->next->prev = n->prev;
    if (n->prev)
        n->prev->next = n->next;
    else
        head = n->next;
}

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

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
    stat61.nactive++;
    stat61.active_size += sz;
    stat61.ntotal++;
    stat61.total_size += sz;

    ptr->size = sz;		// set size equal to size of memory to be stored
    ptr->pntr = (char*) (ptr + sizeof(meta61));	// sets stored pointer to point to stored data
    ptr->state = ALLOC;

    //******* add to linked list with insert_head function defined above ******************
    insert_head(ptr);

    if (stat61.heap_min == NULL && stat61.heap_max == NULL) {	// if no max or min set
        stat61.heap_min = ptr->pntr;				// set min
        stat61.heap_max = ptr->pntr + sz;				// set max
    }
    else if (stat61.heap_min > ptr->pntr) {				// if current min greater than ptr
        stat61.heap_min = ptr->pntr;				// set min to ptr
    }
    else if (stat61.heap_max < ptr->pntr + sz) {			// if current max less than ptr
        stat61.heap_max = ptr->pntr + sz;				// set max to ptr
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
    if ((char*) ptr > stat61.heap_max || (char*) ptr < stat61.heap_min) {
        printf("MEMORY BUG %s:%d: invalid free of pointer %p, not in heap\n",file, line, ptr);
        abort();
    }
    // if (mptr->state != ALLOC) {

        // case: specifically, user ptr within allocation

        /*
        meta61* tmp = head;

        while(tmp != NULL){

            //tmp stuff

            tmp = tmp->next;
        }

        [ meta     |usr pointer   [evil pointer]    user alloc stuff (sz sized) ]
        (char*)evil pointer - (user ptr)


        IF WITHIN REGION PRINT SECOND LINE OF ERROR ... 100 bytes into 2001 byte rgion etc..
        IF NOT IN ANY REGION GIVE JUST THE ONE LINE WARNING

        */

    //     printf("MEMORY BUG %s:%d: invalid free of pointer %p, not allocated\n",file, line, ptr);




    //     abort();
    // }
    meta61* mptr = (meta61*) ptr - sizeof(meta61);
    if (mptr->state == FREE) {
        printf("MEMORY BUG %s:%d: invalid free of pointer %p\n",file, line, ptr);
        abort();
    }
    if (mptr->state != ALLOC) {
        meta61* tmp = head;
        while (tmp != NULL){
            if (tmp == ptr) {
                size_t offset = ((size_t) ptr - (size_t) tmp) - sizeof(meta61);
                printf("MEMORY BUG %s:%d: invalid free of pointer %p, not allocated\n  %s:%d: %p: %p is %d bytes inside a %zu byte region allocated here",file, line, ptr,file,line,tmp,ptr,offset,tmp->size);
                abort();
            }
            tmp = tmp->next;
        printf("MEMORY BUG %s:%d: invalid free of pointer %p, not allocated\n",file, line, ptr);
        abort();
        }
        // printf("MEMORY BUG %s:%d: invalid free of pointer %p, not allocated\n",file, line, ptr);        
        abort();
    }
    stat61.nactive--;
    stat61.active_size -= mptr->size;
    mptr->state = FREE;
    base_free(mptr);
    // base_free(ptr);
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
        meta61 *meta = (meta61*) ptr - sizeof(meta61);	// "recreates" the struct so it can be used here
        size_t old_sz = meta -> size;
        if (old_sz < sz)
            memcpy(new_ptr, ptr, old_sz);
        else
            memcpy(new_ptr, ptr, sz);

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
    if (total_sz >= nmemb) { //ensure no overflow
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
