#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>



#define ARGC_ERR_MSG "6 arguments are required!\n"
#define COMMAND_INFO_MSG "./client.out [서버 IP] [포트번호] [요청할 파일명들 담긴 파일][서버 연결 유지 여부] [쓰레드 개수] [쓰레드 별 요청 개수]\n" \
                          " [서버 IP] : 연결을 요청할 서버 ip\n"\
                          " [포트번호] : 서버가 Listen 할 포트 번호\n"\
                          " [요청할 파일명들 담긴 파일] : 서버로 요청할 파일명 목록이 내용인 파일명\n"\
                          " [서버 연결 유지 여부] : 1)\"stateful\" : 각 쓰레드가 요청들을 보낼 때, 기존 커넥션 이용/유지\n"\
                          "                    2)\"stateless\" : 각 쓰레드가 매 요청마다 새로 커넥션 생성/끊기\n"                            \
                          " [쓰레드 개수] : 서버로 http 요청을 보내는 것을 담당할 쓰레드 개수\n"         \
                          " [쓰레드 별 요청 개수] : 각 쓰레드 별로 보낼 요청의 개수\n"                                             \
                          "  (총 요청 개수 : 쓰래드 개수 * 쓰레드 별 요청 개수)"

// argument [hostname] [port] [file of html file names] [num of threads] [request cnt of each thread]
#define ARG_HOSTNAME 1
#define ARG_PORT 2
#define ARG_FILE_NAME 3
#define ARG_CONN_STATE 4
#define ARG_THREAD_CNT 5
#define ARG_REQUEST_CNT_PER_THREAD 6
#define STR_TO_LONG_ERR 0L
#define THREAD_NUM_ERR_MSG "쓰레드 개수가 잘못되었습니다.\n"
#define REQUEST_CNT_ERR_MSG "각 쓰레드가 보낼 요청의 개수가 잘못되었습니다.\n"
#define CONNECTION_TYPE_ERR_MSG "서버 연결 유지 여부 옵션이 잘못되었습니다.\n"


#define FILE_OPEN_ERR_MSG "file could not be opened!"
#define THREAD_POOL_CREATE_ERR_MSG "쓰레드 풀 생성 실패\n"
#define THREAD_POOL_CREATE_OK_MSG "Thread pool 생성 성공!\n"

#define HTTP_REQUEST_OVERVIEW_FORMAT "%[^ ] %[^ ] %[^ ]"
#define HTTP_GET "GET"
#define RESPONSE_SEND_ERR -1

#define FILE_INFO_OBTAINED 0
#define IP_V4 AF_INET
#define TCP SOCK_STREAM
#define IP 0
#define INVALID_SOCKET -1

#define SOCKET_CREATE_ERR_MSG "Socket creation failed!\n"
#define CONNECTION_SETUP_ERR -1
#define CLIENT_NAME "localhost"

typedef struct {
    int threadId;
    int requestCnt;
    char **htmlFileNames;
    int htmlFileCnt;
    char * hostname;
    long port;
    char * connStatus;
} ThreadArgs;

#define REQUEST_RECV_ERR -1
#define BAD_REQUEST 0

int isHttpRecvErr(char * reqBuf, int reqLen){

  return  (reqLen == REQUEST_RECV_ERR)
          || (reqLen == BAD_REQUEST)
          || (strlen(reqBuf) == BAD_REQUEST);
}

#define STATEFUL "stateful"
#define STATELESS "stateless"

int isSupportedConnectionState(char * type) {
  return (strcmp(type, STATEFUL) == 0) ||
      (strcmp(type, STATELESS) == 0);
}

#define UNIFORM_DISTRIBUTION_UPPER_BOUND 100
#define UNIFORM_DISTRIBUTION_LOWER_BOUND 1


int getUniformRand(int rangeLow, int rangeHigh) {

  return ((double) rand() / ((double)RAND_MAX + 1)) * (rangeHigh-rangeLow) + rangeLow;
}

int connectServer(char * hostname, int port) {
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

  int isConnected = connect(
      clientSockFd,
      (struct sockaddr*)&serverAddress,
      sizeof(serverAddress)
  );

  if (isConnected == CONNECTION_SETUP_ERR){
    printf("서버(%s:%lu) 연결 실패! \n", hostname, port);
    return -1;
  }

  return clientSockFd;
}

