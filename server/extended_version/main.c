#include "main.h"

RequestQueue requestQueue;

int main(int argc, char *argv[]) {

  //./server.out 8080 thread_num [master/peer] @unstable - [file I/O type : blocking/non-blocking]
  // peer 모델 같은 경우 accept는 non-blocking (새로 요청 들어온 거 accept와 이미 연결된 커넥션에서 요청 들어오는 epoll간 block을 막기 위해!)
  // master 모델의 경우 accept는 blocking! (어차피 master 쓰레드는 네트워크 I/O 만 하기 때문에)
  //
  // thread_num = 0 일 떄를 금지해야한다!
  //
  // master 모델의 경우, thread_num 은 워커 쓰레드의 개수를 나타내느 것으로, 값이 1이라면, 마스터 한개 워커 한개 해서 실제로는 전체로 2개다.
  // (네트워크 I/O와 클라이언트 요청을 분리해서 처리하기 때문이다)
  //
  // peer 모델의 경우, thread_num은 전체 쓰레드의 개수를 나타내는 것으로, 값이 1이라면 하나의 쓰레드로 accept, response를 모두 수행한다.
  // (네트워크 I/O와 클라이언트 요청을 합쳐서 처리하기 때문이다)

  if ((argc != 4)){
    printHelpMsg();
    return 0;
  }

  // "./server.out [포트번호] [서버 모델] [쓰레드 개수] [File I/O Type]\n"\

  long port = strtol(argv[ARG_PORT], NULL, 10);
  if (port == STR_TO_LONG_ERR){
    printf(PORT_NUM_ERR_MSG);
    printHelpMsg();
    return 0;
  }

  char * serverModel = argv[ARG_SERVER_MODEL];
  if (!isSupportedModel(serverModel)){
    printf("Server model argument error!\n");
    printHelpMsg();
    return 0;
  }

  long workersNum = strtol(argv[ARG_WORKER_NUM], NULL, 10);
  if (workersNum == STR_TO_LONG_ERR){
    printf(THREAD_POOL_SIZE_ERR_MSG);
    printHelpMsg();
    return 0;
  }

  if (workersNum < MIN_THREAD_NUM){
    printf("쓰레드 개수는 최소 1개 이상이어야 합니다.\n");
    printHelpMsg();
    return 0;
  }

  char * fileIoBlockType = argv[ARG_FILE_IO_BLOCKING_MODE];
  if (!isSupportedFileIoType(fileIoBlockType)){
    printf("File I/O argument error!\n");
    printHelpMsg();
    return 0;
  }

  BlockingMode fileIoType = getBlockingMode(fileIoBlockType);

  initReqQueue(&requestQueue);

  // Create Server Main Socket
  int serverSockFd = socket(IP_V4, TCP, IP);
  if (serverSockFd == INVALID_SOCKET) {
    printf(SOCKET_CREATE_ERR_MSG);
    return 0;
  }

  if (isPeerModel(serverModel)){
    // set socket as non-block
    setSocketNonBlocking(serverSockFd);
  }

  struct sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = IP_V4;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = inet_addr(HOST_IP);


  int isBinded = bind(
      serverSockFd,
      (struct sockaddr *) &address,
      sizeof(address)
  );

  if (isBinded == BIND_ERROR) {
    printf(SOCKET_BIND_ERROR_MSG);
    return 0;
  }

  int isListening = listen(serverSockFd, MAX_PENDING_QUEUE_SIZE);

  if (isListening == LISTEN_ERROR) {
    printf(SOCKET_LISTEN_ERROR_MSG);
    return 0;
  }

  // 1. 서버 소켓을 Non block으로 만든다.
  //
  // 2. 서버 소켓을 모든 쓰레드에게 공유한다.
  //
  // ====== 메인쓰레드 또한 워커 쓰레드로 합류 ===
  // ====== 이제 각 쓰레드 기준 ==============
  //
  // 3. 1) 새로운 커넥션 생성하 : 각 쓰레드는 서버 소켓에 요청이 들어왔는지 accept 를 한다 (이 때, sync를 맞춰야한다) (이전에 서버 소켓을 nonblocking 하지 않고, blocking accept를 할 경우, epoll로 등록해놓은 클라이언트 요청들에 대응하기전 막힐 수가 있다!)
  //      - 성공 할 경우, epoll_ctl (ADD)
  // 4. 2) 기존 연결된 커넥션에서 요청 받기 (epoll_wait 처리하기 (네트워크 요청이랑 file read 요청 구분해서 처리하))
  //
  // File I/O 가 blocking 일 경우 : (epoll_wait는 네트워크 요청만 수신할 것이다)
  //   - epoll_wait의 결과를 그대로 recv, read, response 해준다.
  //
  // File I/O 가 non blocking 일 경우
  // 5.   recv 해서 파일 open(nonblock) 한 뒤 epoll 등록하기 = epoll_ctl(ADD)
  // 6.   epoll_wait 처리하기


  // In Peer model, Main thread will be part of Thread pool
  // So the number of threads to create should be lessen.
  if (isPeerModel(serverModel)){
    workersNum --;
  }

  ServerInfo * serverInfo = makeServerInfo(
      serverModel,
      serverSockFd,
      fileIoType
  );

  int isOk = createThreadPool(
      workersNum,
      (void *) &requestQueue,
      serverInfo
  );

  if (isOk == THREAD_POOL_CREATE_ERR){
    return 0;
  }

  struct sockaddr_in clientAddress;
  socklen_t clientAddrSize = sizeof(clientAddress);

  initResource();

  // Start waiting incoming requests  - Blocking
  if (isMasterModel(serverModel)){

    while (1) {
//      printf("Master - Starting Blocking Accpet\n");

      int clientSockFd = accept(
          serverSockFd,
          (struct sockaddr *) &clientAddress,
          &clientAddrSize
      );

      if (clientSockFd == INVALID_SOCKET) {
        printf(ACCEPT_REQUEST_ERROR_MSG);
        return 0;
      }

      // Options for efficiently reusing opened socket
      // as after close() call, kernel will hold socket resources for a period
      // so, there might be binding problems
      // reusing socket option will solve this binding problems
      int optionOn = TRUE;
      int isOptionSet = setsockopt(
          clientSockFd,
          SOL_SOCKET,
          SO_REUSEADDR,
          &optionOn,
          sizeof(optionOn)
      );

      if (isOptionSet == SOCK_OPT_ERR) {
        // set error
      }

      // push client socket to queue
      enqueue(&requestQueue, clientSockFd);
    }

  } else { // Peer Model

    // In Peer Model, the main thread will also work as a peer

    ThreadArgs args;
    args.threadId = MAIN_THREAD_ID;
    args.requestQueue = &requestQueue;
    args.serverInfo = serverInfo;

    initThread((void *)&args);
  }

  // close server socket
  shutdown(serverSockFd, SHUT_RDWR);
  return 0;
//    return 0;

}


