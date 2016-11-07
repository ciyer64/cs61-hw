#include "kernel.h"
#include "lib.h"

// kernel.c
//
//    This is the kernel.


// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000       // initial state only

static proc processes[NPROC];   // array of process descriptors
                                // Note that `processes[0]` is never used.
proc* current;                  // pointer to currently executing proc

#define HZ 100                  // timer interrupt frequency (interrupts/sec)
static unsigned ticks;          // # timer interrupts so far

void schedule(void);
void run(proc* p) __attribute__((noreturn));


// PAGEINFO
//
//    The pageinfo[] array keeps track of information about each physical page.
//    There is one entry per physical page.
//    `pageinfo[pn]` holds the information for physical page number `pn`.
//    You can get a physical page number from a physical address `pa` using
//    `PAGENUMBER(pa)`. (This also works for page table entries.)
//    To change a physical page number `pn` into a physical address, use
//    `PAGEADDRESS(pn)`.
//
//    pageinfo[pn].refcount is the number of times physical page `pn` is
//      currently referenced. 0 means it's free.
//    pageinfo[pn].owner is a constant indicating who owns the page.
//      PO_KERNEL means the kernel, PO_RESERVED means reserved memory (such
//      as the console), and a number >=0 means that process ID.
//
//    pageinfo_init() sets up the initial pageinfo[] state.

typedef struct physical_pageinfo {
    int8_t owner;
    int8_t refcount;
} physical_pageinfo;

static physical_pageinfo pageinfo[PAGENUMBER(MEMSIZE_PHYSICAL)];

typedef enum pageowner {
    PO_FREE = 0,                // this page is free
    PO_RESERVED = -1,           // this page is reserved memory
    PO_KERNEL = -2              // this page is used by the kernel
} pageowner_t;

static void pageinfo_init(void);


// Memory functions

void check_virtual_memory(void);
void memshow_physical(void);
void memshow_virtual(x86_64_pagetable* pagetable, const char* name);
void memshow_virtual_animate(void);


// kernel(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, int program_number);

int8_t owner_global = 0;

void kernel(const char* command) {
    hardware_init();
    pageinfo_init();
    console_clear();
    timer_init(HZ);


/*****************************Code written by Frank (Frank 1/3)************************/
    // Set the permissions for kernel memory
    //virtual_memory_map(kernel_pagetable, 0x0, 0x0, 
	//(uintptr_t) console, PTE_P | PTE_W, NULL);
    //virtual_memory_map(kernel_pagetable, (0xB8000+PAGESIZE), (0xB8000+PAGESIZE), 
    //    (PROC_START_ADDR-0xB8000-PAGESIZE), PTE_P | PTE_W, NULL);
    //virtual_memory_map(kernel_pagetable, 0xB8000, 0xB8000, 
	//PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);
/**********************end of code written by Frank************************/

    // Set up process descriptors
    memset(processes, 0, sizeof(processes));
    for (pid_t i = 0; i < NPROC; i++) {
        processes[i].p_pid = i;
        processes[i].p_state = P_FREE;
    }

    if (command && strcmp(command, "fork") == 0)
        process_setup(1, 4);
    else if (command && strcmp(command, "forkexit") == 0)
        process_setup(1, 5);
    else
        for (pid_t i = 1; i <= 4; ++i)
            process_setup(i, i - 1);
	
	virtual_memory_map(kernel_pagetable, 0, 0, PROC_START_ADDR,
		PTE_P | PTE_W, NULL);
	virtual_memory_map(kernel_pagetable, (uintptr_t) console, (uintptr_t) console,
		PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);	
	
	/*
	virtual_memory_map(kernel_pagetable, 0x0, 0x0, 
		(uintptr_t) console, PTE_P | PTE_W, NULL);
    virtual_memory_map(kernel_pagetable, (0xB8000+PAGESIZE), (0xB8000+PAGESIZE), 
        (PROC_START_ADDR-0xB8000-PAGESIZE), PTE_P | PTE_W, NULL);
    virtual_memory_map(kernel_pagetable, 0xB8000, 0xB8000, 
		PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);
	*/
    // Switch to the first process using run()
    run(&processes[1]);
}

