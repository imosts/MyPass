#include "traceHeapPoint.h"

const void *GobalTracePtrNull = NULL;

int insertNode(void *node ,void **back){
    if (node == NULL || ((heapNode *)node)->head == NULL) {
        return -1;
    }
    heapNode *newNode = malloc(sizeof(heapNode));
    memset(newNode, 0, sizeof(heapNode));
    newNode->referenceCount = 1;
    heapNode *nodeP = node;
    if (nodeP->head != nodeP) {
        nodeP = nodeP->head;
    }
    newNode->next = nodeP->next;
    newNode->head = nodeP->head;
    newNode->back = back;
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

void safeMalloc(size_t size, void **back){
    heapNode *ptr = malloc(sizeof(heapNode));
    if (ptr) {
        ptr->addr = malloc(size);
        ptr->head = ptr;
        ptr->next = NULL;
        ptr->back = back;
        ptr->referenceCount = 0;
        ptr->isHead = true;
        ptr->isFree = false;
        traceDoublePoint(ptr, back);
    }else{
        traceDoublePoint(&GobalTracePtrNull, back);
    }
}

//TODO:
void *safeRealloc(void * ptr, size_t new_size){
    void *tmp = realloc(((heapNode *)ptr)->addr, new_size);
    if (tmp == ((heapNode *)ptr)->addr) {
        return ptr;
    }
    heapNode *newHeadP = malloc(sizeof(heapNode));
    if (tmp && newHeadP) {
        if (newHeadP) {
            memset(newHeadP, 0, sizeof(heapNode));
            newHeadP->head = newHeadP;
            newHeadP->referenceCount = 1;
            newHeadP->addr = tmp;
        }
    }
    heapNode *nodeHeadP = ptr;
    nodeHeadP->addr = NULL;
    heapNode *nodeP = nodeHeadP->next;
    while (nodeP != NULL) {
        nodeP->addr = NULL;
        nodeP = nodeP->next;
    }
    if (tmp && newHeadP) {
        return newHeadP;
    }else{
        return NULL;
    }
}

//TODO:
void *safeCalloc(size_t numitems, size_t size){
    heapNode *ptr = malloc(sizeof(heapNode));
    if (ptr) {
        memset(ptr, 0, sizeof(heapNode));
        ptr->head = ptr;
        ptr->referenceCount = 1;
        ptr->addr = calloc(numitems, size);
        return ptr;
    }else{
        return NULL;
    }
}


void safeFree(void** ptr){
    if (*ptr == NULL || ((heapNode *)*ptr)->head == NULL) {
        return;
    }
    if (((heapNode *)*ptr)->head != *ptr) {
        return;
    }
    heapNode *nodeHeadP = *ptr;
    free(nodeHeadP->addr);
    nodeHeadP->addr = NULL;
    nodeHeadP->isFree = true;
    if (nodeHeadP->next) {
        nodeHeadP = nodeHeadP->next;
        heapNode *tmp = NULL;
        while (nodeHeadP != NULL) {
            if (nodeHeadP->referenceCount <= 1 && nodeHeadP->back != NULL) {
                *(nodeHeadP->back) = &GobalTracePtrNull;
                tmp = nodeHeadP->next;
                free(nodeHeadP);
                nodeHeadP = tmp;
            }else{
                nodeHeadP->addr = NULL;
                nodeHeadP = nodeHeadP->next;
            }
        }
    }
    if (nodeHeadP->referenceCount <= 1) {
        *ptr = &GobalTracePtrNull;
        free(nodeHeadP);
    }
    
}

void getPtr(void *origin, void ** ptrOld, void *ptrNew){
    if (((heapNode *)origin)->head == NULL) {
        if (((heapNode *)*ptrOld)->head != NULL) {
            heapNode * ptrOldP = *ptrOld;
            ptrOldP->referenceCount--;
            if (ptrOldP->referenceCount <= 0 && ptrOldP->back != NULL) {
                deleteNode(ptrOldP);
                *(ptrOldP->back) = &GobalTracePtrNull;
            }
        }
        *ptrOld = ptrNew;
        return;
    }else if(((heapNode *)*ptrOld)->head == NULL){
        if (!insertNode(origin, ptrOld)) {
            heapNode *newNode = ((heapNode *)origin)->next;
            newNode->addr = ptrNew;
            *ptrOld = newNode;
            return;
        }else{
            printf("getPtr Error1!\n");
            *ptrOld = ptrNew;
            return;
        }
    }else{
        heapNode *nodeOld = *ptrOld;
        heapNode *nodeOri = origin;
        if (nodeOri->head == nodeOld->head && nodeOld->referenceCount == 1) {
            nodeOld->addr = ptrNew;
            return;
        }else{
            (nodeOld->referenceCount)--;
            if (nodeOld->referenceCount <= 0) {
                deleteNode(nodeOld);
            }
            if (!insertNode(origin, ptrOld)) {
                heapNode *newNode = ((heapNode *)origin)->next;
                newNode->addr = ptrNew;
                *ptrOld = newNode;
                return;
            }else{
                printf("getPtr Error2!\n");
                *ptrOld = ptrNew;
                return;
            }
        }
    }
}

void traceDoublePoint(void **value, void ***ptr){
    if (*ptr != NULL && ((heapNode *)*ptr)->head != NULL) {
        if (--((heapNode *)*ptr)->referenceCount <= 0) {
            if (((heapNode *)*ptr)->isHead) {
                free(*ptr);
            }else{
                deleteNode(*ptr);
            }
            
        }
    }
    if (value != NULL && ((heapNode *)value)->head != NULL) {
        ((heapNode *)value)->referenceCount++;
        ((heapNode *)value)->back = NULL;
    }
    *ptr = value;
}
