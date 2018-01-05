#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct heapNode{
    void *addr;
    struct heapHeadNode *head;
    struct heapNode *next;
    void ** back;
    int referenceCount;
    bool isHead;
    bool isFree;
}heapNode;

extern void safeMalloc(size_t size, void **back);
extern void* safeRealloc(void * ptr, size_t new_size);
extern void* safeCalloc(size_t numitems, size_t size);
extern void safeFree(void** ptr);
extern void getPtr(void *origin, void ** ptrOld, void *ptrNew);
extern void traceDoublePoint(void **value, void ***ptr);