// find a free page to use as destination using pageinfo array
x86_64_pagetable* p_allocator() {
    int pn = 0;   // page number incrementer

	// iterate through physical pages until find one w/ refcount == 0
    while (pageinfo[pn].refcount != 0 && pageinfo[pn].owner != PO_FREE) {
		// if no free page to be found
		if (pn > NPAGES) {
	    	return (x86_64_pagetable*) -1;
		}
		pn++;
    }
    x86_64_pagetable* addr = (x86_64_pagetable*) PAGEADDRESS(pn);

    int n = assign_physical_page((uintptr_t) addr, (uint8_t) owner_global);
	
    if (n != 0) {
		return (x86_64_pagetable*) -1;
    }

    memset(addr, 0, PAGESIZE);
    return addr;
}

/**************************Code written by Frank (Frank 2/3)***********************/
x86_64_pagetable* copy_pagetable(x86_64_pagetable* pagetable, int8_t owner) {
	owner_global = owner;
    // find free pagetable to copy kernel pagetable into
    x86_64_pagetable* free_page = p_allocator();

    if ((intptr_t) free_page == -1)
		return NULL;

	// copy pre-process data from kernel pagetable
    for (int i = 0x0; i < PROC_START_ADDR; i += PAGESIZE) {

		// see kernel.h for details on virtual_memory_lookup
		vamapping virt_lookup = virtual_memory_lookup(pagetable, i);

		// map the virtual memory (from lookup above) into 
		// the free physical page (from allocator)
		virtual_memory_map(free_page, i, virt_lookup.pa, PAGESIZE, 
			PTE_P | PTE_W, p_allocator);
    }

    // return the allocated and initialized page table
    return free_page;
}

int free_mem(proc* cur) {
	for (uintptr_t pn = 0; pn < PAGENUMBER(MEMSIZE_PHYSICAL); pn++) {
		if (pageinfo[pn].owner == cur->p_pid) {
			// reset ownership to free
			pageinfo[pn].refcount--;
			if (pageinfo[pn].refcount == 0) {
				pageinfo[pn].owner = PO_FREE;
			}
		}
		// in cases where it is referred but not owned by another
		else if (pageinfo[pn].refcount > 1 && pageinfo[pn].owner > 0) {
			pageinfo[pn].refcount--;
		}
	}
	// set process state to free
	processes[cur->p_pid].p_state = P_FREE;
	return 0;
}

// process_setup(pid, program_number)
//    Load application program `program_number` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %rip and %rsp, gives it a stack page, and marks it as runnable.

void process_setup(pid_t pid, int program_number) {

    process_init(&processes[pid], 0);
	// copy kernel pagetable
    processes[pid].p_pagetable = copy_pagetable(kernel_pagetable, pid);

	virtual_memory_map(processes[pid].p_pagetable, PROC_START_ADDR, PROC_START_ADDR,
		MEMSIZE_PHYSICAL - PROC_START_ADDR, 0, NULL);
	virtual_memory_map(processes[pid].p_pagetable, (uintptr_t) console, (uintptr_t) console,
		PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);

    int r = program_load(&processes[pid], program_number, NULL);
    assert(r >= 0);
    //processes[pid].p_registers.reg_rsp = PROC_START_ADDR + PROC_SIZE * pid;
    processes[pid].p_registers.reg_rsp = MEMSIZE_VIRTUAL;
    //uintptr_t stack_page = processes[pid].p_registers.reg_rsp - PAGESIZE;
	uintptr_t stack_page = (uintptr_t) p_allocator();
    //assign_physical_page(stack_page, pid);
    virtual_memory_map(processes[pid].p_pagetable, 
		MEMSIZE_VIRTUAL - PAGESIZE, stack_page, PAGESIZE, 
		PTE_P | PTE_W | PTE_U, p_allocator);
    processes[pid].p_state = P_RUNNABLE;
}


