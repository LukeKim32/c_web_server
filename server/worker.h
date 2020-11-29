#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "http.h"
#include "requestQueue.h"
#include "hashMap.h"

#define STR_TO_LONG_ERR 0L
#define THREAD_POOL_CREATE_ERR -1

#define FILE_OPEN_ERR_MSG "file could not be opened!"
#define THREAD_POOL_CREATE_ERR_MSG "쓰레드 풀 생성 실패\n"
#define THREAD_POOL_CREATE_OK_MSG "Thread pool 생성 성공!\n"
#define FILE_READ_ERR -1

#define EPOLL_TIMEOUT 10
#define EPOLL_MAX_FD_SIZE 100

#define SERVER_MODEL_MASTER "master"
#define SERVER_MODEL_PEER "peer"
#define FILE_IO_BLOCKING "blocking"
#define FILE_IO_NON_BLOCKING "non-blocking"

#define MAIN_THREAD_ID 0
#define NEW_THREAD_ID_START 1

#define BASE_FILE_PATH "/var/tmp/20151529"

typedef struct {
    BlockingMode  fileIoType;
    int serverSockFd;
    char * serverModel;
} ServerInfo;

typedef struct {
    RequestQueue * requestQueue;
    long threadId;
    ServerInfo* serverInfo;
} ThreadArgs;

void * initThread(void * args);
int createThreadPool(long size, RequestQueue *requestQueue, ServerInfo* serverInfo);

// Request Handling
void readRequestedFile(void * aux, char ** buf, long * size, BlockingMode blockingMode);
char* verifyRequest(int clientSockFd, int isStateful);
void handleRequest(int clientSockFd, void *aux, BlockingMode fileIoType, int isStateful);

#define NETWORK_IO 0
#define FILE_IO 1

// Non-Blocking I/O Epoll Wrapper
void addNewEpollListen(struct epoll_event *events, int epollFdSet, int newFd);
int recvReqAndRegisterFileEpoll(int clientSockFd, struct epoll_event *events, int epollFdSet, HashMap * fdHashMap);

// I/O Type
void markFdIoType(HashMap *map, unsigned long fd, unsigned long ioType);
int isNetworkIo(HashMap *map, unsigned long fd);
int isSupportedFileIoType(char * blockingType);

// Thread Runner
// Master - Worker Model
void runBlockingFileIoWorker(ThreadArgs * threadArgs);
void runNonBlockingFileIoWorker(ThreadArgs *threadArgs);
int isMasterModel(char *serverModel);

// Peer Model
void runBlockingFileIoPeer(ThreadArgs * threadArgs);
void runNonBlockingFileIoPeer(ThreadArgs * threadArgs);
int isPeerModel(char *serverModel);

int isSupportedModel(char * modelName);
