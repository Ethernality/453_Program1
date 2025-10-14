#ifdef f
#undef f			// for some wierd typedef error
#endif			

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

// for implicit declaration and initialization warnings 
// void *sbrk(intptr_t);

// for debug
static int dbg_inited = 0;
static int dbg_on = 0;

static void init_debug_once(void) {
    if (!dbg_inited) {
	dbg_inited = 1;
	const char *e = getenv("DEBUG_MALLOC");
	//dbg_on = (e && *e) ? 1:0;
	dbg_on = (e != NULL);
    }
}

static void debug_write(const char *s) {
    if (!dbg_on) {
	return;
    }
    write(2, s, (unsigned)strlen(s));
}

static void debug_log_malloc(int req, void *ptr, int sz) {
    init_debug_once();
    if (!dbg_on) {
	return;
    }
    char buf[160];
    int n = snprintf(buf, sizeof(buf), "MALLOC: malloc(%d) => (ptr=%p, size %d)\n", req, ptr, sz);
   
    if (n > 0) {
	debug_write(buf);
    }

}

static void debug_log_calloc(int nmemb, int size, void *ptr, int sz) {
    init_debug_once();
    if (!dbg_on) {
	return;
    }
    char buf[200];
    int n = snprintf(buf, sizeof(buf), "MALLOC: calloc(%d,%d) => (ptr=%p, size=%d)\n", nmemb, size, ptr, sz);
    if (n > 0) {
	debug_write(buf);
    }
}

static void debug_log_realloc(void *oldp, int req, void *newp, int sz) {
    init_debug_once();
    if (!dbg_on) {
	return;
    }
    char buf[200];
    int n = snprintf(buf, sizeof(buf), "MALLOC: realloc(%p,%d) => (ptr=%p, size=%d)\n", oldp, req, newp, sz);
    if (n > 0) {
	debug_write(buf);
    }
}

static void debug_log_free(void *p) {
    init_debug_once();
    if (!dbg_on) {
	return;
    }
    char buf[120];
    int n = snprintf(buf, sizeof(buf), "MALLOC: free(%p)\n", p);
    if (n > 0) {
	debug_write(buf);
    }
}

typedef struct Block {
    size_t size;
    struct Block *next;		// sets up linked list
    int free;			// check for if block is free
} Block;

static Block *b_head = NULL;	// initialize beginning of blocks


static size_t align16(size_t n) { 	// user payload block alignment
    const size_t Alignment = 16;
    size_t remainder = n % Alignment;
    if (remainder == 0) {
	return n;
    } else {
	return n + (Alignment - remainder);
    }
}

static size_t header_size(void) {	// internal block alignment
    const size_t Alignment = 16;
    size_t sz = sizeof(Block);
    size_t remainder = sz % Alignment;
    if (remainder == 0) {
	return sz;
    } else {
	return sz + (Alignment - remainder);
    }
}

static inline void *user_ptr_from_block(Block *b) {	// primes the header block for user ptr 
    return (void *)((char *)b + header_size());
}

static Block *find_free_block(size_t asize) {	// helper to find a free block that fits asize
    Block *cur = b_head;
    while (cur) {
        if (cur->free && cur->size >= asize) {
	    return cur;
        }
	cur = cur->next;
    }
    return NULL;
}

static void maybe_split(Block *b, size_t asize) {	// for if a free block is big enough for asize
    size_t hsz = header_size();
    if (b->size >= asize + hsz + 16u) {
        char *base = (char *)b;
        Block *newb = (Block *)(base + hsz + asize);
        newb->size = b->size - asize - hsz;
        newb->free = 1;
        newb->next = b->next;
        b->size = asize;
        b->next = newb;
    }
}

static Block *request_from_os(size_t asize) {		// grows heap w/ header + payload using sbrk
    size_t hsz = header_size();
    if (asize > (size_t)-1 - hsz) { 
	errno = ENOMEM; return NULL; 
    }
    size_t total = hsz + asize;

    void *mem = sbrk((intptr_t)total);
    if (mem == (void *)-1) { 
	errno = ENOMEM; return NULL; 
    }

    Block *b = (Block *)mem;
    b->size = asize;
    b->free = 0;
    b->next = NULL;

    if (!b_head) {
        b_head = b;
    } else {
        Block *t = b_head;
        while (t->next) t = t->next;
        t->next = b;
    }
    return b;
}

