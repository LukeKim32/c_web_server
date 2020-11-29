#include "worker.h"

// Thread Initiators
void *initThread(void *args) {

  ThreadArgs *threadArgs = (ThreadArgs *) args;
  ServerInfo  * serverInfo = threadArgs->serverInfo;


  if (isMasterModel(serverInfo->serverModel)){

    switch (serverInfo->fileIoType){
      case BLOCKING :
        runBlockingFileIoWorker(threadArgs);
        break;

      case NON_BLOCKING:

        runNonBlockingFileIoWorker(threadArgs);
        break;
    }

  } else { // Peer model

    switch (serverInfo->fileIoType){
      case BLOCKING :
        runBlockingFileIoPeer(threadArgs);
        break;

      case NON_BLOCKING:

        runNonBlockingFileIoPeer(threadArgs);
        break;
    }


  }

  return NULL;
}


int createThreadPool(long size, RequestQueue *requestQueue, ServerInfo* serverInfo) {

  if (size == 0){ // No creation if size is 0
    return 0;
  }

  for (long i = NEW_THREAD_ID_START; i < NEW_THREAD_ID_START + size; i++) {

    pthread_t newThread;

    ThreadArgs *args = (ThreadArgs *) (malloc(sizeof(ThreadArgs)));
    args->threadId = i;
    args->requestQueue = requestQueue;
    args->serverInfo = serverInfo;

    int isCreated = pthread_create(
        &newThread,
        NULL,
        initThread,
        (void *) args
    );

    if (isCreated != 0) {
      printf(THREAD_POOL_CREATE_ERR_MSG);
      return -1;
    }
  }

  printf(THREAD_POOL_CREATE_OK_MSG);
  return 0;
}


// Master - Worker Model
// - Blocking Network I/O
// - Blocking File I/O
//
void runBlockingFileIoWorker(ThreadArgs * threadArgs){

  RequestQueue *requestQueue = threadArgs->requestQueue;
  long threadId = threadArgs->threadId;
  ServerInfo  * serverInfo = threadArgs->serverInfo;

  printf("Blocking File I/O 쓰레드 실행!\n");

  while (1) {

    // 들을 요청 추가하기
    // will not proceed with empty client socket
    int clientSockFd = dequeue(requestQueue, serverInfo->fileIoType);

    printf("워커쓰레드 (id : %lu) 가 요청을 받았음\n", threadId);

    char *fileName = verifyRequest(clientSockFd, STATELESS);
    if (fileName == NULL) {
      continue;
    }

    printf("debug 1 - file name : %s\n", fileName);

    FILE *fp = fopen(fileName, "r");
    if (fp == NULL) {

      printf("file open error 인데??\n");

      printf(FILE_OPEN_ERR_MSG);

      // Result : clientSockFd - Closed
      // fp - Not Opened before
      handleError(
          clientSockFd,
          STATUS_NOT_FOUND_CODE,
          CONTENT_TYPE_HTML,
          STATELESS
      );
      continue;
    }


    printf("file open error가 아닌데??\n");

    // Result : clientSockFd - Closed
    // fp - Closed
    handleRequest(
        clientSockFd,
        (void *) fp,
        serverInfo->fileIoType,
        STATELESS
    );
  }

  free(threadArgs);
}

