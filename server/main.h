#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <dirent.h>

#include "requestQueue.h"
#include "http.h"
#include "worker.h"

#define HOST_IP "127.0.0.1"
#define ARG_PORT 1
#define ARG_SERVER_MODEL 2
#define ARG_WORKER_NUM 3
#define ARG_FILE_IO_BLOCKING_MODE 4
#define STR_TO_LONG_ERR 0L
#define INVALID_SOCKET -1
#define BIND_ERROR -1
#define LISTEN_ERROR -1
#define SOCK_OPT_ERR -1
#define MAX_FD_LIMIT 131072

#define MIN_THREAD_NUM 1

#define ARGC_ERR_MSG "3 arguments are required!\n"
#define COMMAND_INFO_MSG_VER_DEPLOY "./server.out [포트번호] [서버 모델] [쓰레드 개수]\n"\
                          " [포트번호] : 서버가 Listen 할 포트 번호\n"\
                          " [서버 모델] : 1)\"master\" : 마스터-워커 모델, 2)\"peer\" : 피어 모델\n"\
                          " [쓰레드 개수] : \"master\" 모델의 경우, 워커 쓰레드의 개수\n"\
                          "               \"peer\" 모델의 경우, 전체 쓰레드의 개수\n"

#define ARGC_ERR_MSG_VER_UNSTABLE "4 arguments are required!\n"
#define COMMAND_INFO_MSG_VER_UNSTABLE "./server.out [포트번호] [서버 모델] [쓰레드 개수] [File I/O Type]\n"\
                          " [포트번호] : 서버가 Listen 할 포트 번호\n"\
                          " [서버 모델] : 1)\"master\" : 마스터-워커 모델, 2)\"peer\" : 피어 모델\n"\
                          " [쓰레드 개수] : \"master\" 모델의 경우, 워커 쓰레드의 개수\n"\
                          "               \"peer\" 모델의 경우, 전체 쓰레드의 개수\n"                            \
                          " [File I/O Type] : 1)\"blocking\" : blocking File I/O function used\n"         \
                          "                   2)\"non-blocking\" : Non-blocking File I/O function used\n"

#define COMMAND_EXAMPLE_MSG_VER_DEPLOY "./server.out [8080] [2] [\"master\"/\"peer\"]\n"

#define COMMAND_EXAMPLE_MSG_VER_UNSTABLE "./server.out [8080] [2] [\"master\"/\"peer\"] [\"blocking\"/\"non-blocking\"]\n"

#define PORT_NUM_ERR_MSG "적절하지 않은 포트 번호입니다\n"
#define THREAD_POOL_SIZE_ERR_MSG "적절하지 않은 워커쓰레드 개수입니다.\n"
#define SOCKET_CREATE_ERR_MSG "Socket creation failed!\n"
#define SOCKET_BIND_ERROR_MSG "Socket bind failed!\n"
#define SOCKET_LISTEN_ERROR_MSG "Listening failed!\n"
#define ACCEPT_REQUEST_ERROR_MSG "Accept failed!\n"


void setSocketNonBlocking(int sockFd);
ServerInfo  * makeServerInfo(char * serverModel, int serverSockFd, BlockingMode fileIoType);
void printHelpMsg();
BlockingMode getBlockingMode(char * fileIoType);
void initResource();