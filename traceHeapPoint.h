#include <stdlib.h>
#include <stdio.h>

typedef struct heapNode{
    void * addr;
    struct heapHeadNode * head;
    struct heapNode * next;
    int referenceCount;
}heapNode;

typedef struct heapHeadNode{
    void * addr;
    struct heapHeadNode * head;
    struct heapNode * next;
    int referenceCount;
    struct heapHeadNode * nextThread;
}heapHeadNode;

extern void nodeReferAdd(void * node);
extern void nodeReferSub(void * node);
extern void* safeMalloc(size_t size);
extern void* safeRealloc(void * ptr, size_t new_size);
extern void* safeCalloc(size_t numitems, size_t size);
extern void safeFree(void * ptr);
extern void* getPtr(void * origin, void * ptrOld, void * ptrNew);
