#include "csapp.h"
#include <sys/socket.h>
#include <netdb.h>

#define WEB_PREFIX "http://"
#define NTHREADS 8

void handle_request(int connfd);
void parse_uri(char *uri, char *filename, char *host, char *port);
void read_requesthdrs(rio_t *rp);
void forward_request(char *method, char *host, char *port, char *filename, int connfd);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void *thread(void *vargp);

int main(int argc, char *argv[])
{

    int listenfd;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    pthread_t tid[NTHREADS];

    listenfd = Open_listenfd(argv[1]);

    while (1)
    {
        struct sockaddr_in clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (connfd < 0)
        {
            perror("accept");
            continue;
        }

        pthread_create(&tid, NULL, thread, (void *)&connfd);
    }
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());

    handle_request(connfd);

    close(connfd);

    return NULL;
}
void handle_request(int connfd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[MAXLINE];
    char filename[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;

    // replace HTTP/1.1 with HTTP/1.0
    char *pos = NULL;
    if ((pos = strstr(buf, "HTTP/1.1")) != NULL)
    {
        buf[pos - buf + strlen("HTTP/1.1") - 1] = '0';
    }
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET"))
        unix_error("Proxy does not implement this method");

    read_requesthdrs(&rio);

    parse_uri(uri, filename, host, port);

    forward_request(method, host, port, filename, connfd);
    return;
}

void forward_request(char *method, char *host, char *port, char *filename, int connfd)
{
    static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
    static const char *connection_hdr = "Connection: close\r\n";
    static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

    char buf[MAXLINE];
    char request[MAXLINE];

    sprintf(request, "%s %s HTTP/1.0\r\n", method, filename);
    sprintf(buf, "Host: %s\r\n", host);
    strcat(request, buf);
    sprintf(buf, "Filename: %s\r\n", filename);
    strcat(request, buf);
    sprintf(buf, "Method: %s\r\n", method);
    strcat(request, buf);
    strcat(request, user_agent_hdr);
    strcat(request, connection_hdr);
    strcat(request, proxy_connection_hdr);
    strcat(request, "\r\n");

    printf("Forwarding request:\n");
    printf("%s", request);

    //  splite localhost:223
    char *hostname = strtok(host, ":");

    int clientfd = Open_clientfd(hostname, port);

    rio_t rio;
    Rio_readinitb(&rio, clientfd);
    Rio_writen(rio.rio_fd, request, strlen(request));

    int n;
    char tinyResponse[MAXLINE];
    while ((n = Rio_readlineb(&rio, tinyResponse, MAXLINE)) != 0)
    {
        printf("%s", tinyResponse);
        Rio_writen(connfd, tinyResponse, n);
    }
}

void parse_uri(char *uri, char *filename, char *host, char *port)
{
    char *hostp = strstr(uri, WEB_PREFIX) + strlen(WEB_PREFIX);
    char *slash = strstr(hostp, "/");
    char *colon = strstr(hostp, ":");
    // get host name
    strncpy(host, hostp, slash - hostp);
    // get port number
    strncpy(port, colon + 1, slash - colon - 1);
    // get file name
    strcpy(filename, slash);
}

void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while (strcmp(buf, "\r\n"))
    { // line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}