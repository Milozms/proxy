//
//  main.cpp
//  MyDNS
//
//  Created by tegusi on 2017/12/5.
//  Copyright © 2017年 tegusi. All rights reserved.
//
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
#include "DNSServer.hpp"
#include "mydns.h"
#include <ctime>
#include <errno.h>
#define RDRB 0
#define GEODIS 1
#define BUFSIZE 1024

using namespace std;
int method;
string logPath,ServerPath,LsaPath,myIP;
int port;
vector<string> serverIPs;

void loadServerIPs(string path)
{
    fstream IPFile;
    IPFile.open(path,ios::in);
    string IP,num;
    while (!IPFile.eof()) {
        IPFile >> IP;
        serverIPs.push_back(IP);
    }
    serverIPs.pop_back();
    IPFile.close();
}

vector<string> splitStr(string str,char symbol)
{
    vector<string> ans;
    string tmp = "";
    for(int i = 0;i < str.size();i++)
    {
        if(str[i] != symbol)
        {
            tmp+=str[i];
        }
        else
        {
            ans.push_back(tmp);
            tmp = "";
        }
    }
    if(tmp != "") ans.push_back(tmp);
    return ans;
}

map<string, map<string, int> > Graph;
void readAndMinPath(string path)
{
    fstream IPFile;
    IPFile.open(path,ios::in);
    string src,seq,dsts_;
    vector<string> dsts;
    int cnt = 0;
    while (!IPFile.eof()) {
        cnt++;
        IPFile >> src >> seq >> dsts_;
        dsts = splitStr(dsts_, ',');
        for(auto i=dsts.begin();i != dsts.end();i++)
        {
            string dst = *i;
            Graph[src][dst] = Graph[dst][src] = 1;
        }
    }
    for(auto _i = Graph.begin();_i != Graph.end();_i++)
    {
        string i = _i->first;
        for(auto _j = Graph.begin();_j != Graph.end();_j++)
        {
            string j = _j->first;
            if(_i->second.find(j) == _i->second.end()) _i->second[j]= 10000;
        }
    }
    for(auto _k = Graph.begin();_k != Graph.end();_k++)
    {
        string k = _k->first;
        for(auto _j = Graph.begin();_j != Graph.end();_j++)
        {
            string j = _j->first;
            for(auto _i = Graph.begin();_i != Graph.end();_i++)
            {
                string i = _i->first;
                Graph[i][j] = min(Graph[i][k] + Graph[k][j],Graph[i][j]);
            }
        }
    }
    IPFile.close();
}

int ip_cnt = 0;
void getIP(const char* ip,int method,char* dst)
{
    //round-robin
    if(method == RDRB)
    {
        ip_cnt = (ip_cnt + 1) % serverIPs.size();
        memcpy(dst,serverIPs[ip_cnt].c_str(),serverIPs[ip_cnt].length());
    }
    //find nearest neighbor
    else
    {
        string i(ip);
        int maxv = 1000;
        string ans;
        for(auto _j = Graph.begin();_j != Graph.end();_j++)
        {
            string j = _j->first;int flag = 0;
            for(auto _ = serverIPs.begin();_ != serverIPs.end();_++)
            {
                if(*_ == j) {flag = 1;break;}
            }
            if(flag && Graph[i][j] < maxv)
            {
                maxv = Graph[i][j];
                ans = j;
            }
        }
        memcpy(dst,ans.c_str(),ans.length());
    }
}
int main(int argc, const char * argv[]) {
    if(argc == 7)
        method = RDRB;
    else
        method = GEODIS;
    
    //load args
    logPath = string(argv[argc-5]);
    myIP = string(argv[argc-4]);
    port = atoi(argv[argc-3]);
    ServerPath = string(argv[argc-2]);
    LsaPath = string(argv[argc-1]);
    
    readAndMinPath(LsaPath);
    loadServerIPs(ServerPath);
    time_t start = time(NULL);
    fstream fout;
    fout.open(logPath,ios::out);
    
    //set up socket
    cout<<"Socket establishing..."<<endl;
    sockaddr_in serveraddr;
    socklen_t clientlen = sizeof(serveraddr);
    int sockfd,res;
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(myIP.c_str());
    serveraddr.sin_port = htons(port);
    res = bind(sockfd, (sockaddr*)&serveraddr, sizeof(serveraddr));
    if(res == 0)
        cout<<"Socket establibed"<<endl;
    cout<<"IP: "<<myIP<<endl<<"Port: "<<port<<endl;
    
    while (1) {
        sockaddr_in clientaddr,cdnserver;
        char buf[BUFSIZE],clientIP[10],cdnIP[4];
        memset(&clientaddr,0,sizeof(clientaddr));
        int nbytes = recvfrom(sockfd, buf, BUFSIZE, 0, (sockaddr*)&clientaddr, &clientlen);
        inet_ntop(AF_INET,&(clientaddr.sin_addr),clientIP,10);
        //        exit(1);
        cout<<"Received "<<nbytes<<"bytes from "<<clientIP<<endl;
        if(nbytes < 0) continue;
        
        //Get DNS address by client IP
        inet_ntop(AF_INET,&(cdnserver.sin_addr),cdnIP,4);
        getIP(cdnIP,method,cdnIP);
        cdnserver.sin_addr.s_addr = inet_addr(cdnIP);
        
        DnsHeader tmpHeader = setHeader(0,1);
        memcpy(buf,&tmpHeader,sizeof(DnsHeader));
        char* name = buf + sizeof(DnsHeader);
        if(name[0] != 5 || name[1] != 'v' || name[2] != 'i')
        {
            cout<<"Iillegal Query!"<<endl;
            DnsHeader* header = (DnsHeader*) buf;
            header->rcode = (uint8_t)3;
            res = sendto(sockfd, buf, nbytes, 0, (sockaddr*)&clientaddr, clientlen);
        }
        else
        {
            uint8_t* rdata = (uint8_t *)buf + nbytes;
            //Set pointer to name
            /*----------NAME---------*/
            *rdata++ = 192;
            *rdata++ = sizeof(DnsHeader);
            /*----------ANSWER-------*/
            DNSAnswer ans = setAnswer();
            memcpy(rdata, &ans, sizeof(DNSAnswer));
            rdata += sizeof(DNSAnswer) - 2; //struct alignment issue
            /*----------RDATA--------*/
            *((uint32_t*)rdata) = cdnserver.sin_addr.s_addr;
            rdata += 4;
            res = sendto(sockfd, buf, (char*)rdata - buf, 0, (sockaddr*)&clientaddr, clientlen);
            cout<<"Reply sent "<<res<<" bytes succesfully"<<endl;
        }
        //Log saved
        fout<<(time(NULL) - start)<<" "<<clientIP<<" "<<name<<" "<<cdnIP<<endl;
    }
    return 0;
}

