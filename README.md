# c_web_server

보고서에도 실행 가이드 명시

## 주의 사항
1. Server 프로세스 실행 시 CLI 인자로 ```[서버 모델명]``` 반드시 명시
        
      ex. ```"master"``` or ```"peer"```

2. Client 프로세스 실행 시, CLI 인자인 ```[서버 연결 유지 여부]``` 값 반드시 올바르게 설정
        
      1) 서버 모델이 ```"master"``` 인 경우, ```"stateless"```로 반드시 설정 
        
      2) ```"peer"``` 모델일 경우, ```"stateful"```로 반드시 설정

      
---

## Server 실행 방법

1. ```server/``` 디렉토리 이동 > ```make``` command를 이용한 컴파일 (```Makefile```)

2. 실행 명령어 : ```./server.out [포트번호] [서버모델명] [쓰레드개수]```

    ```[포트 번호]``` : 서버가 Listen할 포트 번호
    
    ```[서버 모델]``` :  1) ```“master”``` : Master-Worker , 2) ```“peer”``` : Peer
    
    ```[쓰레드 개수]``` : ```“master”``` 모델의 경우, 워커 쓰레드의 개수 / ```“peer”``` 모델의 경우, 전체 쓰레드의 개수 (메인 쓰레드도 Peer에 포함)
    
    
    Command Example
    ./server.out 8080 master 4
    
    ./server.out 8080 peer 8
    

---

## Client 실행 방법

1. ```client/``` 디렉토리 이동 > ```make``` command를 이용한 컴파일 (```Makefile```)

2. 실행 명령어 : ```./client.out out [서버IP] [포트번호] [요청할 파일명들이 담긴 파일] [서버 연결 유지 여부] [쓰레드 개수] [쓰레드 별 요청 개수]```

    ```[서버 IP]``` : 연결을 요청할 서버 IP
    
    ```[포트 번호]``` : 서버가 Listen하는 포트 번호
    
    ```[요청할 파일명들이 담긴 파일]``` :  서버로 요청할 파일명 목록이 내용인 파일의 이름
    
    ```[서버 연결 유지 여부]``` : 
    1) ```“stateful”``` : 서버가 Peer 모델일 때 반드시 사용 (기존 커넥션 유지)
    2) ```“stateless”``` : 서버가 Master 모델일 때 반드시 사용 (요청 마다 커넥션 새로 생성)

    ```[쓰레드 개수]``` : 서버로 HTTP 요청을 보내는 것을 담당할 쓰레드 개수
    
    ```[쓰레드 별 요청 개수]``` : 각 쓰레드 별로 보낼 요청의 개수
        
    
    Command Example
    
    1) Server : "master"
    
    ./client.out 127.0.0.1 8080 html_files.txt stateless 10 10
    
    
    2) Server : "peer"
    
    ./client.out 127.0.0.1 8080 html_files.txt stateful 5 200
    
    
---

# Extra

## 추가 구현 Server 실행 방법

Cspro 환경에서 불안정, 테스트 어려움

### 개발 및 테스트 환경 
1. ```Docker Host``` : ```MacOS Big Sur (openBSD)```

2. ```Container Image``` : ```Ubuntu 16.04```

3. Serve할 HTML 파일들은 ```extended_version/web_static_files/``` 디렉터리에 추가

### 실행 방법
1. ```extended_version/``` 디렉토리로 이동

2. 도커 이미지 빌드 및 컨테이너 실행, 접속

    ```
    // Build Dockerfile in extended_version/
    docker build -t fake_cspro .

    // Run Container
    docker run --name fake_cspro -d -v {PWD}:/server fake_cspro:latest

    // Execute Bash Shell in Ubuntu container "fake_cspro" and enter
    docker exec -it fake_cspro /bin/bash
    ```

3. 컨테이너 내부 접속 후, ```make``` command를 이용한 Compile

4. 실행 명령어 : ```./server.out [포트번호] [서버모델명] [쓰레드개수] [File I/O Type]```

    ```[포트 번호]``` : 서버가 Listen할 포트 번호
    
    ```[서버 모델]``` :  1) ```“master”``` : Master-Worker , 2) ```“peer”``` : Peer
    
    ```[쓰레드 개수]``` : ```“master”``` 모델의 경우, 워커 쓰레드의 개수 / ```“peer”``` 모델의 경우, 전체 쓰레드의 개수 (메인 쓰레드도 Peer에 포함)
    
    ```[File I/O Type]``` : 
        
     1) ```“blocking”``` : Blocking File I/O 
     2) ```“non-blocking”``` : Non-Blocking File I/O (추가 구현모델)

    
    
    Command Example
    ./server.out 8080 master 4 non-blocking
    
    ./server.out 8080 peer 8 non-blocking