// Master - Worker Model
// - Blocking Network I/O
// - Non-Blocking File I/O
//
void runNonBlockingFileIoWorker(ThreadArgs *threadArgs){

  RequestQueue *requestQueue = threadArgs->requestQueue;
  long threadId = threadArgs->threadId;
  ServerInfo  * serverInfo = threadArgs->serverInfo;

  printf("Non-Blocking File I/O 쓰레드 실행!\n");

  int epollFdSet = epoll_create(EPOLL_MAX_FD_SIZE);

  struct epoll_event *events = (struct epoll_event *) (malloc(
      sizeof(struct epoll_event) * EPOLL_MAX_FD_SIZE
  ));

  HashMap *fdHashMap = createHashMap();

  int respondedCnt = 1;

  while (1) {

    int clientSockFd = dequeue(requestQueue, serverInfo->fileIoType);

    // *** 요청 처리하기 Non-blocking 버젼 :
    // 새로 들어온 소켓 (연결은 매번 끊기니깐 새로 온 것) 의 요청을 non block 처리하게 등록하기
    //
    // 요청한 파일을 Epoll 에 등록하는 작업
    if (clientSockFd != -1) {

      printf("워커쓰레드 (id : %lu) 가 요청을 받았음\n", threadId);


      char *fileName = verifyRequest(clientSockFd, STATELESS);
      if (fileName == NULL) {
        continue;
      }

      // Non-block으로 오픈한 파일을 (아직 파일 읽을 준비는 안한다. read() 불려야하지)
      // 후에 read하게 되면, non block 디스크립터이므로 read의 결과가 바로 나온다
      // 따라서 read할 데이터가 없아도 결과(-1) 가 나올 수 있는데, 이 데이터가 오는 것을 기다리는 작업을
      // epoll을 통해 한다.
      // open에 O_Nonblock 옵션을 주는 것은, open() 함수 자체가 논블락이란 의미가 아니라 디스크립터를 논블락킹으로
      // 하겠다는 의미이다!
      //
      int reqFileFd = open(fileName, O_RDONLY | O_NONBLOCK);
      if (reqFileFd != -1) {
        // 요청한 파일의 I/O를 위해 오픈을 epoll에 등록한다
        addNewEpollListen(
            events,
            epollFdSet,
            reqFileFd
        );

        insertHashMap(fdHashMap, reqFileFd, clientSockFd);
      }

      free(fileName);
    }

    // dequeue를 했을 때, 새로 온 요청이 없어도, 기존에 file I/O 등록해놨던 거 처리해줘야한다.
    // Non blocking
    int readyIoCnt = epoll_wait(
        epollFdSet,
        events,
        EPOLL_MAX_FD_SIZE,
        EPOLL_TIMEOUT // in milliseconds (As I prefer total response in 30ms, epoll timeout shall be 10ms
    );

    if (readyIoCnt == -1) {
      printf("epoll_wait에서 에러 발생!\n");
    }

    for (int i = 0; i < readyIoCnt; i++) {

      int reqFileFd = events[i].data.fd;

      // get client socket fd from reqFileFd
      HashItem *fileFdToSockFd = pop(fdHashMap, reqFileFd);
      if (fileFdToSockFd == NULL) {
        printf("Hash map pop error! Client is lost!!\n");
      }

      long targetClientSockFd = fileFdToSockFd->value;

      // Result : clientSockFd - Closed
      // fp - Closed
      handleRequest(
          targetClientSockFd,
          (void *) (&reqFileFd),
          serverInfo->fileIoType,
          STATELESS
      );

      printf(
          "워커쓰레드 (id : %lu) 가 %d-번째 요청을 처리함\n",
          threadId,
          respondedCnt++
      );

      free(fileFdToSockFd);
    }
  }

  free(threadArgs);
}

