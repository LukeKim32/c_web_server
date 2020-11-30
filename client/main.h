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

#define STATEFUL "stateful"
#define STATELESS "stateless"

#define UNIFORM_DISTRIBUTION_UPPER_BOUND 100
#define UNIFORM_DISTRIBUTION_LOWER_BOUND 1

typedef struct {
    int threadId;
    int requestCnt;
    char **htmlFileNames;
    int htmlFileCnt;
    char *hostname;
    long port;
    char *connStatus;
} ThreadArgs;

#define REQUEST_RECV_ERR -1
#define BAD_REQUEST 0


int isHttpRecvErr(char *reqBuf, int reqLen);
int isSupportedConnectionState(char *type);
int getUniformRand(int rangeLow, int rangeHigh);
int connectServer(char *hostname, int port);

void *requestToServer(void *args);
char **getHtmlFilesToRequest(FILE *fp, int *htmlFileCnt);