// finds closest block to user ptr
static int find_block_from_user_ptr(void *ptr, Block **out_prev, Block **out_blk) {
    if (!ptr) {
	return 0;
    }
    size_t hsz = header_size();
    Block *prev = NULL, *cur = b_head;
    while (cur) {
        void *user = (void *)((char *)cur + hsz);
        void *end = user + cur->size;
//	if (user == ptr) {
	if ((void *)ptr >= user && (void *)ptr < end) {
            if (out_prev) {
		*out_prev = prev;
	    }
            if (out_blk)  {
		*out_blk  = cur;
            }
	    return 1;
        }
        prev = cur;
        cur = cur->next;
    }
    return 0;
}

void *malloc(size_t size) {
    if (size == 0) {
	debug_log_malloc(0, NULL, 0);
	return NULL;
    }
    size_t asize = align16(size);

    Block *b = find_free_block(asize);
    if (!b) {
        b = request_from_os(asize);
        if (!b) {
	    return NULL;
    	}
    } else {
        maybe_split(b, asize);
        b->free = 0;
    }
    
    void *user = user_ptr_from_block(b);
    debug_log_malloc((int)size, user, (int)b->size);

    return user;
}

void free(void *ptr) {
    debug_log_free(ptr);
    if (!ptr) {
	return;
    }
    Block *prev = NULL, *b = NULL;
    if (!find_block_from_user_ptr(ptr, &prev, &b)) {
	return;
    }
    if (b->free) {
	return;   
    }                                

    b->free = 1;

    // connect back the linked list
    while (b->next && b->next->free) {
        Block *nx = b->next;
        b->size += header_size() + nx->size;
        b->next = nx->next;
    }

    if (prev && prev->free) {
        prev->size += header_size() + b->size;
        prev->next = b->next;
        b = prev;
        while (b->next && b->next->free) {
            Block *nx2 = b->next;
            b->size += header_size() + nx2->size;
            b->next = nx2->next;
        }
    }

    if (!b->next) {
        size_t total = header_size() + b->size;
        void *res = sbrk(-((intptr_t)total));
        if (res != (void *)-1) {
            if (b == b_head) {
                b_head = NULL;
            } else {
                Block *t = b_head;
                while (t && t->next != b) t = t->next;
                if (t) {
		    t->next = NULL;
                }
	    }
        }
    }
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
	return malloc(size);
    }
    if (size == 0) { 
	free(ptr); 
	return NULL; 
    }

    Block *prev = NULL, *b = NULL;
    if (!find_block_from_user_ptr(ptr, &prev, &b)) {
	return malloc(size);
    }
    size_t asize = align16(size);
    void *base_user = (char *)b + header_size(); 

    if (b->size >= asize) { 
	maybe_split(b, asize); 
        debug_log_realloc(ptr, (int)size, base_user, (int)b->size); 
	return ptr; 
    }

    if (b->next && b->next->free) {
        size_t hsz = header_size();
        if (b->size + hsz + b->next->size >= asize) {
            Block *nx = b->next;
            b->size += hsz + nx->size;
            b->next = nx->next;
            maybe_split(b, asize);
            base_user = (char *)b + hsz;
            debug_log_realloc(ptr, (int)size, base_user, (int)b->size);
            return base_user;
        }
    }

    void *newp = malloc(size);
    if (!newp) {
	debug_log_realloc(ptr, (int)size, NULL, 0);
	return NULL;
    }
    size_t copy = (b->size < size) ? b->size : size;
    char *dst = (char *)newp, *src = (char *)ptr;

    for (size_t i = 0; i < copy; ++i) {
	dst[i] = src[i];
    }

    free(ptr);
    debug_log_realloc(ptr, (int)size, newp, (int)asize);
    return newp;
}

void *calloc(size_t nmemb, size_t size) {
    if (nmemb && size && nmemb > (size_t)-1 / size) {
	errno = ENOMEM; 
	debug_log_calloc((int)nmemb,(int)size,NULL,0);
	return NULL; 
    }
    size_t total = nmemb * size;
    void *p = malloc(total);

    if (!p) {
	debug_log_calloc((int)nmemb,(int)size,NULL,0);
	return NULL;
    }
    char *c = (char *)p;
 
    for (size_t i = 0; i < total; ++i) c[i] = 0;
    
    memset(p, 0, total);
    debug_log_calloc((int)nmemb,(int)size,p,(int)align16(total));
    return p;
}
