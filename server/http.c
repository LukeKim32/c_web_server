#include "http.h"

int isHttpRecvErr(char *reqBuf, int reqLen) {

//  printf("isHttpRecvErr() DEBUT === \n");
//  printf("ReqBuf : %s\n", reqBuf);
//  printf("ReqLen : %d\n", reqLen);
//  printf("is ReqLen error : %d\n",(reqLen == REQUEST_RECV_ERR));
//  printf("is ReqLen zero : %d\n",(reqLen == BAD_REQUEST));
//  printf("is reqbuf len zero : %d\n",(strlen(reqBuf) == BAD_REQUEST));


  return (reqLen == REQUEST_RECV_ERR)
         || (reqLen == BAD_REQUEST)
         || (strlen(reqBuf) == BAD_REQUEST);
}

int hexit(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;    /* shouldn't happen, we're guarded by isxdigit() */
}


void strdecode(char *to, char *from) {
  for (; *from != '\0'; ++to, ++from) {
    if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
      *to = hexit(from[1]) * 16 + hexit(from[2]);
      from += 2;
    } else
      *to = *from;
  }
  *to = '\0';
}

int isDirectory(struct stat *target) {
  return S_ISDIR(target->st_mode);
}


int isSnooping(char *path) {

  int len = strlen(path);

  return (path[0] == ROOT_DIR
          || strcmp(path, SUPER_DIR_A) == 0
          || strncmp(path, SUPER_DIR_B, 3) == 0
          || strstr(path, SUPER_OF_ROOT) != (char *) 0
          || strcmp(&(path[len - 3]), ELSE_SNOOP_PATH) == 0
  );
}


int checkValid(char *path, char **fileName, struct stat *fileInfo) {

  if (path[0] != '/') {
    return STATUS_BAD_REQUEST_CODE;
  }

  *fileName = &(path[1]);

  strdecode(*fileName, *fileName);

  if (*fileName[0] == '\0') {
    *fileName = "./";
  }

  // check invalid path and snoofinng
  if (isSnooping(*fileName)) {
    return STATUS_BAD_REQUEST_CODE;
  }

  int isFileInfoObtained = stat(*fileName, fileInfo);
  if (isFileInfoObtained != FILE_INFO_OBTAINED) {
    return STATUS_NOT_FOUND_CODE;
  }

  // is Path directory
  if (isDirectory(fileInfo)) {
    printf("file is directory\n");
    return STATUS_BAD_REQUEST_CODE;
  }

//  printf("checkValid() : %s %d\n", *fileName, fileInfo->st_mode);

  return NO_ERROR;

}


char *makeResponseHeader(int statusCode, char *statusMsg, char *contentType, long contentLength, int isStateful) {

  char *headerBuf = (char *) (malloc(
      sizeof(char) * MAX_HEADER_LEN
  ));

  char timeString[100];
  time_t now = time(0);
  struct tm gmt = *gmtime(&now);

  strftime(
      timeString,
      sizeof(timeString),
      HEADER_DATE_FORMAT,
      &gmt
  );

  char *connectionState = CONNECTION_CLOSE;
  if (isStateful == STATEFUL) {
    connectionState = CONNECTION_KEEP_ALIVE;
  }

  sprintf(
      headerBuf,
      RESPONSE_HEADER_TEMPLATE,
      PROTOCOL,
      statusCode,
      statusMsg,
      contentLength,
      contentType,
      connectionState,
      timeString
  );

  return headerBuf;
}


char *getStatusMsg(int statusCode) {
  switch (statusCode) {
    case STATUS_OK_CODE :
      return STATUS_OK_MSG;

    case STATUS_BAD_REQUEST_CODE :
      return STATUS_BAD_REQUEST_MSG;

    case STATUS_METHOD_NOT_ALLOWD_CODE :
      return STATUS_METHOD_NOT_ALLOWD_MSG;

    case STATUS_NOT_FOUND_CODE:
      return STATUS_NOT_FOUND_MSG;

    case STATUS_SERVER_INTERNAL_ERR_CODE :
      return STATUS_SERVER_INTERNAL_ERR_MSG;
  }
}

// response(clientSockFd, 404, "NOT FOUND", NULL, -1);
void response
(
    int sockFd,
    int statusCode,
    char *contentType,
    char *body,
    int bodySize,
    int isStateful
) {

  char *header = makeResponseHeader(
      statusCode,
      getStatusMsg(statusCode),
      contentType,
      bodySize,
      isStateful
  );

  char *totalBuf = (char *) (malloc(
      sizeof(char) * (strlen(header) + bodySize + 1
  )));

  sprintf(
      totalBuf,
      "%s%s",
      header,
      body
  );

  int responseLen = send(
      sockFd,
      totalBuf,
      strlen(totalBuf),
      0
  );

  if (responseLen == RESPONSE_SEND_ERR) {
    // Error
    printf("error in response header send! errno : %d\n", errno);
  }

  printf(">>>>> 클라이언트 (%d) 응답 완료\n", sockFd);

  if (isStateful == STATELESS) {
    shutdown(sockFd, SHUT_RDWR);
  }

  if (totalBuf != NULL) {
    free(totalBuf);
  }

  if (header != NULL) {
    free(header);
  }

  if (body != NULL) {
    free(body);
  }
}


void makeErrorPage(int statusCode, char **body, long *size) {

  const char *errMsg = getStatusMsg(statusCode);
  char errInfo[30];
  sprintf(errInfo, "%d %s", statusCode, errMsg);

  long pageSize = strlen(ERROR_PAGE_TEMPLATE) - 4 + (strlen(errInfo) * 2) + 2;

  *body = (char *) (malloc(
      sizeof(char) * pageSize
  ));

  *size = sprintf(
      *body,
      ERROR_PAGE_TEMPLATE,
      errInfo,
      errInfo
  );

  // For Null
//  *size += 1;
}


void handleError(int clientSockFd, int statusCode, char *contentType, int isStateful) {

  char *errorPage = NULL;
  long errorPageSize;

  makeErrorPage(
      statusCode,
      &errorPage,
      &errorPageSize
  );

  response(
      clientSockFd,
      statusCode,
      contentType,
      errorPage,
      errorPageSize,
      isStateful
  );
}
