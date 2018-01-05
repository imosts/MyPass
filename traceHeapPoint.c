#include "traceHeapPoint.h"

/*
 *              F                   F       F      F
 *   0      0       0       0      0000    0000    0000
 *  mulP  isnode  ishead  isfree       referenceCount
 */

////TODO:
//void *safeRealloc(void * ptr, size_t new_size){
//    void *tmp = realloc(((heapNode *)ptr)->addr, new_size);
//    if (tmp == ((heapNode *)ptr)->addr) {
//        return ptr;
//    }
//    heapNode *newHeadP = malloc(sizeof(heapNode));
//    if (tmp && newHeadP) {
//        if (newHeadP) {
//            memset(newHeadP, 0, sizeof(heapNode));
//            newHeadP->head = newHeadP;
//            newHeadP->referenceCount = 1;
//            newHeadP->addr = tmp;
//        }
//    }
//    heapNode *nodeHeadP = ptr;
//    nodeHeadP->addr = NULL;
//    heapNode *nodeP = nodeHeadP->next;
//    while (nodeP != NULL) {
//        nodeP->addr = NULL;
//        nodeP = nodeP->next;
//    }
//    if (tmp && newHeadP) {
//        return newHeadP;
//    }else{
//        return NULL;
//    }
//}
//
////TODO:
//void *safeCalloc(size_t numitems, size_t size){
//    heapNode *ptr = malloc(sizeof(heapNode));
//    if (ptr) {
//        memset(ptr, 0, sizeof(heapNode));
//        ptr->head = ptr;
//        ptr->referenceCount = 1;
//        ptr->addr = calloc(numitems, size);
//        return ptr;
//    }else{
//        return NULL;
//    }
//}


#define notMultiPTR(a) (!(((unsigned long)(a))&0x8000000000000000))
#define notNodeHeadPTR(a) (!(((unsigned long)(a))&0x6000000000000000))
#define notNodePTR(a) (!(((unsigned long)(a))&0x4000000000000000))
#define isFree(a) (((unsigned long)(a))&0x1000000000000000)

#define truePtrValue(a) (((unsigned long)(a))&0x00007FFFFFFFFFFF)

#define getMultiPTR(a) (((unsigned long)(a))|0x8000000000000000)
#define getNodeHeadPTR(a) (((unsigned long)(a))|0x6000000000000000)
#define getNodePTR(a) (((unsigned long)(a))|0x4000000000000000)

#define getNodePTRChange(a,b) (((unsigned long)(b))|(0xFFFF000000000000 & (unsigned long)a))

#define getNewNodePTR(a) (((unsigned long)(a))|0x4001000000000000)
#define getNewHeadNodePTR(a) (((unsigned long)(a))|0x6001000000000000)

#define addNodeRefer(a) ((((unsigned long)(a))&0x07FF000000000000)>0x07FE000000000000?(a):(((unsigned long)(a))+0x0001000000000000))
#define subNodeRefer(a) ((((unsigned long)(a))&0x07FF000000000000)>0?(((unsigned long)(a))-0x0001000000000000):(a))

#define getNodeRefer(a) ((unsigned int)((((unsigned long)(a))&0x07FF000000000000)>>48))
#define getFree(a) (((unsigned long)(a))|0x1000000000000000)

