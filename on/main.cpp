#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <string>

using namespace std;

int main(int argc, char *argv[]){
    if(argc!=2){
        cout<<"use ./on file\n";
        return 1;
    }
    ifstream file(argv[1]);
    if(!file){
        cout<<"can't open file\n";
        return 1;
    }

    vector<vector<int>> graph;
    string line;
    while(getline(file,line)){
        if(line.empty() || line.find_first_not_of(" \t\r") == string::npos) continue;
        size_t sta = line.find_first_not_of(" \t\r");
        if (line[sta] == '#') continue;
        stringstream ss(line);
        vector<int> row;
        int cost;
        while (ss >> cost) row.push_back(cost);
        if (!row.empty()) graph.push_back(row);
    }
    file.close();
    int n=graph.size()+1;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //tcp
    if(sockfd < 0){
        perror("socket error"); 
        return 1;
    }
    // int opt = 1;
    // if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    //     perror("setsockopt error");
    //     close(sockfd);
    //     return 1;
    // }

    struct sockaddr_in my_addr{};
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(5000); 
    my_addr.sin_addr.s_addr = inet_addr("172.23.32.143");
    
    if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0){
        perror("bind error");
        close(sockfd);
        return 1;
    }
    if(listen(sockfd, 1) < 0){
        perror("listen error");
        close(sockfd);
        return 1;
    }
    cout<<"on(me) listening...."<<"\n";

    vector<string> vn_ip(n);
    vector<int> vn_port(n);
    vector<int> vn_sockets(n);
    for(int i=0; i<n; i++) {
        struct sockaddr_in got_addr{};
        socklen_t l=sizeof(got_addr);
        int got=accept(sockfd, (struct sockaddr *)&got_addr,&l);
        if(got < 0){
            perror("accept error");
            close(sockfd);
            return 1;
        }
        cout<<"vn "<<char('A'+i)<<" connected....\n";

        char buf[1024];
        int m=recv(got, buf, sizeof(buf)-1, 0);
        if(m <= 0){
            cout<<"failed to receive vn info...";
            close(got);
            close(sockfd);
            return 1;
        }
        buf[m]='\0';
        string ip;
        int port;
        stringstream ss(buf);
        if (!(ss >> ip >> port)){
            cout<<"invalid vn info...\n";
            close(got);
            close(sockfd);
            return 1;
        }

        vn_ip[i] = ip;
        vn_port[i] = port;
        cout<<"vn "<<char('A'+i)<<": ip=" << vn_ip[i]<< ", udp port="<<vn_port[i]<<"\n";

        vn_sockets[i] = got;
    }
    for(int i=0; i<n; i++){
        string msg="link_state info";
        send(vn_sockets[i],msg.c_str(), msg.size(),0);
        cout<<"link_state sent to vn : "<< char('A'+i) <<"\n";
        close(vn_sockets[i]);
    }
    close(sockfd);
}