// Peer Model
// - Non-Blocking Network I/O
// - Blocking File I/O
//
void runBlockingFileIoPeer(ThreadArgs * threadArgs){

  RequestQueue *requestQueue = threadArgs->requestQueue;
  long threadId = threadArgs->threadId;
  ServerInfo  * serverInfo = threadArgs->serverInfo;

  int serverSocketFd = serverInfo->serverSockFd;

  printf("Blocking File I/O Peer 실행!\n");

  int epollFdSet = epoll_create(EPOLL_MAX_FD_SIZE);

  struct epoll_event *events = (struct epoll_event *) (malloc(
      sizeof(struct epoll_event) * EPOLL_MAX_FD_SIZE
  ));

  while (1) {

    struct sockaddr_in clientAddress;
    socklen_t clientAddrSize = sizeof(clientAddress);

    pthread_mutex_lock(&requestQueue->lock);

    // Non blocking accept
    int newClientSockFd = accept(
        serverSocketFd,
        (struct sockaddr *) &clientAddress,
        &clientAddrSize
    );

    pthread_mutex_unlock(&(requestQueue->lock));

    // If Pending connection setup request exists!
    if (newClientSockFd != -1) {

      printf("새로운 연결 생성! 클라이언트 fd : %d\n", newClientSockFd);
      fflush(stdout);

      // Add
      addNewEpollListen(
          events,
          epollFdSet,
          newClientSockFd
      );
    }

    int readyIoCnt = epoll_wait(
        epollFdSet,
        events,
        EPOLL_MAX_FD_SIZE,
        EPOLL_TIMEOUT // in milliseconds (As I prefer total response in 30ms, epoll timeout shall be 10ms
    );

    // if error!
    if (readyIoCnt == -1) {
      printf("epoll_wait에서 에러 발생!\n");

    }


    for (int i = 0; i < readyIoCnt; i++) {

      int clientFd = events[i].data.fd; // Network I/O

      printf("<<<<< (클라이언트 fd : %d) - 쓰레드 (id : %d)\n", clientFd, threadId);
      fflush(stdout);

      char *fileName = verifyRequest(clientFd, STATEFUL);
      fflush(stdout);
      if (fileName == NULL) {
        continue;
      }

      FILE *fp = fopen(fileName, "r");
      if (fp == NULL) {
        printf(FILE_OPEN_ERR_MSG);

        // Result : clientSockFd - Alive
        // fp - Not Opened
        handleError(
            clientFd,
            STATUS_NOT_FOUND_CODE,
            CONTENT_TYPE_HTML,
            STATEFUL
        );
        continue;
      }

      // Result : clientSockFd - Alive
      // fp - Closed
      handleRequest(
          clientFd,
          (void *) fp,
          serverInfo->fileIoType,
          STATEFUL
      );
      fflush(stdout);

      printf(">>>>> (클라이언트 fd : %d) - 쓰레드 (id : %d)\n", clientFd, threadId);
      fflush(stdout);
    }

  }

  free(threadArgs);
}

// Peer Model
// - Non-Blocking Network I/O
// - Non-Blocking File I/O
//
void runNonBlockingFileIoPeer(ThreadArgs * threadArgs){

  RequestQueue *requestQueue = threadArgs->requestQueue;
  long threadId = threadArgs->threadId;
  ServerInfo  * serverInfo = threadArgs->serverInfo;

  int serverSocketFd = serverInfo->serverSockFd;

  printf("Non Blocking File I/O Peer 실행!\n");

  int epollFdSet = epoll_create(EPOLL_MAX_FD_SIZE);

  struct epoll_event *events = (struct epoll_event *) (malloc(
      sizeof(struct epoll_event) * EPOLL_MAX_FD_SIZE
  ));

  HashMap *fdHashMap = createHashMap();
  HashMap * fdToIoTypeMap = createHashMap();

  int respondedCnt = 1;

  while (1) {

    struct sockaddr_in clientAddress;
    socklen_t clientAddrSize = sizeof(clientAddress);

    pthread_mutex_lock(&requestQueue->lock);

    // Non blocking accept
    int newClientSockFd = accept(
        serverSocketFd,
        (struct sockaddr *) &clientAddress,
        &clientAddrSize
    );

    pthread_mutex_unlock(&(requestQueue->lock));

    // If Pending connection setup request exists!
    if (newClientSockFd != -1) {

      // Add
      addNewEpollListen(
          events,
          epollFdSet,
          newClientSockFd
      );

      markFdIoType(fdToIoTypeMap, newClientSockFd, NETWORK_IO);
    }

    int readyIoCnt = epoll_wait(
        epollFdSet,
        events,
        EPOLL_MAX_FD_SIZE,
        EPOLL_TIMEOUT // in milliseconds (As I prefer total response in 30ms, epoll timeout shall be 10ms
    );

    // if error!
    if (readyIoCnt == -1) {
      printf("epoll_wait에서 에러 발생!\n");
    }

    for (int i = 0; i < readyIoCnt; i++) {

      int eventFd = events[i].data.fd; // Network I/O or File I/O

      if (isNetworkIo(fdToIoTypeMap, eventFd)){

        printf("<<<<< (클라이언트 fd : %d) - 쓰레드 (id : %d)\n", eventFd, threadId);
        fflush(stdout);

        int fileFd = recvReqAndRegisterFileEpoll(
            eventFd,
            events,
            epollFdSet,
            fdHashMap
        );

        if (fileFd == -1){
          continue;
        }

        // 현재 eventFD는 이미 epoll 등록 시 networkIO 로 등록되어있다.
        // 파일 open FD만 fileIO로 등록 해놓자.
        markFdIoType(fdToIoTypeMap, fileFd, FILE_IO);

      } else { // FILE IO

        int fileFd = eventFd;

        HashItem *fileFdToSockFd = pop(fdHashMap, fileFd);
        if (fileFdToSockFd == NULL) {
          printf("Hash map pop error! Client is lost!!\n");
          continue;
        }

        long targetClientSockFd = fileFdToSockFd->value;

        // Result : clientSockFd - Alive
        // fp - Closed
        handleRequest(
            targetClientSockFd,
            (void *) (&fileFd),
            serverInfo->fileIoType,
            STATEFUL
        );

        fflush(stdout);

        printf(
            "워커쓰레드 (id : %lu) 가 %d-번째 요청을 처리함\n",
            threadId,
            respondedCnt++
        );
        fflush(stdout);

        // close the file I/O fd
        free(fileFdToSockFd);

      }

      // TODO 파일디스크립터에 따라 \ Network I/O, FIle I/O 인지 구분하기
      // TODO 1) Network I/O 일 경우, 요청 받아서 File I/O 넣어주기
      //   (network fd - file fd map & fd => isNetwork / isFile)
      //
      // TODO 2) File I/O 일 경우, socket fd 찾아서 응답해주기 (file descriptor close)
      //
    }
  }

  free(threadArgs);
}


