//
//  DNSServer.hpp
//  MyDNS
//
//  Created by tegusi on 2017/12/7.
//  Copyright © 2017年 tegusi. All rights reserved.
//

#ifndef DNSServer_hpp
#define DNSServer_hpp

#include <cstdlib>
#include <stdio.h>
#include "DNSHeader.h"
#include "string.h"
#include <netdb.h>
#include <arpa/inet.h>

DNSAnswer setAnswer();
void recvAndSend(const char* dns_ip, unsigned int dns_port);
#endif /* DNSServer_hpp */
