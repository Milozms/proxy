#include "csapp.h"
#include <sys/time.h>
#include <time.h>
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif
/* Recommended max cache and object sizes */
#define MAX_XML_FILE 102400
typedef struct{
    int connfd;
    int count;
} connarg;
void *thread(void* vargp);
void doit(int fd, int count);
int parse_url(const char* url, char* hostname, char* uri, char* port);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
int server_request(int clientfd, int fd, int count);
int open_clientfd_with_fake_ip(char *hostname, char *port, char *fake_ip);
int uri_transform(const char* orig_uri, char* new_uri);
int change_rate(const char* orig_uri, char* new_uri, int newrate);
void parse_xml_bitrates(int clientfd);
int choose_rate();
/* You won't lose style points for including this long line in your code */
static char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static char *conn_hdr = "Connection: close\r\n";
static char *proxy_conn_hdr = "Proxy-Connection: close\r\n";
char *logfilename, *fake_ip, *dns_ip, *dns_port, *www_ip;
FILE* logfile;
char* video_server_name = "video.pku.edu.cn";
char* video_server_port = "8080";
int bitrate_list[100];
int bitrate_count;
double alpha, throughput = 0;
int throughput_initialized = 0, video_start = 0, use_dns = 1;
time_t start_time;
//./proxy <log> <alpha> <listen-port> <fake-ip> <dns-ip> <dns-port> [<www-ip>]
int main(int argc, char **argv) 
{
    /* Check command line args */
    if(argc<7){
        fprintf(stderr, "Arguments error.\n");
        return 0;
    }
    logfilename = argv[1];
    alpha = atof(argv[2]);
    char* listen_port = argv[3];
    fake_ip = argv[4];
    dns_ip = argv[5];
    dns_port = argv[6];
    if(argc>7) {
        www_ip = argv[7];
        use_dns = 0;
    }
    int listenfd;
    connarg *conn;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    Signal(SIGPIPE, SIG_IGN);
    if((listenfd = open_listenfd(listen_port)) < 0){
        fprintf(stderr, "Open listenfd error.\n");
    }
    logfile = fopen(logfilename, "w");
    start_time = time(NULL);
    int count = 1;
    while (1) {
		clientlen = sizeof(clientaddr);
		conn = (connarg*)Malloc(sizeof(connarg));
		conn->connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
        conn->count = count;
        count++;
	    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
	                    port, MAXLINE, 0);
		Pthread_create(&tid, NULL, thread, conn);
    }
    fclose(logfile);
    return 0;
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
    struct timeval t_start, t_finish;
    gettimeofday(&t_start, NULL);
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], client_hdr[MAXLINE],
    hostname[MAXLINE], uri[MAXLINE], new_uri[MAXLINE], version[MAXLINE], port[MAXLINE];
    rio_t connrio;
    int size_of_chunk;
    double throughput_new;
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
        if (parse_url(url, hostname, uri, port) < 0) {
            fprintf(stderr, "url error.\n");
            Close(fd);
            return;
        }
        dbg_printf("hostname = %s, port = %s, uri = %s\n", hostname, port, uri);
        if (strcmp(hostname, video_server_name) == 0) {
            if(use_dns == 0)
                strcpy(hostname, www_ip);
        } else {
            fprintf(stderr, "url invalid.\n");
            return;
        }

        if (uri_transform(uri, new_uri) > 0) {
            //uri is xxx.f4m
            //need to fetch and parse the list of available encodings
            int clientfd = open_clientfd_with_fake_ip(hostname, port, fake_ip);
            if (clientfd < 0) {
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
            dbg_printf("request bitrate list to server is done.\n");
            parse_xml_bitrates(clientfd);
            int i;
            dbg_printf("Bitrate list:\n");
            for (i = 0; i < bitrate_count; ++i) {
                dbg_printf("%d\n", bitrate_list[i]);
            }
            Close(clientfd);
            strcpy(uri, new_uri);
        }

        int rate_chosen = choose_rate();
        dbg_printf("Choose rate = %d\n", rate_chosen);
        if(change_rate(uri, new_uri, rate_chosen) > 0){
            strcpy(uri, new_uri);
        }
        //deal with client-sent headers
        rio_readlineb(&connrio, client_hdr, MAXLINE);
        //rio_writen(clientfd, buf, strlen(buf));
        while (strcmp(client_hdr, "\r\n")) {
            rio_readlineb(&connrio, client_hdr, MAXLINE);
            //rio_writen(clientfd, buf, strlen(buf));
        }

        int clientfd = open_clientfd_with_fake_ip(hostname, port, fake_ip);
        if (clientfd < 0) {
            fprintf(stderr, "connect to server error\n");
            Close(fd);
            return;
        }

        //connect to server

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
        size_of_chunk = server_request(clientfd, fd, count);
        Close(clientfd);
        //request finish
        gettimeofday(&t_finish, NULL);
        double timeuse = 1000.0 * (t_finish.tv_sec - t_start.tv_sec) + (t_finish.tv_usec - t_start.tv_usec) / 1000.0;
        //ms
        throughput_new = (double) 8 * size_of_chunk / timeuse;
        //bit/ms -- kbit/s
        dbg_printf("chunk size = %d, time_use = %lf\n", size_of_chunk, timeuse);
        //update throughput
        if (throughput_initialized == 0) {
            throughput = throughput_new;
            throughput_initialized = 1;
        } else {
            throughput = alpha * throughput_new + (1 - alpha) * throughput;
        }
        dbg_printf("Throughput: %lf Kbps.\n", throughput);
        //<time> <duration> <tput> <avg-tput> <bitrate> <server-ip> <chunkname>
        time_t time_now = time(NULL);
        fprintf(logfile, "%ds %lfs %lf %lf %d %s %s\n",
                time_now-start_time, timeuse/1000, throughput_new, throughput, rate_chosen, hostname, new_uri);
        printf("%ds %lfs %lf %lf %d %s %s\n",
               time_now-start_time, timeuse/1000, throughput_new, throughput, rate_chosen, hostname, new_uri);

    }
    else{
        fprintf(stderr, "method error\n");
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }

}
/* $end doit */

