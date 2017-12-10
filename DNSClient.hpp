//
//  DNSClient.hpp
//  MyDNS
//
//  Created by tegusi on 2017/12/6.
//  Copyright © 2017年 tegusi. All rights reserved.
//

#ifndef DNSClient_hpp
#define DNSClient_hpp

#include <cstdlib>
#include <stdio.h>
#include "DNSHeader.h"
#include "string.h"
#include <netdb.h>
#include <arpa/inet.h>
DnsHeader setHeader(uint16_t id,int qr);
void hostnameToDnsQuery(const char* src, char* dst);
uint8_t* getIP(uint8_t *rr,uint8_t *start,addrinfo** res);
#endif /* DNSClient_hpp */
