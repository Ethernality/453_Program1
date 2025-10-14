#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    size_t size = 4096;

    void *start_break = sbrk(0);
    printf("Initial program break: %p\n", start_break);

    void *ptr = malloc(size);
    if (!ptr) {
        printf("malloc(%zu) failed: %s\n", size, strerror(errno));
        return 1;
    }
    printf("malloc(%zu) returned: %p\n", size, ptr);

    void *after_alloc = sbrk(0);
    printf("Program break after malloc: %p\n", after_alloc);

    char *mem = (char *)ptr;
    for (int i = 0; i < 10; i++) mem[i] = 'A' + i;
    printf("First bytes: %c %c %c\n", mem[0], mem[1], mem[2]);

    void *before_free = sbrk(0);
    printf("Program break before free: %p\n", before_free);

    free(ptr);

    void *after_free = sbrk(0);
    printf("Program break after free: %p\n", after_free);

    /* ------------------ Test realloc ------------------ */
    printf("\n--- Testing realloc ---\n");

    char *rptr = malloc(32);
    if (!rptr) { perror("malloc for realloc test"); return 1; }
    strcpy(rptr, "Hello custom realloc!");
    printf("Before realloc: %p content: %s\n", rptr, rptr);

    rptr = realloc(rptr, 128);
    if (!rptr) { perror("realloc failed"); return 1; }
    printf("After realloc (grow): %p content: %s\n", rptr, rptr);

    rptr = realloc(rptr, 16);
    if (!rptr) { perror("realloc failed"); return 1; }
    printf("After realloc (shrink): %p content prefix: %.5s\n", rptr, rptr);

    free(rptr);

    /* ------------------ Test calloc ------------------ */
    printf("\n--- Testing calloc ---\n");

    int *arr = (int *)calloc(10, sizeof(int));
    if (!arr) { perror("calloc failed"); return 1; }

    int all_zero = 1;
    for (int i = 0; i < 10; i++) {
        if (arr[i] != 0) { all_zero = 0; break; }
    }
    printf("calloc returned: %p — all_zero: %s\n", arr, all_zero ? "YES" : "NO");

    arr[0] = 42;
    arr[9] = 99;
    printf("Modified arr[0]=%d arr[9]=%d\n", arr[0], arr[9]);

    free(arr);

    // final quick tests
    printf("malloc aligned? %d\n", ((uintptr_t)ptr % 16) == 0);
    printf("realloc aligned? %d\n", ((uintptr_t)rptr % 16) == 0);
    printf("calloc aligned? %d\n", ((uintptr_t)arr % 16) == 0);

    void *end_break = sbrk(0);
    printf("\nProgram break after all tests: %p\n", end_break);

    return 0;
}
