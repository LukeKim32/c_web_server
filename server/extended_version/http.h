#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#define HTTP_REQUEST_OVERVIEW_FORMAT "%[^ ] %[^ ] %[^ ]"
#define HTTP_GET "GET"
#define RESPONSE_SEND_ERR -1

#define FILE_INFO_OBTAINED 0
#define IP_V4 AF_INET
#define TCP SOCK_STREAM
#define IP 0

#define TRUE 1
#define FALSE 0
#define ERROR -1
#define NO_ERROR 1

#define NO_BITWISE_FLAG 0

#define ROOT_DIR '/'
#define SUPER_DIR_A ".."
#define SUPER_DIR_B "../"
#define SUPER_OF_ROOT "/../"
#define ELSE_SNOOP_PATH "/.."

#define BASE_FILE_PATH "./web_static_files"


#define ERROR_PAGE_TEMPLATE "<!DOCTYPE html>"\
                            "<html lang=\"en\">"\
                            "<head>"\
                            "<meta charset=\"UTF-8\">"\
                            "<title>%s</title>"\
                            "</head>"\
                            "<body>"\
                            "<h1>%s Error Page</h1>"\
                            "<p>"\
                            "Author : Yaechan Kim"\
                            "</p>"\
                            "</body>"\
                            "</html>"


#define RESPONSE_HEADER_TEMPLATE  "%s %d %s\r\n"\
                                  "Server : yaechan server\r\n"\
                                  "Content-length: %lu\r\n"\
                                  "Content-Type: %s\r\n"\
                                  "Connection: %s\r\n"\
                                  "Date : %s\r\n"\
                                  "\r\n"




#define STATUS_OK_CODE 200
#define STATUS_OK_MSG "OK"
#define STATUS_NOT_FOUND_CODE 404
#define STATUS_NOT_FOUND_MSG "NOT FOUND"
#define STATUS_BAD_REQUEST_CODE 400
#define STATUS_BAD_REQUEST_MSG "BAD REQUEST"
#define STATUS_SERVER_INTERNAL_ERR_CODE 500
#define STATUS_SERVER_INTERNAL_ERR_MSG "SERVER INTERNAL ERROR"
#define STATUS_METHOD_NOT_ALLOWD_CODE 405
#define STATUS_METHOD_NOT_ALLOWD_MSG "METHOD NOT ALLOWED"
#define CONTENT_TYPE_HTML "text/html"
#define NO_BODY "empty"


#define PROTOCOL "HTTP/1.1"
#define SERVER_NAME "yaechan server"
#define HEADER_OVERVIEW "%s %d %s"
#define HEADER_SERVER "Server : %s"
#define HEADER_CONTENT_LEN "Content-Length : %d"
#define HEADER_CONTENT_TYPE "Content-Type : %s"
#define HEADER_CONNECTION "Connection : %s"
#define CONNECTION_CLOSE "close"
#define CONNECTION_KEEP_ALIVE "keep-alive"
#define HEADER_DATE "\r\nDate : %s"
#define HEADER_DATE_FORMAT "%a, %d %b %Y %H:%M:%S %Z"
#define HEADER_END "\r\n"

#define MAX_HEADER_LEN 300

#define REQUEST_RECV_ERR -1
#define BAD_REQUEST 0

#define HTTP_OVERVIEW_FORMAT_ERR_MSG "Request method, path, protocol not scanned!\n"

#define STATELESS 0
#define STATEFUL 1

int isHttpRecvErr(char * reqBuf, int reqLen);
int hexit(char c);
void strdecode(char *to, char *from);
int isDirectory(struct stat *target);
int isSnooping(char *path);
int checkValid(char *path, char **fileName, struct stat *fileInfo);

char *makeResponseHeader(int statusCode, char *statusMsg, char *contentType, long contentLength, int isStateful);
void response(int sockFd, int statusCode, char * contentType, char * body, int bodySize, int isStateful);

void makeErrorPage (int statusCode, char **body, long *size);
void handleError(int clientSockFd, int statusCode, char *contentType, int isStateful);