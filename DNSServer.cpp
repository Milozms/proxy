//
//  DNSServer.cpp
//  MyDNS
//
//  Created by tegusi on 2017/12/7.
//  Copyright © 2017年 tegusi. All rights reserved.
//

#include "DNSServer.hpp"

DNSAnswer setAnswer()
{
    DNSAnswer tmp;
    memset(&tmp,0,sizeof(tmp));
    tmp.atype = htons(1);
    tmp.aclass = htons(1);
    tmp.attl = 0;
    tmp.ardlength = htons(4);
    return tmp;
}

DnsHeader setHeader(uint16_t id,int qr)
{
    DnsHeader tmp;
    memset(&tmp, 0, sizeof(tmp));
    tmp.rd = tmp.z =tmp.ra=tmp.nscount = tmp.arcount= 0;
    tmp.opcode = tmp.tc = 0;
    if(qr == 0) //query
    {
        tmp.qr = 0;
        tmp.rd = 1;
        tmp.aa = 0;
        tmp.opcode = 0;
        tmp.qdcount = htons(1);
        tmp.ancount = 0;
    }
    if(qr == 1) //response
    {
        tmp.qr = 1;
        tmp.aa = 1;
        tmp.rcode = 0;
        tmp.opcode = 0;
        tmp.qdcount = 0;
        tmp.ancount = htons(1);
    }
    return tmp;
}

void recvAndSend(const char* dns_ip, unsigned int dns_port)
{
    sockaddr_in serveraddr,clientaddr;
    socklen_t clientlen;
    int sockfd;
    int res;
    char buf[1000];
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(dns_ip);
    serveraddr.sin_port = htons(dns_port);
    res = bind(sockfd, (sockaddr*)&serveraddr, sizeof(serveraddr));
    
    printf("res1 %d\n",res);
//    if(res < 0)
//    {
//        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
//        exit(1);
//    }
    while (1) {
        int nbytes = recvfrom(sockfd, buf, 900, 0, (sockaddr*)&clientaddr, &clientlen);
        printf("%d\n",nbytes);
        if(nbytes < 0) continue;
        
        DnsHeader tmpHeader = setHeader(0,1);
        memcpy(buf,&tmpHeader,sizeof(DnsHeader));
        char* name = buf + sizeof(DnsHeader);
        DnsHeader* header = (DnsHeader*) buf;
        if(name[0] != 5 || name[1] != 'v' || name[2] != 'i')
        {
            header->rcode = htons(3);
            res = sendto(sockfd, buf, nbytes, 0, (sockaddr*)&clientaddr, clientlen);
            printf("%d\n",res);
        }
        else
        {
            uint8_t* rdata = (uint8_t *)buf + nbytes;
            *rdata++ = 192;
            *rdata++ = sizeof(DnsHeader);
            DNSAnswer ans = setAnswer();
            memcpy(rdata, &ans, sizeof(DNSAnswer));
            rdata += sizeof(DNSAnswer) - 2;
            *((uint32_t*)rdata) = serveraddr.sin_addr.s_addr;
            rdata += 4;
            res = sendto(sockfd, buf, (char*)rdata - buf, 0, (sockaddr*)&clientaddr, clientlen);
            printf("%d\n",res);
        }
    }
}
