#include <iostream>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //tcp
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(5000); 
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    cout<<"vn(me) connected...."<<"\n";

    string msg="A";
    send(sockfd,msg.c_str(), msg.size(),0);
    cout<<"msg sent : "<< msg <<"\n";

    char buf[1024];
    int n=recv(sockfd, buf, sizeof(buf)-1, 0);
    if(n>0){
        buf[n]='\0';
        cout<<"received msg : "<< buf <<"\n";
    }

    close(sockfd);
}

