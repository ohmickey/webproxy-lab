/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */

// open 함수는 filename 을 파일식별자로 변환하고 식별자 번호를 리턴한다. 리턴된 식별자는 항상 프로세스 내에서 현재 열려있지 않은 가장 작은 식별자이다.
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

void doit(int fd){  // Transaction 단위 처리, Tiny는 GET 메소드만 지원.
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;  // for client

    // Read request line and headers
    Rio_readinitb(&rio, fd); // rio 구조체 초기화, rio_fd -> fd(connfd), rio_bufptr -> rio_buf, rio_cnt = 0

    // Associate a descriptor with a read buffer and reset buffer.
    Rio_readlineb(&rio, buf, MAXLINE); // buf 에 MAXLINE (\n or EOF) 까지 rio에 있는 걸 쓴다.
    // buf 포인터 변하지 않는다.
    // buf 는 connfd 스트림에 입력되어있는 File 의 내용이 입력된다.
    // File에 제일 처음 들어오는 것은 GET / HTTP/1.1

    /*
    rio_read - This is a wrapper for the Unix read() function that transfers min(n, rio_cnt) bytes from an internal buffer to a user
    buffer, where n is the number of bytes requested by the user and rio_cnt is the number of unread bytes in the internal buffer. On
    entry, rio_read() refills the internal buffer via a call to read() if the internal buffer is empty.
    */

    printf("Request headers:\n"); // 표준 출력, \n 개행 문자를 만나서 출력 버퍼 비워짐.
    printf("%s", buf); // 출력 버퍼에 buf(method, uri, version) 담는다. buf 내용은 그대로 있음.
    sscanf(buf, "%s %s %s", method, uri, version);  // buf(소스문자열)에서 format을 지정하여 method, uri, version 으로 저장.
    // if (strcasecmp(method, "GET")){ // strcasecmp: 같으면 0 return (대소문자구분 X)
    if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {  // HEAD 메소드
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio); // request header
    // 요청 라인을 제외한 요청헤더들을 읽어서 출력버퍼에 넣어놓음(버퍼에 있는 쓰잘데기 없는 것들)

    // Parse URI from GET request
    is_static = parse_uri(uri, filename, cgiargs); // 콘텐츠가 정적(1)/동적(0) 인지 나타내는 flag 생성  // filename, cgiargs 의 값을 채운다.
    if (stat(filename, &sbuf) < 0){ // filename의 Attribute을 가져와서 &sbuf 에 저장. (성공시 0 / 실패시 -1)
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    if (is_static){ // Serve static content
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){ // 보통파일인지, 읽기 권한이 있는지
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, method);
    }
    else{ // Serve dynamic content
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs, method);
    }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
    char buf[MAXLINE], body[MAXBUF];

    // Build the HTTP response body
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    // Print the HTTP response
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf)); // 버퍼에 있는 내용을 fd에 써준다. buf 포인터는 그대로
    Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp){
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")){
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
    char *ptr;

    /* strstr(대상문자열, 검색할문자열);
    -> 문자열을 찾았으면 문자열로 "시작"하는 문자열의 "포인터"를 반환, 문자열이 없으면 NULL을 반환 */

    if (!strstr(uri, "cgi-bin")){ // 정적 콘텐츠
        strcpy(cgiargs, "");  // CGI 인자 스트링 지움 <= Copy "" to cgiargs
        strcpy(filename, "."); // filename = .        <= Copy "." to filename
        strcat(filename, uri); // filename = .uri     <= append uri onto filename
        // 상대 리눅스 경로이름으로 변환
        if (uri[strlen(uri)-1] == '/')  // uri가 '/' 로 끝나면 (첫 연결 이라면)
            strcat(filename, "home.html");  // filename = ./home.html
        return 1;
    }
    else{ // 동적 컨텐츠
        /*
        요청 헤더 예시는 다음과 같다.
        method(GET) / URI(GET ~ HTTP/1.1 사이) / version(HTTP/1.1) <- 공백 기준으로 구분.
        GET /cgi-bin/adder?first=11111&second=11111 HTTP/1.1
        */
        // index ? 없으면(인자 입력 X) NULL 반환
        ptr = index(uri, '?'); // /엔드포인트 이후에나오는 '?' 뒤에 쿼리스트링 파라미터들을 확인하기위해 '?' 인덱스를 가져온다.
        if (ptr){
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        }
        else // 동적컨텐츠인데, 쿼리스트링이 NULL
            strcpy(cgiargs, "");  // cgi 아그 스트링 지움.
        strcpy(filename, "."); // filename = "."
        strcat(filename, uri); // filename = "uri."   <-/cgi-bin/adder
        return 0;
    }
}

