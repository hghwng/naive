//
// Created by wafer,robin on 11/14/15.
//
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <plg_api.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#define MAX_BUFFER 1024
int read_server(int sock, char *buffer);

int send_email(const char* receiver,
    const char* content,
    const char* title){
    int sockfd;
    struct sockaddr_in serv_addr;
    unsigned int server_port = 25;
    struct hostent *hostptr;
    struct in_addr *addrptr;
    char request[MAX_BUFFER];
    if((hostptr = (struct hostent *) gethostbyname("smtp.163.com")) == NULL){
        return -1;
    }

    addrptr = (struct in_addr *) *(hostptr->h_addr_list);
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = addrptr->s_addr;
    serv_addr.sin_port = htons(server_port);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
         return -1;
    }

    char reply[MAX_BUFFER];
    read_server(sockfd, reply);

    sprintf(request, "EHLO localhost\r\n");
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);


    sprintf(request, "Auth Login\r\n");
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    sprintf(request, "YXV0aG1haWxAMTYzLmNvbQ==\r\n");
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    sprintf(request, "cm1waW9yb2hnY3BjdmJldA==\r\n");
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    sprintf(request,"MAIL FROM: <authmail@163.com>\r\n");
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    sprintf(request, "RCPT TO: <%s>\r\n", receiver);
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    sprintf(request, "DATA\r\n");
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    sprintf(request, "From: \"authmail@163.com\" <authmail@163.com>\r\n\
To: \"%s\" <%s>\r\n\
Subject: %s\r\n\
X-Priority: 3\r\n\
X-GUID: FE00084E-4B5A-4135-8EBB-00F70098B526\r\n\
X-Has-Attach: no\r\n\
X-Mailer: Foxmail 7, 2, 7, 26[cn]\r\n\
Mime-Version: 1.0\r\n\
\r\n\
%s\r\n\
.\r\n\
", receiver, receiver, title, content);
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    sprintf(request, "quit\r\n");
    send(sockfd, request, strlen(request), 0);
    read_server(sockfd, reply);

    close(sockfd);
    return 0;
}

int read_server(int sock, char *buffer)
{
    char *p;
    int n = 0;

    if((n = recv(sock, buffer, MAX_BUFFER - 1, 0)) < 0)
    {
        perror("recv()");
        exit(-1);
    }

    if(n == 0)
    {
       printf("Connection lost\n");
       exit(-1);
    }

    buffer[n] = 0;

    p = strstr(buffer, "\r\n");

    if(p != NULL)
    {
        *p = 0;
    }
}

std::string generateAuthCode() {

    long long sysTime = (time(0) + 545454465454) % 1000000;

    char a[8];
    sprintf(a,"%06d",(int)sysTime);

    std::string s(a);


    return s;
}

PlgCustomAuthResponse::PlgCustomAuthResult check(std::string generateAuthCode, std::string inputAuthCode) {

    if (generateAuthCode == inputAuthCode) {
        return PlgCustomAuthResponse::kSuccess;
    }
    else if(generateAuthCode != inputAuthCode) {
        return PlgCustomAuthResponse::kFail;
    }
    else {
        return PlgCustomAuthResponse::kError;
    }
}


PlgCustomAuthResponse emailAuth_callback(PlgConf &conf) {
    PlgCustomAuthResponse resp;

    std::string AuthCode = generateAuthCode();
    send_email(conf["email"].c_str(), ("Your email auth code is " + AuthCode).c_str(), "Auth Code");
				

    std::string inputAuthCode;
    std::cout<<"Please input your Email Auth code"<<std::endl;
    std::cin>> inputAuthCode;

    resp.result = check(AuthCode, inputAuthCode);

    switch (resp.result) {
        case PlgCustomAuthResponse::kSuccess :
            resp.message = "Welcome";
            resp.result = PlgCustomAuthResponse::kSuccess;
            break;
        case PlgCustomAuthResponse::kFail :
            resp.message = "Incorrect code";
            resp.result = PlgCustomAuthResponse::kFail;
            break;
        case PlgCustomAuthResponse::kError :
            resp.message = "Unknown error";
            resp.result = PlgCustomAuthResponse::kError;
            break;
    }

    return resp;
}

bool plg_init(PlgInfo *info) {
    info->name = "emailauth";
    info->description = "The EMAIL Auth Way";
    info->auth_type = kCustomAuth;
    info->cust_auth_cb = emailAuth_callback;
    return true;
}
