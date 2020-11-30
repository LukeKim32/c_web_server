#include "main.h"

int main(int argc, char *argv[]) {

  if (argc != 7) {
    printf(ARGC_ERR_MSG);
    printf(COMMAND_INFO_MSG);
    return 0;
  }

  char *serverHostname = argv[ARG_HOSTNAME];

  long serverPort = strtol(
      argv[ARG_PORT],
      NULL,
      10
  );

  if (serverPort == STR_TO_LONG_ERR) {
    printf("Port number has a problem!\n");
    return 0;
  }

  char *connStatus = argv[ARG_CONN_STATE];
  if (!isSupportedConnectionState(connStatus)) {
    printf(CONNECTION_TYPE_ERR_MSG);
    return 0;
  }

  long threadPoolSize = strtol(
      argv[ARG_THREAD_CNT],
      NULL,
      10
  );

  if (threadPoolSize == STR_TO_LONG_ERR) {
    printf(THREAD_NUM_ERR_MSG);
    return 0;
  }

  long requestCntPerThread = strtol(
      argv[ARG_REQUEST_CNT_PER_THREAD],
      NULL,
      10
  );

  if (requestCntPerThread == STR_TO_LONG_ERR) {
    printf(REQUEST_CNT_ERR_MSG);
    return 0;
  }

  char *fileName = argv[ARG_FILE_NAME];
  FILE *fp = fopen(fileName, "r");
  if (fp == NULL) {
    printf("%s 를 열 수 없습니다.\n", fileName);
    return 0;
  }

  int htmlFileCnt = 0;

  char **htmlFileNames = getHtmlFilesToRequest(fp, &htmlFileCnt);
  if (htmlFileNames == NULL) {
    printf("요청할 파일명이 없습니다..\n");
    return 0;
  }

  pthread_t newThread[threadPoolSize];

  for (long i = 0; i < threadPoolSize; i++) {

    ThreadArgs *args = (ThreadArgs *) (malloc(sizeof(ThreadArgs)));
    args->threadId = i;
    args->requestCnt = requestCntPerThread;
    args->htmlFileNames = htmlFileNames;
    args->htmlFileCnt = htmlFileCnt;
    args->hostname = serverHostname;
    args->port = serverPort;
    args->connStatus = connStatus;

    int isCreated = pthread_create(
        &newThread[i],
        NULL,
        requestToServer,
        (void *) args
    );
    if (isCreated != 0) {
      printf(THREAD_POOL_CREATE_ERR_MSG);
      return -1;
    }
  }

  for (int i = 0; i < threadPoolSize; i++) {
    pthread_join(newThread[i], NULL);
  }


  for (int i = 0; i < htmlFileCnt; i++) {
    free(htmlFileNames[i]);
  }

  free(htmlFileNames);

  return 0;
}


int isHttpRecvErr(char *reqBuf, int reqLen) {

  return (reqLen == REQUEST_RECV_ERR)
         || (reqLen == BAD_REQUEST)
         || (strlen(reqBuf) == BAD_REQUEST);
}


int isSupportedConnectionState(char *type) {
  return (strcmp(type, STATEFUL) == 0) ||
         (strcmp(type, STATELESS) == 0);
}



int getUniformRand(int rangeLow, int rangeHigh) {

  return ((double) rand() / ((double) RAND_MAX + 1)) * (rangeHigh - rangeLow) + rangeLow;
}

int connectServer(char *hostname, int port) {

  int clientSockFd = socket(IP_V4, TCP, IP);

  if (clientSockFd == INVALID_SOCKET) {

    printf(SOCKET_CREATE_ERR_MSG);
    return -1;
  }

  struct sockaddr_in serverAddress;
  memset(&serverAddress, 0, sizeof(serverAddress));
  serverAddress.sin_family = IP_V4;
  serverAddress.sin_port = htons(port);
  serverAddress.sin_addr.s_addr = inet_addr(hostname);

  int optionOn = 1;
  int isOptionSet = setsockopt(
      clientSockFd,
      SOL_SOCKET,
      SO_REUSEADDR,
      &optionOn,
      sizeof(optionOn)
  );

  if (isOptionSet == -1) {
    // set error
    printf("소켓 옵션 설정이 되지 않습니다\n");
  }

  int isConnected = connect(
      clientSockFd,
      (struct sockaddr *) &serverAddress,
      sizeof(serverAddress)
  );


  if (isConnected == CONNECTION_SETUP_ERR) {
//    printf("서버(%s:%lu) 연결 실패! \n", hostname, port);
    return -1;
  }

  return clientSockFd;
}