void tracePoint(void **value, void ***ptr){
//    printf("tracePoint11 %lx\n",  ptr);
//    printf("tracePoint12 %lx\n",  value);
    if (*ptr != NULL && ((((unsigned long)(*ptr)) & 0xFFFF000000000000) ==  0x8000000000000000) && (((unsigned long)(*ptr)) & 0x00007FFFFFFFFFFF)) {
//        printf("tracePoint2 %lx\n",  ptr);
//        printf("tracePoint3 %lx\n",  *ptr);
        heapNode *nodeP = truePtrValue(*ptr);
        if (nodeP) {
            nodeP->addr = subNodeRefer(nodeP->addr);
            if (getNodeRefer(nodeP->addr) <= 0) {
                if (!notNodeHeadPTR(nodeP->addr)) {
                    free(nodeP);
                }else{
                    deleteNode(nodeP);
                }
            }
        }
    }
//    printf("tracePoint4\n");
    if (value != NULL && !notMultiPTR(value)) {
//        printf("tracePoint51 %lx\n",  value);
//        void **tmp = truePtrValue(value);
//        printf("tracePoint52 %lx\n",  tmp);
        heapNode *nodeP = truePtrValue(value);
//        printf("tracePoint53 %lx\n",  nodeP);
        nodeP->addr = addNodeRefer(nodeP->addr);
        if (getNodeRefer(nodeP->addr) > 1) {
            nodeP->back = NULL;
        }
    }
//    printf("tracePoint6\n");
    *ptr = value;
}

int insertNode(void *node){
    if (node == NULL || notNodePTR(((heapNode *)node)->addr)) {
        return -1;
    }
    heapNode *newNode = malloc(sizeof(heapNode));
    memset(newNode, 0, sizeof(heapNode));
    newNode->addr = getNewNodePTR(newNode->addr);//应该在函数外部设置，不然要在外部重新设置一次
    heapNode *nodeP = node;
    newNode->next = nodeP->next;
    newNode->head = nodeP->head;
    newNode->back = NULL;
    nodeP->next = newNode;
    return 0;
}

int deleteNode(void *node){
    if (node == NULL || notNodePTR(((heapNode *)node)->addr)) {
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
        ptr->addr = getNodeHeadPTR(ptr->addr);
        ptr->head = ptr;
        ptr->next = NULL;
        ptr->back = back;
        ptr = getMultiPTR(ptr);
        tracePoint(ptr, back);
    }else{
        tracePoint(NULL, back);
    }
}


void safeFree(void** ptr){
    if (*ptr == NULL || notMultiPTR(((heapNode *)(*ptr)))) {
        return;
    }
    void* ptrAddress = truePtrValue(*ptr);
    
    heapNode *nodeHeadP = ptrAddress;
    if (isFree(nodeHeadP->addr)) {
        *ptr = NULL;
        nodeHeadP->addr = subNodeRefer(nodeHeadP->addr);
        if (getNodeRefer(nodeHeadP->addr) <= 0) {
            free(ptrAddress);
        }
        return;
    }
    
    nodeHeadP->addr = getFree(nodeHeadP->addr);
    free(truePtrValue(nodeHeadP->addr));
    nodeHeadP->addr = (unsigned long)(nodeHeadP->addr) & 0xFFFF000000000000;
    *ptr = NULL;
    nodeHeadP->addr = subNodeRefer(nodeHeadP->addr);
    
    if (nodeHeadP->next) {
        nodeHeadP = nodeHeadP->next;
        heapNode *tmp = NULL;
        while (nodeHeadP != NULL) {
            if (getNodeRefer(nodeHeadP) <= 1 && nodeHeadP->back != NULL) {
                *(nodeHeadP->back) = NULL;
                tmp = nodeHeadP->next;
                free(nodeHeadP);
                nodeHeadP = tmp;
            }else{
                nodeHeadP->addr = NULL;
                nodeHeadP = nodeHeadP->next;
            }
        }
    }
    
    if (getNodeRefer(((heapNode *)ptrAddress)->addr) < 1) {
        free(nodeHeadP);
    }
    
    
}

