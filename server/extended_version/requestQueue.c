#include "requestQueue.h"

void initReqQueue(RequestQueue * requestQueue){
  requestQueue->head = NULL;
  requestQueue->size = 0;
  pthread_mutex_init((&requestQueue->lock), NULL);
  pthread_cond_init((&requestQueue->ifEmptyLock), NULL);
  pthread_cond_init((&requestQueue->ifFullLock), NULL);
}

int isFull(RequestQueue *requestQueue){
  return requestQueue->size == MAX_PENDING_QUEUE_SIZE;
}

void enqueue(RequestQueue * requestQueue, int socketFd) {
  Request *new_node = malloc(sizeof(Request));
  if (!new_node) {
    return;
  }

  pthread_mutex_lock(&(requestQueue->lock));

  while(isFull(requestQueue)){

    printf(REQ_QUEUE_FULL_MSG);

    pthread_cond_wait(
        &requestQueue->ifFullLock,
        &requestQueue->lock
    );
  }

  new_node->socketFd = socketFd;
  new_node->next = requestQueue->head;

  requestQueue->head = new_node;
  requestQueue->size++;

  pthread_cond_signal(&requestQueue->ifEmptyLock);
  pthread_mutex_unlock(&(requestQueue->lock));

}


int isEmpty(RequestQueue *requestQueue){
  return requestQueue->size == 0;
}

int dequeue(RequestQueue * requestQueue, BlockingMode blockingMode) {

  Request *current, *prev = NULL;
  int socketFd = -1;

  pthread_mutex_lock(&(requestQueue->lock));

  if (blockingMode == BLOCKING){

    while(isEmpty(requestQueue)){

//      printf("워커쓰레드 요청 대기 중(Block)!\n");

      pthread_cond_wait(
          &requestQueue->ifEmptyLock,
          &requestQueue->lock
      );
    }

  } else if (blockingMode == NON_BLOCKING){

    if (isEmpty(requestQueue)){
      pthread_cond_signal(&requestQueue->ifFullLock);
      pthread_mutex_unlock(&(requestQueue->lock));
      return -1;
    }
  }


  current = requestQueue->head;

  while (current->next != NULL) {
    prev = current;
    current = current->next;
  }

  socketFd = current->socketFd;
  free(current);

  if (prev) {
    prev->next = NULL;

  } else {
    requestQueue->head = NULL;
  }

  requestQueue->size--;

//  printf("큐에 남아있는 리퀘스트 개수 : %d\n", requestQueue->size);

  pthread_cond_signal(&requestQueue->ifFullLock);
  pthread_mutex_unlock(&(requestQueue->lock));

  return socketFd;
}