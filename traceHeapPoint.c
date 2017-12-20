#include "traceHeapPoint.h"

int insertNode(void *node){
    if (node == NULL || ((heapNode *)node)->head == NULL) {
        return -1;
    }
    heapNode *newNode = malloc(sizeof(heapNode));
    memset(newNode, 0, sizeof(heapNode));
    newNode->referenceCount = 1;
    heapHeadNode *nodeP = node;
    if (nodeP->head != nodeP) {
        nodeP = nodeP->head;
    }
    newNode->next = nodeP->next;
    newNode->head = nodeP->head;
    nodeP->next = newNode;
    return 0;
}

int deleteNode(void *node){
    if (node == NULL || ((heapNode *)node)->head == NULL) {
        return -1;
    }
    if (((heapNode *)node)->head == node) {
        return -1;
    }
    heapNode *nodeP = ((heapNode *)node)->head;
    while (nodeP->next != node) {
        nodeP = nodeP->next;
    }
    if (nodeP->next == NULL) {
        return -1;
    }
    nodeP->next = nodeP->next->next;
    free(node);
    return 0;
}

void nodeReferAdd(void *node){
    if (node == NULL || ((heapNode *)node)->head == NULL) {
        return;
    }
    heapNode * nodeP = node;
    nodeP->referenceCount++;
}

void nodeReferSub(void *node){
    if (node == NULL || ((heapNode *)node)->head == NULL) {
        return;
    }
    heapNode * nodeP = node;
    nodeP->referenceCount--;
    if ((nodeP->referenceCount) <= 0) {
        if (deleteNode(node)) {
            printf("freeNode Error!\n");
        }
    }
}

void* safeMalloc(size_t size){
    heapHeadNode *ptr = malloc(sizeof(heapHeadNode));
    if (ptr) {
        ptr->head = ptr;
        ptr->next = NULL;
        ptr->nextThread = NULL;
        ptr->referenceCount = 1;
        ptr->addr = malloc(size);
        return ptr;
    }else{
        return NULL;
    }
}

void* safeRealloc(void * ptr, size_t new_size){
    void *tmp = realloc(((heapNode *)ptr)->addr, new_size);
    if (tmp == ((heapNode *)ptr)->addr) {
        return ptr;
    }
    heapHeadNode *newHeadP = malloc(sizeof(heapHeadNode));
    if (tmp && newHeadP) {
        if (newHeadP) {
            memset(newHeadP, 0, sizeof(heapHeadNode));
            newHeadP->head = newHeadP;
            newHeadP->referenceCount = 1;
            newHeadP->addr = tmp;
        }
    }
    heapHeadNode *nodeHeadP = ptr;
    nodeHeadP->addr = NULL;
    heapNode *nodeP = nodeHeadP->next;
    while (nodeP != NULL) {
        nodeP->addr = NULL;
        nodeP = nodeP->next;
    }
    heapHeadNode *threadNodeP = nodeHeadP->nextThread;
    while (threadNodeP != NULL) {
        threadNodeP->addr = NULL;
        heapNode *nodeP = threadNodeP->next;
        while (nodeP != NULL) {
            nodeP->addr = NULL;
            nodeP = nodeP->next;
        }
        threadNodeP = threadNodeP->nextThread;
    }
    if (tmp && newHeadP) {
        return newHeadP;
    }else{
        return NULL;
    }
}

void* safeCalloc(size_t numitems, size_t size){
    heapHeadNode *ptr = malloc(sizeof(heapHeadNode));
    if (ptr) {
        memset(ptr, 0, sizeof(heapHeadNode));
        ptr->head = ptr;
        ptr->referenceCount = 1;
        ptr->addr = calloc(numitems, size);
        return ptr;
    }else{
        return NULL;
    }
}

void safeFree(void* ptr){
    if (ptr == NULL || ((heapNode *)ptr)->head == NULL) {
        return;
    }
    if (((heapNode *)ptr)->head != ptr) {
        printf("safeFree Error!\n");
        return;
    }
    heapHeadNode *nodeHeadP = ptr;
    free(nodeHeadP->addr);
    nodeHeadP->addr = NULL;
    heapNode *nodeP = nodeHeadP->next;
    while (nodeP != NULL) {
        nodeP->addr = NULL;
        nodeP = nodeP->next;
    }
    heapHeadNode *threadNodeP = nodeHeadP->nextThread;
    while (threadNodeP != NULL) {
        threadNodeP->addr = NULL;
        heapNode *nodeP = threadNodeP->next;
        while (nodeP != NULL) {
            nodeP->addr = NULL;
            nodeP = nodeP->next;
        }
        threadNodeP = threadNodeP->nextThread;
    }
}

void* getPtr(void *origin, void * ptrOld, void *ptrNew){
    if (((heapNode *)origin)->head == NULL) {
        if (((heapNode *)ptrOld)->head != NULL) {
            heapNode * ptrOldP = ptrOld;
            ptrOldP->referenceCount--;
        }
        return ptrNew;
    }else if(((heapNode *)ptrOld)->head == NULL){
        if (!insertNode(origin)) {
            heapNode *newNode = ((heapNode *)origin)->next;
            newNode->addr = ptrNew;
            return newNode;
        }else{
            printf("getPtr Error1!\n");
            return ptrNew;
        }
    }else{
        heapNode *nodeOld = ptrOld;
        heapNode *nodeOri = origin;
        if (nodeOri->head == nodeOld->head && nodeOld->referenceCount == 1) {
            nodeOld->addr = ptrNew;
            return nodeOld;
        }else{
            (nodeOld->referenceCount)--;
            if (nodeOld->referenceCount <= 0) {
                deleteNode(nodeOld);
            }
            if (!insertNode(origin)) {
                heapNode *newNode = ((heapNode *)origin)->next;
                newNode->addr = ptrNew;
                return newNode;
            }else{
                printf("getPtr Error2!\n");
                return ptrNew;
            }
        }
    }
}
