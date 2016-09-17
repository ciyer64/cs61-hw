#define M61_DISABLE 1
#include "m61.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#define FREE ((char*) 0xF213E3ED)
#define ALLOC ((char*) 0xA110C47D)
#define ENDCHECK ((char*) 0xBACCCCCC)

static struct m61_statistics stat61 = {
    0, 0, 0, 0, 0, 0, NULL, NULL
};

typedef struct meta61 {
    size_t size;
    char* pntr;
    struct meta61* prev;
    struct meta61* next;
    char* state;
    const char* file;
    int line;
} meta61;

typedef struct bookend {
	char* foot;
} bookend;

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
	meta61* tmp3 = head;
	while (tmp3) {
		if (tmp3 == n) {
			meta61* nxt = n->next;
			meta61* prv = n->prev;
			if (nxt)
				nxt->prev = prv;
			if (prv)
				prv->next = nxt;
			break;
		}
		tmp3 = tmp3->next;
	}
}

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc may
///    either return NULL or a unique, newly-allocated pointer value.
///    The allocation request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, int line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    // Your code here.
    size_t max_size = sz + sizeof(meta61) + sizeof(bookend);
    if (sz > max_size) { 
		// checks size overflow
        stat61.nfail++;
        stat61.fail_size += sz;
        return NULL;
    }
    if (sz==0) {
        return NULL;
    }
    meta61 *ptr = base_malloc(sz + sizeof(meta61) + sizeof(bookend)); // allocate extra space
    if (!ptr) { 
		//checks failed allocation
        stat61.nfail++;
        stat61.fail_size += sz;
        return ptr;
    }
    //set metadata
    ptr->size = sz;		// set size equal to size of memory to be stored
    ptr->pntr = (char*) (ptr + 1);	// sets stored pointer to point to stored data
    ptr->state = ALLOC;
    ptr->file = file;
    ptr->line = line;

    //set footer
    bookend *end = (bookend*) (ptr->pntr + ptr->size);
    end->foot = ENDCHECK;

	//set stats
	stat61.nactive++;
	stat61.active_size += sz;
	stat61.ntotal++;
	stat61.total_size += sz;


    //*** add to linked list with insert_head function defined above ***
    insert_head(ptr);
	// if no max or min, set them
    if (stat61.heap_min == NULL && stat61.heap_max == NULL) {
        stat61.heap_min = ptr->pntr;
        stat61.heap_max = ptr->pntr + sz;
    }
	// if current min greater than ptr, set min to ptr
    else if (stat61.heap_min > ptr->pntr) {
        stat61.heap_min = ptr->pntr;
    }
	// if current max less than ptr, set max to ptr
    else if (stat61.heap_max < ptr->pntr + sz) {
        stat61.heap_max = ptr->pntr + sz;
    }
    return (void*) (ptr->pntr);
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
	// check if the ptr falls outside the heap
    if ((char*) ptr > stat61.heap_max || (char*) ptr < stat61.heap_min) {
        printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not in heap\n",file, line, ptr);
        abort();
    }
	// allocate ptr to the metadata
	meta61* mptr = (meta61*) ptr - 1;

    meta61* tmp1 = head;
    int isAlloc=0;
    while (tmp1) {
        if (tmp1->pntr == ptr) {
            isAlloc=1;
            break;
        }
        tmp1 = tmp1->next;
    }
	// check for misallocation
    if (!mptr || mptr->state != ALLOC || !isAlloc) {
        printf("MEMORY BUG: %s:%d: invalid free of pointer %p, not allocated\n",file, line, ptr);
        meta61* tmp2 = head;
        while (tmp2) {
            if ((tmp2->pntr < ((char*) ptr)) && (((char*) ptr) < (tmp2->pntr + tmp2->size))) {
                size_t offset = (size_t) ptr - (size_t) tmp2->pntr;
                printf("  %s:%d: %p is %zu bytes inside a %zu byte region allocated here\n",file, line-1, ptr,offset,tmp2->size);
                abort();
            }
            tmp2 = tmp2->next;
        abort();
        }      
    }
	// check for double frees
    if (mptr->state == FREE) {
        printf("MEMORY BUG: %s:%d: invalid free of pointer %p\n",file, line, ptr);
        abort();
    }

	if (mptr->next && (mptr->next)->prev != mptr) {
		printf("MEMORY BUG: %s:%d: %zu free of pointer %p\n",file, line, mptr->size, ptr);
		abort();
	}

    bookend* endcap = (bookend*) ((char*) ptr + mptr->size);

    if (endcap->foot != ENDCHECK) {
        printf("MEMORY BUG: %s:%d: detected wild write during free of pointer %p\n",file, line, ptr);
        abort();
    }
	// update stats
    stat61.nactive--;
    stat61.active_size -= mptr->size;
    mptr->state = FREE;
    remove_node(mptr);
    base_free(mptr);
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
        meta61 *meta = ((meta61*) ptr) - 1;	// "recreates" the struct so it can be used here
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
    //memset(stats, 255, sizeof(struct m61_statistics));
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
    if (head != NULL){
        meta61* tmp = head;
        while (tmp != NULL) {
            if (tmp->state == ALLOC) {
		printf("LEAK CHECK: %s:%d: allocated object %p with size %zu\n", tmp->file, tmp->line, tmp->pntr,tmp->size);
            }
	    tmp=tmp->next;
        }
    }
}