// 파일 디스크립터 닫기 처리
//
void readRequestedFile(void *aux, char **buf, long *size, BlockingMode fileIoType) {

  if (fileIoType == BLOCKING) {

    FILE *fp = (FILE *) aux;

    // 파일 크기 측정
    fseek(fp, 0L, SEEK_END);
    *size = ftell(fp);

    // 커서 위치 초기화
    fseek(fp, 0L, SEEK_SET);

    *buf = (char *) (malloc(
        sizeof(char) * (*size + 1)
    ));

    size_t end = fread(
        *buf,
        sizeof(char),
        *size,
        fp
    );

    (*buf)[end] = '\0';

    fclose(fp);

  } else if (fileIoType == NON_BLOCKING) {

    int reqFileFd = *((int *) aux);

    // 파일 크기 측정
    *size = lseek(reqFileFd, 0, SEEK_END);
    if (*size == -1) {
      // error
      printf("lseek error!\n");
      *size = 1024 * 100; // 100 Kb
    }

    // 커서 위치 초기화
    lseek(reqFileFd, 0, SEEK_SET);

    *buf = (char *) (malloc(
        sizeof(char) * (*size + 1)
    ));

    size_t end = read(
        reqFileFd,
        *buf,
        *size
    );

    (*buf)[end] = '\0';

    close(reqFileFd);
  }
}