void getPtr(void **nodeOrigin, void ** nodeOld, void *ptrNew){
    if ((*nodeOrigin == NULL) || (*nodeOrigin != NULL && notMultiPTR(*nodeOrigin))) {
//        printf("getPtr11 %lx\n",  nodeOrigin);
//        printf("getPtr12 %lx\n",  *nodeOrigin);
//        printf("getPtr13 %lx\n",  nodeOld);
//        printf("getPtr14 %lx\n",  *nodeOld);
        if (*nodeOld != NULL && !notMultiPTR(*nodeOld)) {
            heapNode *ptrOldP = truePtrValue(*nodeOld);
            ptrOldP->addr = subNodeRefer(ptrOldP->addr);
            if (getNodeRefer(ptrOldP->addr) <= 0) {
                if (!notNodeHeadPTR(ptrOldP->addr)) {
                    if (ptrOldP->back) {
                        *(ptrOldP->back) = NULL;
                    }
                    free(ptrOldP);
                }else{
                    if (ptrOldP->back) {
                        *(ptrOldP->back) = NULL;
                    }
                    deleteNode(ptrOldP);
                }
            }
        }
        *nodeOld = ptrNew;
        return;
    }else if((*nodeOld == NULL) || (*nodeOld != NULL && notMultiPTR(*nodeOld))){
//        printf("getPtr21 %lx\n",  nodeOrigin);
//        printf("getPtr22 %lx\n",  *nodeOrigin);
//        printf("getPtr23 %lx\n",  nodeOld);
//        printf("getPtr24 %lx\n",  *nodeOld);
        if (!insertNode(truePtrValue(*nodeOrigin))) {
//            printf("getPtr25\n");
            heapNode *newNode = ((heapNode *)truePtrValue(*nodeOrigin))->next;
//            printf("getPtr26\n");
//            printf("getPtr261 %lx\n",  nodeOld);
//            printf("getPtr262 %lx\n",  newNode->back);
//            newNode->back = nodeOld;
//            printf("getPtr27\n");
            newNode->addr = getNewNodePTR(ptrNew);
//            printf("getPtr28\n");
            *nodeOld = getMultiPTR(newNode);
//            printf("getPtr29\n");
            return;
        }else{
            printf("getPtr Error1!\n");
            *nodeOld = ptrNew;
            return;
        }
    }else{
//        printf("getPtr31 %lx\n",  nodeOrigin);
//        printf("getPtr32 %lx\n",  *nodeOrigin);
//        printf("getPtr33 %lx\n",  nodeOld);
//        printf("getPtr34 %lx\n",  *nodeOld);
        heapNode *ptrOldP = truePtrValue(*nodeOld);
        heapNode *ptrOriginP = truePtrValue(*nodeOrigin);
        if (ptrOriginP->head == ptrOldP->head && getNodeRefer(ptrOldP->addr) == 1) {
            //如果节点是头节点？？？是否有BUG
            ptrOldP->addr = getNodePTRChange(ptrOldP->addr, ptrNew);
            return;
        }else{
            ptrOldP->addr = subNodeRefer(ptrOldP->addr);
            if (getNodeRefer(ptrOldP->addr) <= 0) {
                if (!notNodeHeadPTR(ptrOldP->addr)) {
                    if (ptrOldP->back) {
                        *(ptrOldP->back) = NULL;
                    }
                    free(ptrOldP);
                }else{
                    if (ptrOldP->back) {
                        *(ptrOldP->back) = NULL;
                    }
                    deleteNode(ptrOldP);
                }
            }
            if (!insertNode(truePtrValue(*nodeOrigin))) {
                heapNode *newNode = ((heapNode *)truePtrValue(*nodeOrigin))->next;
                newNode->back = nodeOld;
                newNode->addr = getNewNodePTR(ptrNew);
                *nodeOld = getMultiPTR(newNode);
                return;
            }else{
                printf("getPtr Error2!\n");
                *nodeOld = ptrNew;
                return;
            }
        }
    }
}

void *Load(void **ptr){
    if (!((unsigned long)ptr & 0xC000000000000000)) {
        return ptr;
    }else if ((unsigned long)ptr & 0x8000000000000000) {
        return *((void **)((unsigned long)ptr & 0x00007FFFFFFFFFFF));
    }else{
        return ((void *)((unsigned long)ptr & 0x00007FFFFFFFFFFF));
    }
}