/*******************end of code written by Frank************************/

// assign_physical_page(addr, owner)
//    Allocates the page with physical address `addr` to the given owner.
//    Fails if physical page `addr` was already allocated. Returns 0 on
//    success and -1 on failure. Used by the program loader.

int assign_physical_page(uintptr_t addr, int8_t owner) {
    if ((addr & 0xFFF) != 0
        || addr >= MEMSIZE_PHYSICAL
        || pageinfo[PAGENUMBER(addr)].refcount != 0)
        return -1;
    else {
        pageinfo[PAGENUMBER(addr)].refcount = 1;
        pageinfo[PAGENUMBER(addr)].owner = owner;
        return 0;
    }
}


// exception(reg)
//    Exception handler (for interrupts, traps, and faults).
//
//    The register values from exception time are stored in `reg`.
//    The processor responds to an exception by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-exception.S). That code saves more registers on the kernel's stack,
//    then calls exception().
//
//    Note that hardware interrupts are disabled whenever the kernel is running.

void exception(x86_64_registers* reg) {
    // Copy the saved registers into the `current` process descriptor
    // and always use the kernel's page table.
    current->p_registers = *reg;
    set_pagetable(kernel_pagetable);

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /*log_printf("proc %d: exception %d\n", current->p_pid, reg->reg_intno);*/

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor(cursorpos);
    if (reg->reg_intno != INT_PAGEFAULT || (reg->reg_err & PFERR_USER)) {
        check_virtual_memory();
        memshow_physical();
        memshow_virtual_animate();
    }

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the exception.
    switch (reg->reg_intno) {

    case INT_SYS_PANIC:
        panic(NULL);
        break;                  // will not be reached

    case INT_SYS_GETPID:
        current->p_registers.reg_rax = current->p_pid;
        break;

    case INT_SYS_YIELD:
        schedule();
        break;                  /* will not be reached */

    case INT_SYS_PAGE_ALLOC: {
		uintptr_t addr = current->p_registers.reg_rdi;
		owner_global = current->p_pid;
		intptr_t fpage = (intptr_t) p_allocator();
        if (fpage >= 0) {
            virtual_memory_map(current->p_pagetable, addr, fpage,
                PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);
            current->p_registers.reg_rax = 0;
		} 
		else {
			console_printf(CPOS(24,2), 0x0C00, "Out of physical memory!\n");
    		current->p_registers.reg_rax = -1;
        	break;
		}
    }

    case INT_TIMER:
        ++ticks;
        schedule();
        break;                  /* will not be reached */

    case INT_PAGEFAULT: {
        // Analyze faulting address and access type.
        uintptr_t addr = rcr2();
        const char* operation = reg->reg_err & PFERR_WRITE
                ? "write" : "read";
        const char* problem = reg->reg_err & PFERR_PRESENT
                ? "protection problem" : "missing page";

        if (!(reg->reg_err & PFERR_USER))
            panic("Kernel page fault for %p (%s %s, rip=%p)!\n",
                  addr, operation, problem, reg->reg_rip);
        console_printf(CPOS(24, 0), 0x0C00,
                       "Process %d page fault for %p (%s %s, rip=%p)!\n",
                       current->p_pid, addr, operation, problem, reg->reg_rip);
        current->p_state = P_BROKEN;
        break;
    }
	
	// exit() implementation
	case INT_SYS_EXIT: {
		free_mem(current);
		break;
	}

	// fork() implementation
	case INT_SYS_FORK: {
		// variable that will be -1 if fails
		// or denotes free process
		int pid_f = -1;

		// find the free process slot
		for (int i = 1; i < NPROC; i++) {
			if (processes[i].p_state == P_FREE) {
				pid_f = i;
				break;
			}
		}

		// initialize pointer to child processes
		if (pid_f != -1) {
			proc* child = &processes[pid_f];

			// copy parent process data to child
			child->p_pid = pid_f;
			child->p_registers = current->p_registers;
			child->p_state = P_RUNNABLE;

			// copy parent's page table
			child->p_pagetable = copy_pagetable(current->p_pagetable, child->p_pid);

			// error case if copy fails
			if (!child->p_pagetable) {
				free_mem(child);
				current->p_registers.reg_rax = -1;
				run(current);
				break;
			}

			// re-map the console to prevent blink
			virtual_memory_map(child->p_pagetable, (uintptr_t) console, (uintptr_t) console,
				PAGESIZE, PTE_P | PTE_W | PTE_U, NULL);

			// examine all virtual addresses in old page table
			for (uintptr_t va = PROC_START_ADDR; va < MEMSIZE_VIRTUAL; va += PAGESIZE) {
				vamapping vm = virtual_memory_lookup(current->p_pagetable, va);

				// check if the page at this address is application-writable
				if ((vm.perm & (PTE_P | PTE_W | PTE_U)) == (PTE_P | PTE_W | PTE_U)) {
					x86_64_pagetable* addr = p_allocator();

					// error check with allocation
					if ((intptr_t) addr == -1) {
						free_mem(child);
						current->p_registers.reg_rax = -1;
						run(current);
						return;
					}
					// copy data from parent's page
					memcpy((void*) addr, (void*) vm.pa, PAGESIZE);

					// map physical page at virt. addr. in child's page table
					virtual_memory_map(child->p_pagetable, va, (uintptr_t) addr, PAGESIZE, 
						PTE_P|PTE_W|PTE_U, p_allocator);
				}
				// track the number of active references to each page
				else if (pageinfo[vm.pn].refcount > 0 && pageinfo[vm.pn].owner > 0) {
					virtual_memory_map(child->p_pagetable, va, vm.pa, PAGESIZE, 
						PTE_P | PTE_U, p_allocator);
					pageinfo[vm.pn].refcount++;
				}
			}
			current->p_registers.reg_rax = child->p_pid;
			child->p_registers.reg_rax = 0;
			run(current);
			break;

		} else {
			// if no slot can be found
			current->p_registers.reg_rax = -1;
			//run(current);
			break;
		}
	}
	
    default:
        panic("Unexpected exception %d!\n", reg->reg_intno);
        break;                  /* will not be reached */

    }


    // Return to the current process (or run something else).
    if (current->p_state == P_RUNNABLE)
        run(current);
    else
        schedule();
}


// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule(void) {
    pid_t pid = current->p_pid;
    while (1) {
        pid = (pid + 1) % NPROC;
        if (processes[pid].p_state == P_RUNNABLE)
            run(&processes[pid]);
        // If Control-C was typed, exit the virtual machine.
        check_keyboard();
    }
}


// run(p)
//    Run process `p`. This means reloading all the registers from
//    `p->p_registers` using the `popal`, `popl`, and `iret` instructions.
//
//    As a side effect, sets `current = p`.

void run(proc* p) {
    assert(p->p_state == P_RUNNABLE);
    current = p;

    set_pagetable(p->p_pagetable);

    // This function is defined in k-exception.S. It restores the process's
    // registers then jumps back to user mode.
    exception_return(&p->p_registers);

 spinloop: goto spinloop;       // should never get here
}


// pageinfo_init
//    Initialize the `pageinfo[]` array.

void pageinfo_init(void) {
    extern char end[];

    for (uintptr_t addr = 0; addr < MEMSIZE_PHYSICAL; addr += PAGESIZE) {
        int owner;
        if (physical_memory_isreserved(addr))
            owner = PO_RESERVED;
        else if ((addr >= KERNEL_START_ADDR && addr < (uintptr_t) end)
                 || addr == KERNEL_STACK_TOP - PAGESIZE)
            owner = PO_KERNEL;
        else
            owner = PO_FREE;
        pageinfo[PAGENUMBER(addr)].owner = owner;
        pageinfo[PAGENUMBER(addr)].refcount = (owner != PO_FREE);
    }
}