void *requestToServer(void *args) {

  ThreadArgs *threadArgs = (ThreadArgs *) args;

  int requestCntPerThread = threadArgs->requestCnt;
  char **htmlFileNames = threadArgs->htmlFileNames;
  const int htmlFileCnt = threadArgs->htmlFileCnt;
  char *hostname = threadArgs->hostname;
  long port = threadArgs->port;
  char *connStatus = threadArgs->connStatus;
  // 요청 개수 만큼 반복
  // 임의이 시간만큼 슬립
  // 랜덤 파일을 요청한다

  srand((unsigned int) pthread_self());

  int interval = getUniformRand(
      UNIFORM_DISTRIBUTION_LOWER_BOUND,
      UNIFORM_DISTRIBUTION_UPPER_BOUND
  );

  unsigned int requestIntervalMs = interval * 1000;

  int clientSockFd = connectServer(hostname, port);
  if (clientSockFd == -1) {
    printf("소켓 생성에 실패하였습니다.\n");
    return NULL;
  }

  char msgBuf[1024 * 100];

  double timeSpent = 0;

  for (int i = 0; i < requestCntPerThread; i++) {

    char *htmlFileName = htmlFileNames[rand() % htmlFileCnt];
    char *htmlFilePath = (char *) (malloc(
        sizeof(char) * (strlen(htmlFileName) + 2)
    ));
    sprintf(htmlFilePath, "/%s", htmlFileName);


    if (i != 0 && (strcmp(connStatus, STATELESS) == 0)) {
      clientSockFd = connectServer(hostname, port);
      if (clientSockFd == -1) {
        printf("소켓 생성에 실패하였습니다.\n");
        break;
      }
    }

    memset(msgBuf, '\0', sizeof(msgBuf));

    sprintf(
        msgBuf,
        "GET %s HTTP/1.1\r\n"
        "Host : %s\r\n"
        "\r\n",
        htmlFilePath,
        CLIENT_NAME
    );

//    clock_t start = clock();

    int responseLen = send(
        clientSockFd,
        msgBuf,
        strlen(msgBuf),
        0
    );

    memset(msgBuf, '\0', sizeof(msgBuf));

    if (responseLen != RESPONSE_SEND_ERR) {

      int responseBufLen = (int) recv(
          clientSockFd,
          msgBuf,
          sizeof(msgBuf),
          0
      );

//      clock_t end = clock();
//      timeSpent += (double) (end - start) / CLOCKS_PER_SEC;

//      if (i == requestCntPerThread -1){
//        printf("%f\n", timeSpent);
//      }

      // Error should not stop the loop
      if (isHttpRecvErr(msgBuf, responseBufLen)) {
//      printf("[에러]응답 수신간 에러가 발생했습니다.\n");
//      printf("[안내]Peer model 서버는 stateful 옵션만이 정상 작동합니다.\n");
//      fflush(stdout);신
//        return NULL;

      } else {
        printf("응답 받음! (bytes : %lu)\n", strlen(msgBuf));
      }

    } else {
      printf("error in request send! errno : %d\n", errno);
    }


    free(htmlFilePath);

    if (strcmp(connStatus, STATELESS) == 0) {
//      shutdown(clientSockFd, SHUT_RDWR);
      close(clientSockFd);
    }

    usleep(requestIntervalMs);
  }

  if (strcmp(connStatus, STATELESS) == 0) {
//    shutdown(clientSockFd, SHUT_RDWR);
    close(clientSockFd);
  }

  free(threadArgs);
  pthread_exit(NULL);
  return NULL;
}

char **getHtmlFilesToRequest(FILE *fp, int *htmlFileCnt) {

  char **htmlFileNames = NULL;
  char htmlFileNameBuf[100];

  while (fgets(htmlFileNameBuf, sizeof(htmlFileNameBuf), fp) != NULL) {

    if (*htmlFileCnt == 0) {
      htmlFileNames = (char **) (malloc(sizeof(char *)));

    } else {

      htmlFileNames = realloc(
          htmlFileNames,
          sizeof(char *) * (*htmlFileCnt + 1)
      );
    }

    htmlFileNames[*htmlFileCnt] = (char *) (malloc(
        sizeof(char) * (strlen(htmlFileNameBuf) + 1))
    );

    if (htmlFileNameBuf[strlen(htmlFileNameBuf) - 1] == '\n') {
      htmlFileNameBuf[strlen(htmlFileNameBuf) - 1] = '\0';
    }

    strcpy(htmlFileNames[*htmlFileCnt], htmlFileNameBuf);

    printf(
        "Read filename : %s\n",
        htmlFileNames[*htmlFileCnt]
    );

    (*htmlFileCnt)++;

    memset(htmlFileNameBuf, '\0', sizeof(htmlFileNameBuf));
  }

  fclose(fp);

  return htmlFileNames;
}