void serve_static(int fd, char *filename, int filesize, char *method) {
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
    /* Send response headers to client */
    get_filetype(filename, filetype);  // filetype 을 채워준다. (접미사를 입력해줌 ex -> .gif)
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    // 11.11
    if (strcasecmp(method, "HEAD") == 0) {
        return;
    }

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0); // O_RDONLY -> read only

    /*
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
    */

    // 11.9 반영
    srcp = (char*)Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize); // 파일을 클라이언트에게 보냄.
    free(srcp);
}

    /*
    * get_filetype - Derive file type from filename
    */
void get_filetype(char *filename, char *filetype){ // 파일형식 추출
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))
        strcpy(filetype, "video/mp4");
    else if (strstr(filename, ".mp3"))
        strcpy(filetype, "audio/mpeg");
    else
        strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) {
    char buf[MAXLINE], *emptylist[] = {NULL};

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { /* Child 생성*/

        // process id (pid) = 0 -> Child 프로세스

        /* Real server would set all CGI vars here */
        setenv("QUERY_STRING", cgiargs, 1); // none zero 시에 쿼리스트링을 cigiargs 를 덮어 씌운다.
        setenv("REQUEST_METHOD", method, 1);
        Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ // Duplicate FD to FD2, closing FD2 and making it open on the same file.
        Execve(filename, emptylist, environ); /* Run CGI program */
        // execve(char *path, char *const *ARGV[], char *const *ENVP[])
        // Replace the current process, executing PATH with arguments ARGV and environment ENVP. ARGV and ENVP are terminated by NULL pointers
    }
    Wait(NULL); /* Parent waits for and reaps child */
}

int main(int argc, char **argv) { // char *argv[] 형태로 받아도 동일.
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE]; // MAXLINE 8192
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    // printf("%d\n", sizeof(clientaddr));
    /*
    main 인자 argc, **argv 에 대해서,

        입력 값 : ./tiny 8000

        argc - 인자의 갯수 -> 2개
        argv - ["./tiny", "8000"] 배열
        배열을 인자로 받으려면 이중포인터로 넘겨받아야한다.

        argv[0] = "./tiny"
        argv[1] = "8000" -> 포트 번호

    */

    /* Check command line args */
    if (argc != 2) { // 인자가 2개가 안되면 포트번호 입력하라고 하고 종료해라
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    // else{
    //     printf("%d\n", argc);
    //     printf("%s, %s\n",argv[0],argv[1]);
    //     for (char i = 0; i < 2; i ++){
    //         printf("%d\n", &argv[i]);
    //     }
    //     printf("%d\n", *argv);
    //     printf("%d", **argv);
    //     exit(1);
    // }

    // argv[1] = 8000, 8000 포트에 서버용 듣기소켓 생성 후 , 듣기 소켓 파일 식별자 리턴해온다.
    listenfd = Open_listenfd(argv[1]);  // 듣기 식별자, getaddrinfo, socket, bind, listen 실행

    // 무한 서버 루프
    while (1) { // 서버에는 연결요청을 담는 큐가 있고, while 문을 돌면서 큐에서 하나씩 빼서 Accept를 시도하고 성공시에 연결소켓 오픈, 새로운 연결식별자를 리턴하고 Client <-> Server가 연결된다.
        clientlen = sizeof(clientaddr); // 128 ???!!
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // listenfd 의 connection peer의 주소를 client 소켓의 주소로 채우고, client 소켓의 길이의 주소또한 저장한다.
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);  // 소켓주소구조체 -> 호스트, 서비스이름(포트)의 스트링
        // 즉, Getnameinfo 호출시, 비어있던 hostname 과 port에 값을 채워온다.
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);   // 1 Transaction 처리, (GET, static, dynamic serve)
        Close(connfd);  // line:netp:tiny:close
    }
}
