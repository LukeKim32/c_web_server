#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef int BlockingMode;

#define BLOCKING 0
#define NON_BLOCKING 1

typedef struct req {
    int socketFd;
    struct req *next;
} Request;

#pragma once
typedef struct {
    Request * head;
    pthread_mutex_t lock;
    pthread_cond_t ifEmptyLock;
    pthread_cond_t ifFullLock;
    unsigned long size;
} RequestQueue;

#define REQ_QUEUE_FULL_MSG "워커쓰레드가 바쁩니다 - 요청 큐가 가득 찼습니다\n"
#define MAX_PENDING_QUEUE_SIZE 10000

void initReqQueue(RequestQueue * requestQueue);

int isFull(RequestQueue *requestQueue);

void enqueue(RequestQueue * requestQueue, int socketFd);


int isEmpty(RequestQueue *requestQueue);

int dequeue(RequestQueue * requestQueue, BlockingMode blockingMode);