static void * requestToServer(void * args){
  ThreadArgs* threadArgs = (ThreadArgs *)args;

  int requestCntPerThread = threadArgs->requestCnt;
  char ** htmlFileNames = threadArgs->htmlFileNames;
  const int htmlFileCnt = threadArgs->htmlFileCnt;
  char * hostname = threadArgs->hostname;
  long port = threadArgs -> port;
  char * connStatus = threadArgs->connStatus;
  // 요청 개수 만큼 반복
  // 임의이 시간만큼 슬립
  // 랜덤 파일을 요청한다

  srand((unsigned int) pthread_self());

  int interval = getUniformRand(
      UNIFORM_DISTRIBUTION_LOWER_BOUND,
      UNIFORM_DISTRIBUTION_UPPER_BOUND
  );

  printf("쓰레드 (id : %d) 시작!\n", threadArgs->threadId);

  unsigned int requestIntervalMs = interval * 1000;
  printf("쓰레드 (id : %d)의 요청 간격 : %dms\n", threadArgs->threadId, interval);


  int clientSockFd = connectServer(hostname, port);

  char msgBuf[1024 * 100]; // REQUEST RESPONSE BUFFER SIZE is 100kb

  for (int i=0; i<requestCntPerThread; i++){

    // Ask for random html file
    char * htmlFileName = htmlFileNames[i % htmlFileCnt];//htmlFileNames[rand() % htmlFileCnt];
    char * htmlFilePath = (char *)(malloc(sizeof(char)* (strlen(htmlFileName) + 2)));
    sprintf(htmlFilePath, "/%s", htmlFileName);

    // Create Client Socket
    if (i!=0 && (strcmp(connStatus, STATELESS)==0)){
      clientSockFd = connectServer(hostname, port);
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

    printf("클라이언트 쓰레드 (id : %d) 의 %d-번째 요청! %s\n", threadArgs->threadId, i+1, htmlFileName);
//    printf("클라이언트 쓰레드 (id : %d) 의 요청 : %s\n", threadArgs->threadId, msgBuf);
    fflush(stdout);

    // send
    int responseLen = send(
        clientSockFd,
        msgBuf,
        strlen(msgBuf),
        0
    );

    if (responseLen == RESPONSE_SEND_ERR) {
      // Error
      printf("error in request send! errno : %d\n", errno);
    }


    memset(msgBuf, '\0', sizeof(msgBuf));

    int responseBufLen = (int) recv(
        clientSockFd,
        msgBuf,
        sizeof(msgBuf),
        0
    );

    // Error should not stop the loop
    if (isHttpRecvErr(msgBuf, responseBufLen)){
      printf("Receiving Response error!\n");
      fflush(stdout);
      return NULL;
    }

//    char tmpBuf[50];
//    strncpy(tmpBuf, msgBuf, sizeof(tmpBuf)-1);
    printf("쓰레드(id:%d)의 %d-번째 요청 응답 받음! (bytes : %d) %s\n", threadArgs->threadId, i+1, strlen(msgBuf), msgBuf);
    fflush(stdout);
//    printf("요청 결과(응답) : %s\n", msgBuf);


    free(htmlFilePath);

    usleep(requestIntervalMs);
  }

  if (strcmp(connStatus, STATEFUL)==0){
    shutdown(clientSockFd, SHUT_RDWR);
  }

  free(threadArgs);
  pthread_exit(NULL);
  return NULL;
}

char ** getHtmlFilesToRequest(FILE *fp, int * htmlFileCnt){

  char **htmlFileNames = NULL;
  char htmlFileNameBuf[100];

  while (fgets(htmlFileNameBuf, sizeof(htmlFileNameBuf), fp) != NULL){

    if (*htmlFileCnt == 0){
      htmlFileNames = (char **)(malloc(sizeof(char *)));

    } else {
      htmlFileNames = realloc(htmlFileNames, sizeof(char *) * (*htmlFileCnt + 1));
    }

    htmlFileNames[*htmlFileCnt] = (char *)(malloc(
        sizeof(char) * (strlen(htmlFileNameBuf) + 1))
    );

    if (htmlFileNameBuf[strlen(htmlFileNameBuf) - 1] == '\n'){
      htmlFileNameBuf[strlen(htmlFileNameBuf) - 1] = '\0';
    }

    strcpy(htmlFileNames[*htmlFileCnt], htmlFileNameBuf);
    printf("Read filename : %s\n", htmlFileNames[*htmlFileCnt]);
    (*htmlFileCnt) ++;

    memset(htmlFileNameBuf, '\0', sizeof(htmlFileNameBuf));
  }

  fclose(fp);

  return htmlFileNames;
}

// argument [hostname] [port] [file of html file names] [num of threads] [alive] [request cnt of each thread]
int main(int argc, char *argv[]) {

  if (argc != 7){
    printf(ARGC_ERR_MSG);
    printf(COMMAND_INFO_MSG);
    return 0;
  }

  char * serverHostname = argv[ARG_HOSTNAME];

  long serverPort = strtol(
      argv[ARG_PORT],
      NULL,
      10
  );

  if (serverPort == STR_TO_LONG_ERR){
    printf("Port number has a problem!\n");
    return 0;
  }

  char * connStatus = argv[ARG_CONN_STATE];
  if (!isSupportedConnectionState(connStatus)){
    printf(CONNECTION_TYPE_ERR_MSG);
    return 0;
  }

  long threadPoolSize = strtol(
      argv[ARG_THREAD_CNT],
      NULL,
      10
  );

  if (threadPoolSize == STR_TO_LONG_ERR){
    printf(THREAD_NUM_ERR_MSG);
    return 0;
  }

  long requestCntPerThread = strtol(
      argv[ARG_REQUEST_CNT_PER_THREAD],
      NULL,
      10
  );

  if (requestCntPerThread == STR_TO_LONG_ERR){
    printf(REQUEST_CNT_ERR_MSG);
    return 0;
  }

  char * fileName = argv[ARG_FILE_NAME];
  FILE * fp = fopen(fileName, "r");
  if (fp == NULL){
    printf("%s 를 열 수 없습니다.\n", fileName);
    return 0;
  }

  int htmlFileCnt = 0;

  char ** htmlFileNames = getHtmlFilesToRequest(fp, &htmlFileCnt);
  if (htmlFileNames == NULL){
    printf("요청할 파일명이 없습니다..\n");
    return 0;
  }

  pthread_t newThread[threadPoolSize];

  for(long i = 0; i < threadPoolSize; i++) {

    ThreadArgs * args = (ThreadArgs *)(malloc(sizeof(ThreadArgs)));
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
    if (isCreated != 0){
      printf(THREAD_POOL_CREATE_ERR_MSG);
      return -1;
    }
  }

  for (int i = 0; i < threadPoolSize; i++){
    pthread_join(newThread[i], NULL);
  }


  for (int i=0; i<htmlFileCnt; i++){
    free(htmlFileNames[i]);
  }

  free(htmlFileNames);

  return 0;
}
