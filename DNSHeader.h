//
//  DNSHeader.h
//  MyDNS
//
//  Created by tegusi on 2017/12/6.
//  Copyright © 2017年 tegusi. All rights reserved.
//

#ifndef DNSHeader_h
#define DNSHeader_h

#include <sys/types.h>
#include <stdint.h>
struct DnsHeader {
    uint16_t id;
    
    uint8_t rd:1;
    uint8_t tc:1;
    uint8_t aa:1;
    uint8_t opcode:4;
    uint8_t qr:1;
    
    uint8_t rcode:4;
    uint8_t z:3;
    uint8_t ra:1;
    
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

struct DNSQuery {
    uint16_t qtype;
    uint16_t qclass;
};

struct DNSAnswer {
    uint16_t atype;
    uint16_t aclass;
    uint32_t attl;
    uint16_t ardlength;
};
DnsHeader setHeader(uint16_t id,int qr);
#endif /* DNSHeader_h */