// check_page_table_mappings
//    Check operating system invariants about kernel mappings for page
//    table `pt`. Panic if any of the invariants are false.

void check_page_table_mappings(x86_64_pagetable* pt) {
    extern char start_data[], end[];
    assert(PTE_ADDR(pt) == (uintptr_t) pt);

    // kernel memory is identity mapped; data is writable
    for (uintptr_t va = KERNEL_START_ADDR; va < (uintptr_t) end;
         va += PAGESIZE) {
        vamapping vam = virtual_memory_lookup(pt, va);
        if (vam.pa != va)
            console_printf(CPOS(22, 0), 0xC000, "%p vs %p\n", va, vam.pa);
        assert(vam.pa == va);
        if (va >= (uintptr_t) start_data)
            assert(vam.perm & PTE_W);
    }

    // kernel stack is identity mapped and writable
    uintptr_t kstack = KERNEL_STACK_TOP - PAGESIZE;
    vamapping vam = virtual_memory_lookup(pt, kstack);
    assert(vam.pa == kstack);
    assert(vam.perm & PTE_W);
}


// check_page_table_ownership
//    Check operating system invariants about ownership and reference
//    counts for page table `pt`. Panic if any of the invariants are false.

static void check_page_table_ownership_level(x86_64_pagetable* pt, int level,
                                             int owner, int refcount);

void check_page_table_ownership(x86_64_pagetable* pt, pid_t pid) {
    // calculate expected reference count for page tables
    int owner = pid;
    int expected_refcount = 1;
    if (pt == kernel_pagetable) {
        owner = PO_KERNEL;
        for (int xpid = 0; xpid < NPROC; ++xpid)
            if (processes[xpid].p_state != P_FREE
                && processes[xpid].p_pagetable == kernel_pagetable)
                ++expected_refcount;
    }
    check_page_table_ownership_level(pt, 0, owner, expected_refcount);
}

static void check_page_table_ownership_level(x86_64_pagetable* pt, int level,
                                             int owner, int refcount) {
    assert(PAGENUMBER(pt) < NPAGES);
    assert(pageinfo[PAGENUMBER(pt)].owner == owner);
    assert(pageinfo[PAGENUMBER(pt)].refcount == refcount);
    if (level < 3)
        for (int index = 0; index < NPAGETABLEENTRIES; ++index)
            if (pt->entry[index]) {
                x86_64_pagetable* nextpt =
                    (x86_64_pagetable*) PTE_ADDR(pt->entry[index]);
                check_page_table_ownership_level(nextpt, level + 1, owner, 1);
            }
}


// check_virtual_memory
//    Check operating system invariants about virtual memory. Panic if any
//    of the invariants are false.

void check_virtual_memory(void) {
    // Process 0 must never be used.
    assert(processes[0].p_state == P_FREE);

    // The kernel page table should be owned by the kernel;
    // its reference count should equal 1, plus the number of processes
    // that don't have their own page tables.
    // Active processes have their own page tables. A process page table
    // should be owned by that process and have reference count 1.
    // All level-2-4 page tables must have reference count 1.

    check_page_table_mappings(kernel_pagetable);
    check_page_table_ownership(kernel_pagetable, -1);

    for (int pid = 0; pid < NPROC; ++pid)
        if (processes[pid].p_state != P_FREE
            && processes[pid].p_pagetable != kernel_pagetable) {
            check_page_table_mappings(processes[pid].p_pagetable);
            check_page_table_ownership(processes[pid].p_pagetable, pid);
        }

    // Check that all referenced pages refer to active processes
    for (int pn = 0; pn < PAGENUMBER(MEMSIZE_PHYSICAL); ++pn)
        if (pageinfo[pn].refcount > 0 && pageinfo[pn].owner >= 0)
            assert(processes[pageinfo[pn].owner].p_state != P_FREE);
}