int open_clientfd_with_fake_ip(char *hostname, char *port, char *fake_ip) {
    int clientfd;
    struct addrinfo hints, *listp, *p;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV;  /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG;  /* Recommended for connections */
    Getaddrinfo(hostname, port, &hints, &listp);

    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next) {
        /* Create a socket descriptor */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */
        /* Bind fake_ip*/
        struct sockaddr_in saddr;
        memset((void *) &saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons(0);
        saddr.sin_addr.s_addr = inet_addr(fake_ip); //bind ip
        if (bind(clientfd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
            fprintf(stderr, "Bind clientfd error.\n");
            Close(clientfd);
            continue;
        }
        /* Connect to the server */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break; /* Success */
        Close(clientfd); /* Connect failed, try another */  //line:netp:openclientfd:closefd
    }

    /* Clean up */
    Freeaddrinfo(listp);
    if (!p) /* All connects failed */
        return -1;
    else    /* The last connect succeeded */
        return clientfd;
}

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

int server_request(int clientfd, int fd, int count){
    char buf[MAXLINE];
    int len = 0, n;
    rio_t clientrio;
	//read from clientfd
    rio_readinitb(&clientrio, clientfd);
    while((n = rio_readnb(&clientrio, buf, MAXLINE)) > 0){
        rio_writen(fd, buf, n);
        len += n;
    }
    return len;
}

void parse_xml_bitrates(int clientfd){
    char xmlbuf[MAX_XML_FILE];
    int xmllen = 0, n;
    bitrate_count = 0;
    rio_t clientrio;
    //read from clientfd
    rio_readinitb(&clientrio, clientfd);
    while((n = rio_readnb(&clientrio, xmlbuf, MAXLINE)) > 0){\
        xmllen += n;
    }
    char* p;
    for(p = xmlbuf; p<xmlbuf+xmllen;++p){
        if(strncmp(p, "bitrate=", strlen("bitrate=")) == 0){
            char* bitrate_start = p + strlen("bitrate=") + 1;
            char* bitrate_end = index(bitrate_start, '"');
            if(bitrate_end == NULL){
                continue;
            }
            int bitrate_len = bitrate_end - bitrate_start;
            char bitrate_str[MAXLINE] = {0};
            strncpy(bitrate_str, bitrate_start, bitrate_len);
            bitrate_list[bitrate_count] = atoi(bitrate_str);
            bitrate_count++;
        }
    }
}

int uri_transform(const char* orig_uri, char* new_uri){
    char uri[MAXLINE];
    char* p;
    strcpy(uri, orig_uri);
    //add _nolist
    for(p = uri; *p; ++p){
        if(strncmp(p, ".f4m", strlen(".f4m"))==0){
            strcpy(p, "_nolist.f4m");
            dbg_printf("URI transformed to: %s\n", uri);
            strcpy(new_uri, uri);
            return 1;
        }
    }

    strcpy(orig_uri, uri);
    return 0;
}
int choose_rate(){
    if(bitrate_count <= 0) {
        return -1;
    }
    int i;
    if(bitrate_list[0]*1.5>throughput){
        return bitrate_list[0];
    }
    for(i=0;i<bitrate_count - 1; i++){
        if(bitrate_list[i]*1.5 <= throughput && bitrate_list[i+1]*1.5>throughput){
            return bitrate_list[i];
        }
    }
    return bitrate_list[bitrate_count - 1];
}
int change_rate(const char* orig_uri, char* new_uri, int newrate){
    char uri[MAXLINE] = {0};
    char old_rate_str[MAXLINE] = {0}, new_rate_str[MAXLINE] = {0}, prefix[MAXLINE] = {0}, postfix[MAXLINE] = {0};
    char *postfix_start, *bitrate_start;
    strcpy(uri, orig_uri);
    //change rate
    for(postfix_start = uri; *postfix_start; ++postfix_start){
        if(strncmp(postfix_start, "Seg", 3) == 0){
            for(bitrate_start = postfix_start; bitrate_start >= uri; --bitrate_start){
                if(*(bitrate_start - 1) == '/'){
                    break;
                }
            }
            if(newrate < 0){
                fprintf(stderr, "No bitrate list initialized, please clear the buffer of the browser.\n");
                exit(0);
            }
            strcpy(postfix, postfix_start);
            strncpy(prefix, uri, bitrate_start - uri);
            strncpy(old_rate_str, bitrate_start, postfix_start - bitrate_start);
            int old_rate = atoi(old_rate_str);
            dbg_printf("Old rate = %d\n", old_rate);
            if(old_rate == newrate){
                strcpy(new_uri, orig_uri);
                return 0;
            }
            sprintf(new_uri, "%s%d%s", prefix, newrate, postfix);
            dbg_printf("New uri = %s\n", new_uri);
            return 1;
        }
    }
    strcpy(new_uri, orig_uri);
    return 0;
}