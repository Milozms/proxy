#include "csapp.h"
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define MAX_OBJECT_NUM 10
typedef struct{
    int connfd;
    int count;
} connarg;
void *thread(void* vargp);
void doit(int fd, int count);
int parse_url(const char* url, char* hostname, char* uri, char* port);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
void server_request(const char* url, int clientfd, int fd, int count);
/* You won't lose style points for including this long line in your code */
static char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static char *conn_hdr = "Connection: close\r\n";
static char *proxy_conn_hdr = "Proxy-Connection: close\r\n";
char *logfile, *fake_ip, *dns_ip, *dns_port, *www_ip;
char* video_server_name = "video.pku.edu.cn";
char* video_server_port = "8080";
double alpha;
//./proxy <log> <alpha> <listen-port> <fake-ip> <dns-ip> <dns-port> [<www-ip>]
int main(int argc, char **argv) 
{
    /* Check command line args */
    if(argc<7){
        fprintf(stderr, "Arguments error.\n");
        return 0;
    }
    logfile = argv[1];
    alpha = atof(argv[2]);
    char* listen_port = argv[3];
    fake_ip = argv[4];
    dns_ip = argv[5];
    dns_port = argv[6];
    if(argc>7){
        www_ip = argv[7];
    }
    int listenfd;
    connarg *conn;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    printf("%s", user_agent_hdr);

    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(listen_port);
    int count = 1;
    while (1) {
		clientlen = sizeof(clientaddr);
		conn = (connarg*)Malloc(sizeof(connarg));
		conn->connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
        conn->count = count;
        count++;
	    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
	                    port, MAXLINE, 0);
	    dbg_printf("Accepted connection from (%s, %s)\n", hostname, port);
		Pthread_create(&tid, NULL, thread, conn);
    }
}
/* $end main */

/*Thread routine*/
void *thread(void* vargp){
	connarg *conn = (connarg*)vargp;
    int fd = conn->connfd;
    int count = conn->count;
	Pthread_detach(pthread_self());
    Free(vargp);
    doit(fd, count);
    Close(fd);
    return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd, int count) 
{
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], client_hdr[MAXLINE],
    hostname[MAXLINE], uri[MAXLINE], version[MAXLINE], port[MAXLINE];
    rio_t connrio;
    int cache_index;
    /* Read request line and headers */
    rio_readinitb(&connrio, fd);
    if (!rio_readlineb(&connrio, buf, MAXLINE))
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, url, version);

	if(sscanf(buf, "%s %s %s", method, url, version) < 3){  
        fprintf(stderr, "sscanf error\n");  
        clienterror(fd, method, "404","Not Found", "Not Found");    
        Close(fd);  
        return;  
    }

    if (strcasecmp(method, "GET") == 0) {
        //parse url
        if(parse_url(url, hostname, uri, port) < 0){
            fprintf(stderr, "url error.\n");
            Close(fd);
            return;
        }
        printf("hostname = %s, port = %s, uri = %s\n", hostname, port, uri);
        if(strcmp(hostname, video_server_name)==0){
            strcpy(hostname, www_ip);
        }
        else {
            fprintf(stderr, "url invalid.\n");
            return;
        }

        //deal with client-sent headers
        rio_readlineb(&connrio, client_hdr, MAXLINE);
        //rio_writen(clientfd, buf, strlen(buf));
        while(strcmp(client_hdr, "\r\n")) {
            rio_readlineb(&connrio, client_hdr, MAXLINE);
            //rio_writen(clientfd, buf, strlen(buf));
        }
        //connect to server
        dbg_printf("breakpoint 1 : %s %s\r\n", hostname, port);
        int clientfd = open_clientfd(hostname, port);
        if(clientfd < 0){
            fprintf(stderr, "connect to server error\n");
            Close(fd);
            return;
        }
        //HTTP GET request
        sprintf(buf, "GET %s HTTP/1.0\r\n", uri);
        rio_writen(clientfd, buf, strlen(buf));
        //request headers
        sprintf(buf, "Host: %s\r\n", hostname);
        rio_writen(clientfd, buf, strlen(buf));
        rio_writen(clientfd, user_agent_hdr, strlen(user_agent_hdr));
        rio_writen(clientfd, conn_hdr, strlen(conn_hdr));
        rio_writen(clientfd, proxy_conn_hdr, strlen(proxy_conn_hdr));
        rio_writen(clientfd, "\r\n", strlen("\r\n"));
        printf("request to server is done.\n");

        //read from clientfd
        server_request(url, clientfd, fd, count);
        Close(clientfd);
    }
    else{
        fprintf(stderr, "method error\n");
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

}
/* $end doit */

/*
 *parse_url - split url into hostname, port and uri
 */
int parse_url(const char* url, char* hostname, char* uri, char* port){
	char tmpurl[MAXLINE];
	char *prefixstart, *uristart, *portstart;
	strcpy(tmpurl, url);
	prefixstart = index(tmpurl, '/');//first'/'
	if(prefixstart == NULL)
		return -1;
    if(prefixstart == tmpurl){
        //for reverse proxy
        //no hostname, only uri
        strcpy(hostname, video_server_name);
        strcpy(port, video_server_port);
        strcpy(uri, tmpurl);
        return 0;
    }
	prefixstart += 2;//second'/'
	uristart = index(prefixstart,'/');
	if(uristart == NULL)
		return -1;
	strcpy(uri, uristart);
	*uristart = '\0';
	portstart = index(prefixstart, ':');
	if(portstart == NULL){
		strcpy(hostname, prefixstart);
		strcpy(port, "80"); // default port
	}
	else{
		*portstart = '\0';
		portstart++;
		strcpy(port, portstart);
		strcpy(hostname, prefixstart);
	}
	return 0;
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

void server_request(const char* url, int clientfd, int fd, int count){
    char buf[MAXLINE];
	char tmpcontent[MAX_OBJECT_SIZE];
	int len = 0, n, i;
    rio_t clientrio;
    char *p = tmpcontent;
	//read from clientfd
    rio_readinitb(&clientrio, clientfd);
    while((n = rio_readnb(&clientrio, buf, MAXLINE)) > 0){
        rio_writen(fd, buf, n);
        if(len + n < MAX_OBJECT_SIZE){
            strncpy(p, buf, n);
            p += n;
        }
        len += n;
    }
}
