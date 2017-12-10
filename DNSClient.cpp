//
//  DNSClient.cpp
//  MyDNS
//
//  Created by tegusi on 2017/12/6.
//  Copyright © 2017年 tegusi. All rights reserved.
//

#include "DNSClient.hpp"

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
        tmp.ancount = 1;
    }
    return tmp;
}

void hostnameToDnsQuery(const char* src, char* dst)
{
    int lock = 0 , i;
    for(i = 0 ; i < strlen((char*)src) ; i++)
    {
        if(src[i]=='.')
        {
            *dst++ = i-lock;
            for(;lock<i;lock++)
            {
                *dst++=src[lock];
            }
            lock++;
        }
        if(i == strlen(src) - 1)
        {
            *dst++ = i-lock+1;
            for(;lock<i;lock++)
            {
                *dst++=src[lock];
            }
            lock++;
        }
    }
    *dst++=src[i-1];
    *dst++='\0';
}

uint8_t* getIP(uint8_t *rr,uint8_t *start,addrinfo** res)
{
    char *name;
    int offset = 0;
    if(*rr >= 192)
    {
        offset = ((*rr) << 8) + *(rr + 1) - 49152;
        name = (char*)start + offset;
        rr += 2;
    }
    else
    {
        name = (char*)rr;
        rr += strlen(name);
    }
//    printf("%s\n",name);
    DNSAnswer *ans = (DNSAnswer*) rr;
//    printf("type%hu\n",ntohs(ans->atype));
//    printf("length%hu\n",ntohs(ans->ardlength));
//    printf("class%hu\n",ntohs(ans->aclass));
//    printf("%s\n",rr + sizeof(DNSAnswer) - 2);
    rr += sizeof(DNSAnswer) - 2;
    if(ntohs(ans->atype) == 1)
    {
        char tmps[10];
        sprintf(tmps,"%hu.%hu.%hu.%hu", *rr,*(rr+1),*(rr+2),*(rr+3));
//        printf("%s\n",tmps);
//        (*res)->ai_addr
        ((sockaddr_in*)((*res)->ai_addr))->sin_addr.s_addr = inet_addr(tmps);
//        ((sockaddr_in*)((*res)->ai_addr))->sin_port = 0;
        ((sockaddr_in*)((*res)->ai_addr))->sin_family = AF_INET;
//        inet_pton(AF_INET, tmps, &((*res)->ai_addr));
    }
    rr += ntohs(ans->ardlength);
    return rr;
}