// Checks if the client request is valid
// if success, @filename will be filled and return 0
// if error, response appropriate error to client and return -1
//
char* verifyRequest(int clientSockFd, int isStateful) {

  char requestBuf[1024] = {'\0', };
  char method[100];
  char path[100];
  char protocol[100];
  char *fileName;

  int reqBufLen = (int) recv(
      clientSockFd,
      requestBuf,
      sizeof(requestBuf),
      NO_BITWISE_FLAG
  );

  // Error should not stop the loop
  if (isHttpRecvErr(requestBuf, reqBufLen)) {

    // 아무것도 안보낼 수도 있나?
    if (reqBufLen == 0){ // Peer has shutdown!
      printf("XXXXX 클라이언트 : %d has shutdown the socket!\n", clientSockFd);
      return NULL;
    }

    handleError(
        clientSockFd,
        STATUS_BAD_REQUEST_CODE,
        CONTENT_TYPE_HTML,
        isStateful
    );

    return NULL;
  }

  requestBuf[reqBufLen - 1] = '\0';


  int scannedCnt = sscanf(
      requestBuf,
      HTTP_REQUEST_OVERVIEW_FORMAT,
      method,
      path,
      protocol
  );

  if (scannedCnt != 3) {

    printf(HTTP_OVERVIEW_FORMAT_ERR_MSG);

    handleError(
        clientSockFd,
        STATUS_BAD_REQUEST_CODE,
        CONTENT_TYPE_HTML,
        isStateful
    );

    return NULL;
  }

  // Check if supported method
  if (strcmp(method, HTTP_GET) != 0) {
    handleError(
        clientSockFd,
        STATUS_METHOD_NOT_ALLOWD_CODE,
        CONTENT_TYPE_HTML,
        isStateful
    );

    return NULL;
  }


  struct stat fileInfo;


  int status = checkValid(path, &fileName, &fileInfo);

  printf("after check valid! filename : %s\n", fileName);

  if (status != NO_ERROR) {
    printf("after check valid - status error!!\n");

    handleError(
        clientSockFd,
        status,
        CONTENT_TYPE_HTML,
        isStateful
    );

    return NULL;
  }


  return fileName;
}

// @clientSockFd 는 @isStateful 여부에 따라 닫히지 않도록 설정할 수 있다.
// @aux는 읽히고 닫힌다.
//
void handleRequest(int clientSockFd, void *aux, BlockingMode fileIoType, int isStateful) {

  char *requestedFileBuf = NULL;
  long fileSize;

  readRequestedFile(
      aux,
      &requestedFileBuf,
      &fileSize,
      fileIoType
  );

  response(
      clientSockFd,
      STATUS_OK_CODE,
      CONTENT_TYPE_HTML,
      requestedFileBuf,
      fileSize,
      isStateful
  );
}

// EPOLL Wrapper

void addNewEpollListen(struct epoll_event *events, int epollFdSet, int newFd) {

  // 나중에 malloc으로 변경
  struct epoll_event newFileReadEvent;

  newFileReadEvent.events = EPOLLIN | EPOLLET;
  newFileReadEvent.data.fd = newFd;

  epoll_ctl(epollFdSet, EPOLL_CTL_ADD, newFd, &newFileReadEvent);
}


int recvReqAndRegisterFileEpoll
(
    int clientSockFd,
    struct epoll_event *events,
    int epollFdSet,
    HashMap * fdHashMap
){

  // Error response is handled in the func
  // ** But Still connection has to be kept alive!!
  char *fileName = verifyRequest(clientSockFd, STATEFUL);
  if (fileName == NULL){
    return -1;
  }

  int reqFileFd = open(fileName, O_RDONLY | O_NONBLOCK);
  if (reqFileFd != -1) {
    // 요청한 파일의 I/O를 위해 오픈을 epoll에 등록한다
    addNewEpollListen(
        events,
        epollFdSet,
        reqFileFd
    );

    insertHashMap(fdHashMap, reqFileFd, clientSockFd);
  }

  free(fileName);

  return reqFileFd;
}


// I/O Functions

// Function Wrapper for Readability
void markFdIoType(HashMap *map, unsigned long fd, unsigned long ioType){
  insertHashMap(map, fd, ioType);
}

int isNetworkIo(HashMap *map, unsigned long fd){

  long ioType = searchHashMap(map, fd);

  return ioType == NETWORK_IO;
}

int isSupportedFileIoType(char * blockingType){
  return (strcmp(blockingType, FILE_IO_BLOCKING) == 0) ||
         (strcmp(blockingType, FILE_IO_NON_BLOCKING) == 0);
}


int isPeerModel(char *serverModel){
  return (strcmp(serverModel, SERVER_MODEL_PEER) == 0);
}

int isMasterModel(char *serverModel){
  return (strcmp(serverModel, SERVER_MODEL_MASTER)==0);
}

int isSupportedModel(char * modelName){
  return (strcmp(modelName, SERVER_MODEL_MASTER) == 0) ||
         (strcmp(modelName, SERVER_MODEL_PEER) == 0);
}