void setSocketNonBlocking(int sockFd){
  int flag = fcntl(sockFd, F_GETFL, 0);
  fcntl(sockFd, F_SETFL, flag | O_NONBLOCK);
}

ServerInfo  * makeServerInfo(char * serverModel, int serverSockFd, BlockingMode fileIoType){
  ServerInfo * serverInfo = (ServerInfo *)(malloc(sizeof(ServerInfo)));
  serverInfo->serverModel = (char *)(malloc(
      sizeof(char)*(strlen(serverModel)+1)
  ));
  strcpy(serverInfo->serverModel, serverModel);
  serverInfo->serverSockFd = serverSockFd;
  serverInfo->fileIoType = fileIoType;

  return serverInfo;
}

void printHelpMsg(){
  printf(ARGC_ERR_MSG);
  printf(COMMAND_INFO_MSG_VER_DEPLOY);
  printf(COMMAND_EXAMPLE_MSG_VER_DEPLOY);
}

BlockingMode getBlockingMode(char * fileIoType){
  if (strcmp(fileIoType, FILE_IO_BLOCKING) == 0){
    return BLOCKING;
  }
  return NON_BLOCKING;
}


void initResource(){

  struct rlimit sysResourceLimit;
  getrlimit(RLIMIT_NOFILE, &sysResourceLimit);
  sysResourceLimit.rlim_cur = MAX_FD_LIMIT;
  setrlimit(RLIMIT_NOFILE, &sysResourceLimit);
}
