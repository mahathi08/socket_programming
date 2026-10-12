#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //tcp
    struct sockaddr_in my_addr;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(12345); 
    my_addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
    listen(sockfd, 3);
    cout<<"on(me) listening...."<<"\n";
    struct sockaddr_in got_addr;
    socklen_t l=sizeof(got_addr);
    int got=accept(sockfd, (struct sockaddr *)&got_addr,&l);
    cout<<"got(vn) connected...."<<"\n";
    char buf[1024];
    int n=recv(got, buf, sizeof(buf)-1, 0);
    if(n>0)buf[n]='\0';
    cout<<"received msg : "<< buf <<"\n";
    string msg="link_state info";
    send(got,msg.c_str(), msg.size(),0);
    cout<<"msg sent : "<< msg <<"\n";
    close(sockfd);
    close(got);
}

