//
//  mydns.cpp
//  MyDNS
//
//  Created by tegusi on 2017/12/6.
//  Copyright © 2017年 tegusi. All rights reserved.
//

#include <stdio.h>
#include "mydns.h"
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "DNSHeader.h"
#include "DNSClient.hpp"
//#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif
int sockfd;
sockaddr_in clientaddr,serveraddr;
/**
 * Initialize your client DNS library with the IP address and port number of
 * your DNS server.
 *
 * @param  dns_ip  The IP address of the DNS server.
 * @param  dns_port  The port number of the DNS server.
 * @param  client_ip  The IP address of the client
 *
 * @return 0 on success, -1 otherwise
 */
int init_mydns(const char *dns_ip, unsigned int dns_port, const char *client_ip)
{
    int res;
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_addr.s_addr = inet_addr(client_ip);
    
    //bind to given IP
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(dns_ip);
    serveraddr.sin_port = htons(dns_port);
    res = bind(sockfd, (sockaddr*)&clientaddr, sizeof(clientaddr));
    printf("res1 %d\n",res);
    res = connect(sockfd, (sockaddr*)&serveraddr, sizeof(serveraddr));
    printf("res1 %d\n",res);
    return res;
}


/**
 * Resolve a DNS name using your custom DNS server.
 *
 * Whenever your proxy needs to open a connection to a web server, it calls
 * resolve() as follows:
 *
 * struct addrinfo *result;
 * int rc = resolve("video.pku.edu.cn", "8080", null, &result);
 * if (rc != 0) {
 *     // handle error
 * }
 * // connect to address in result
 * mydns_freeaddrinfo(result);
 *
 *
 * @param  node  The hostname to resolve.
 * @param  service  The desired port number as a string.
 * @param  hints  Should be null. resolve() ignores this parameter.
 * @param  res  The result. resolve() should allocate a struct addrinfo, which
 * the caller is responsible for freeing.
 *
 * @return 0 on success, -1 otherwise
 */

#define BUFSIZE 1000

#include <cstdlib>
int resolve(const char *node, const char *service,
            const struct addrinfo *hints, struct addrinfo **res)
{
    using namespace std;
    (*res) = (addrinfo*) malloc(sizeof(addrinfo));
    (*res)->ai_flags = AI_PASSIVE;
    (*res)->ai_family = AF_INET;
    (*res)->ai_socktype = SOCK_STREAM;
    (*res)->ai_protocol = 0;
    (*res)->ai_addrlen = (socklen_t)sizeof(sockaddr);
    (*res)->ai_canonname = (char*)node;
    (*res)->ai_next = NULL;
    (*res)->ai_addr = (sockaddr*) malloc(sizeof(sockaddr));
    ((sockaddr_in*) (*res)->ai_addr)->sin_port = ntohs(atoi(service));
    char buf[BUFSIZE];
    
    //set dns header
    DnsHeader header = setHeader((uint16_t)htons(getpid()), 0);
    memcpy(buf, &header, sizeof(DnsHeader));
    
    //set query field
    char* qname = buf + sizeof(DnsHeader);
    hostnameToDnsQuery(node, qname);
    DNSQuery* query = (DNSQuery*)(qname + strlen(qname) + 1);
    query->qclass = htons(1);
    query->qtype = htons(1);
    
    //send packet
    uint16_t packetLength = sizeof(DnsHeader) + strlen(qname) + 1 + sizeof(DNSQuery);
    if(send(sockfd, buf, packetLength, 0) < 0)
    {printf("Sent failed\n");return -1;}
    
    //debug info

    int nbytes;
    nbytes = recv(sockfd, buf, BUFSIZE, 0);
//    printf("%d bytes\n",nbytes);
    uint8_t *cursor = (uint8_t*)buf + packetLength;
    DnsHeader *headerp = (DnsHeader*)buf;
//    printf("\nThe response contains : \n");
//    cout<<"qr:"<<(int)headerp->qr<<endl;
//    cout<<"id:"<<ntohs(headerp->id)<<endl;
//    cout<<"ans_cnt:"<<ntohs(headerp->ancount)<<endl;
//    cout<<"qd_cnt:"<<ntohs(headerp->qdcount)<<endl;
//    cout<<"rcode:"<<(int)headerp->rcode<<endl;
    //debug end
    while(cursor - (uint8_t*)buf < nbytes)
    {
        cursor = getIP(cursor,(uint8_t*) buf,res);
    }
    return 0;
}

/**
 * Release the addrinfo structure.
 *
 * @param  p  the addrinfo structure to release
 *
 * @return 0 on success, -1 otherwise
 */
int mydns_freeaddrinfo(struct addrinfo *p)
{
    if(p->ai_addr) free(p->ai_addr);
//    if(p->ai_canonname) free(p->ai_canonname);
    if(p) free(p);
    return 0;
}