// memshow_physical
//    Draw a picture of physical memory on the CGA console.

static const uint16_t memstate_colors[] = {
    'K' | 0x0D00, 'R' | 0x0700, '.' | 0x0700, '1' | 0x0C00,
    '2' | 0x0A00, '3' | 0x0900, '4' | 0x0E00, '5' | 0x0F00,
    '6' | 0x0C00, '7' | 0x0A00, '8' | 0x0900, '9' | 0x0E00,
    'A' | 0x0F00, 'B' | 0x0C00, 'C' | 0x0A00, 'D' | 0x0900,
    'E' | 0x0E00, 'F' | 0x0F00
};

void memshow_physical(void) {
    console_printf(CPOS(0, 32), 0x0F00, "PHYSICAL MEMORY");
    for (int pn = 0; pn < PAGENUMBER(MEMSIZE_PHYSICAL); ++pn) {
        if (pn % 64 == 0)
            console_printf(CPOS(1 + pn / 64, 3), 0x0F00, "0x%06X ", pn << 12);

        int owner = pageinfo[pn].owner;
        if (pageinfo[pn].refcount == 0)
            owner = PO_FREE;
        uint16_t color = memstate_colors[owner - PO_KERNEL];
        // darker color for shared pages
        if (pageinfo[pn].refcount > 1)
            color &= 0x77FF;

        console[CPOS(1 + pn / 64, 12 + pn % 64)] = color;
    }
}


// memshow_virtual(pagetable, name)
//    Draw a picture of the virtual memory map `pagetable` (named `name`) on
//    the CGA console.

void memshow_virtual(x86_64_pagetable* pagetable, const char* name) {
    assert((uintptr_t) pagetable == PTE_ADDR(pagetable));

    console_printf(CPOS(10, 26), 0x0F00, "VIRTUAL ADDRESS SPACE FOR %s", name);
    for (uintptr_t va = 0; va < MEMSIZE_VIRTUAL; va += PAGESIZE) {
        vamapping vam = virtual_memory_lookup(pagetable, va);
        uint16_t color;
        if (vam.pn < 0)
            color = ' ';
        else {
            assert(vam.pa < MEMSIZE_PHYSICAL);
            int owner = pageinfo[vam.pn].owner;
            if (pageinfo[vam.pn].refcount == 0)
                owner = PO_FREE;
            color = memstate_colors[owner - PO_KERNEL];
            // reverse video for user-accessible pages
            if (vam.perm & PTE_U)
                color = ((color & 0x0F00) << 4) | ((color & 0xF000) >> 4)
                    | (color & 0x00FF);
            // darker color for shared pages
            if (pageinfo[vam.pn].refcount > 1)
                color &= 0x77FF;
        }
        uint32_t pn = PAGENUMBER(va);
        if (pn % 64 == 0)
            console_printf(CPOS(11 + pn / 64, 3), 0x0F00, "0x%06X ", va);
        console[CPOS(11 + pn / 64, 12 + pn % 64)] = color;
    }
}


// memshow_virtual_animate
//    Draw a picture of process virtual memory maps on the CGA console.
//    Starts with process 1, then switches to a new process every 0.25 sec.

void memshow_virtual_animate(void) {
    static unsigned last_ticks = 0;
    static int showing = 1;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 2) {
        last_ticks = ticks;
        ++showing;
    }

    // the current process may have died -- don't display it if so
    while (showing <= 2*NPROC && processes[showing % NPROC].p_state == P_FREE)
        ++showing;
    showing = showing % NPROC;

    if (processes[showing].p_state != P_FREE) {
        char s[4];
        snprintf(s, 4, "%d ", showing);
        memshow_virtual(processes[showing].p_pagetable, s);
    }
}
