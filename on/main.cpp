#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <sys/select.h>
#include <sys/stat.h>
#include <signal.h>
#include <cerrno>
#include <thread>

using namespace std;


bool sen(int fd,const void *data,size_t len){
    const char *p = static_cast<const char *>(data);
    while (len > 0) {
        ssize_t n = send(fd, p, len, 0);
        if(n<=0) return false;
        p+=n;
        len-=n;
    }
    return true;
}

bool receive(int fd,void *data,size_t len){
    char *p = static_cast<char *>(data);
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if(n<=0) return false;
        p+=n;
        len-=n;
    }
    return true;
}

void add_u16(vector<uint8_t>& b, uint16_t x) {
    x = htons(x);
    const uint8_t *p = reinterpret_cast<const uint8_t*>(&x);
    b.insert(b.end(),p,p+2);
}

void add_u32(vector<uint8_t>& b, uint32_t x) {
    x = htonl(x);
    const uint8_t *p = reinterpret_cast<const uint8_t*>(&x);
    b.insert(b.end(),p,p+4);
}

bool send_frame(int fd, char type, const vector<uint8_t>& bod) {
    uint16_t l = htons(static_cast<uint16_t>(bod.size()));
    char h[3] = {type, 0, 0};
    memcpy(h+1, &l, 2);
    return sen(fd, h, 3) && sen(fd, bod.data(), bod.size());
}


bool read_graph(const string& f,vector<vector<int>>& graph){
    ifstream file(f);
    if(!file) return false;
    graph.clear();
    string line;
    while(getline(file,line)){
        if(line.empty() || line.find_first_not_of(" \t\r") == string::npos) continue;
        size_t sta = line.find_first_not_of(" \t\r");
        if (line[sta] == '#') continue;
        stringstream ss(line);
        vector<int> row;
        int cost;
        while (ss >> cost) row.push_back(cost);
        if (!ss.eof()) return false;
        if (!row.empty()) graph.push_back(row);
    }
    int n = static_cast<int>(graph.size())+1;
    for (int i=0; i<n-1; i++) {
        if (graph[i].size() != static_cast<size_t>(n-i-1)) return false;
        for (int cost : graph[i]) {
            if (cost!=-1 && cost<=0) return false;
        }
    }
    return true;
}


bool send_ls(int i,int n,const vector<vector<int>>& graph,const vector<string>& vn_ip,const vector<int>& vn_port,const vector<int>& vn_sockets){
    vector<uint8_t> bod;
    bod.push_back(0);
    uint8_t k = 0;
    for(int j=0;j<n;j++){
        int c;
        if(i==j) c=0;
        else if(i<j) c=graph[i][j-i-1];
        else c=graph[j][i-j-1];
        if(i!=j && c==-1) continue;
        bod.push_back(static_cast<uint8_t>('A'+j));

        in_addr addr{};
        inet_pton(AF_INET, vn_ip[j].c_str(), &addr);
        const uint8_t *by_ip= reinterpret_cast<const uint8_t *>(&addr.s_addr);
        bod.insert(bod.end(),by_ip,by_ip+4);
        add_u16(bod, static_cast<uint16_t>(vn_port[j]));
        add_u32(bod, static_cast<uint32_t>(c));
        k++;
    }
    bod[0]=k;
    return send_frame(vn_sockets[i],'L',bod);
}

int main(int argc,char *argv[]){
    if(argc!=2){
        cout<<"use \"./main filename\"\n";
        return 1;
    }
    vector<vector<int>> graph;
    if (!read_graph(argv[1], graph)) {
        cout<<"Invalid config file\n";
        return 1;
    }
    int n = static_cast<int>(graph.size()) + 1;
    if (n>26) {
        cout<<"max no of nodes is 26\n";
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0); //tcp
    if(sockfd < 0){
        perror("socket error"); 
        return 1;
    }
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt error");
        close(sockfd);
        return 1;
    }

    struct sockaddr_in my_addr{};
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(5000); 
    my_addr.sin_addr.s_addr = inet_addr("172.23.32.143");
    
    if(bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0){
        perror("bind error");
        close(sockfd);
        return 1;
    }
    if(listen(sockfd, SOMAXCONN) < 0){
        perror("listen error");
        close(sockfd);
        return 1;
    }
    cout<<"on(me) listening...."<<"\n";

    vector<string> vn_ip(n);
    vector<int> vn_port(n);
    vector<int> vn_sockets(n,-1);
    for(int i=0; i<n; i++) {
        struct sockaddr_in got_addr{};
        socklen_t l=sizeof(got_addr);
        int got=accept(sockfd, (struct sockaddr *)&got_addr,&l);
        if(got < 0){
            perror("accept error");
            close(sockfd);
            return 1;
        }
        // cout<<"vn "<<char('A'+i)<<" connected....\n";

        uint8_t h[3];
        if(!receive(got,h,3) ||  h[0]!='C'){
            cout<<"invalid connect...";
            close(got);
            return 1;
        }
        uint16_t body_len;
        memcpy(&body_len, h+1, 2);
        body_len = ntohs(body_len);
        if(body_len!=6){
            cout<<"invalid connect...";
            close(got);
            return 1;
        }
        uint8_t bod[6];
        if(!receive(got,bod,6)){
            cout<<"invalid connect body...";
            close(got);
            return 1;
        }
        in_addr ip_addr{};
        memcpy(&ip_addr.s_addr,bod,4);

        uint16_t port_net;
        memcpy(&port_net,bod+4,2);
        int port = ntohs(port_net);

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, ip, sizeof(ip));

        vn_ip[i] = ip;
        vn_port[i] = port;
        cout<<"vn "<<char('A'+i)<<": ip=" << vn_ip[i]<< ", udp port="<<vn_port[i]<<"\n";

        vn_sockets[i] = got;
    }
    for(int i=0; i<n; i++){
        if(!send_ls(i,n,graph,vn_ip,vn_port,vn_sockets)){
            perror("link_state send error");
        }
        else cout<<"sent link_state to vn("<<char('A'+i)<<")"<<"\n";
    }
    cout<<"initial link_state messages sent.\n";
    vector<int> extra_sockets;
    int next_extra_id = n;
    struct stat st{};
    if(stat(argv[1], &st)!=0){
        perror("stat");
        close(sockfd);
        return 1;
    }
    auto last_mod_sec = st.st_mtim.tv_sec;
    auto last_mod_nsec = st.st_mtim.tv_nsec;
    while(true){
        this_thread::sleep_for(chrono::seconds(1));
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        int ready = select(sockfd+1, &readfds, nullptr, nullptr, &tv);
        if (ready > 0 && FD_ISSET(sockfd, &readfds)) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int got = accept(sockfd,reinterpret_cast<sockaddr*>(&client_addr),&client_len);
            if (got >= 0) {
                uint8_t h[3];
                uint8_t bod[6];
                bool valid = receive(got, h, 3) && h[0] == 'C';

                uint16_t body_len = 0;
                if (valid) {
                    memcpy(&body_len, h + 1, 2);
                    body_len = ntohs(body_len);
                    valid = (body_len == 6);
                }
                if (valid) valid = receive(got, bod, 6);
                if (!valid) {
                    cerr << "Invalid CONNECT from extra VN\n";
                    close(got);
                } else if (next_extra_id >= 26) {
                    cerr << "Cannot assign an ID beyond Z\n";
                    close(got);
                } else {
                    vector<uint8_t> body;
                    body.push_back(1);
                    body.push_back(static_cast<uint8_t>('A' + next_extra_id));

                    body.insert(body.end(), bod, bod + 4); 
                    body.push_back(bod[4]);               
                    body.push_back(bod[5]);
                    add_u32(body, 0);
                    if (send_frame(got, 'L', body)) {
                        cout << "Extra isolated vn "<< char('A'+next_extra_id)<< " connected\n";
                        extra_sockets.push_back(got);
                        next_extra_id++;
                    } else {
                        cerr << "Failed to send link-state to extra VN\n";
                        close(got);
                    }
                }
            }
            else perror("accept extra VN");
        }
        if (stat(argv[1], &st) != 0) continue;
        if (st.st_mtim.tv_sec != last_mod_sec || st.st_mtim.tv_nsec != last_mod_nsec){
            vector<vector<int>> new_graph;
            if(!read_graph(argv[1], new_graph) || static_cast<int>(new_graph.size())+1!=n){
                cout<<"invalid\n";
                last_mod_sec = st.st_mtim.tv_sec;
                last_mod_nsec = st.st_mtim.tv_nsec;
                continue;
            }
            graph = new_graph;
            last_mod_sec = st.st_mtim.tv_sec;
            last_mod_nsec = st.st_mtim.tv_nsec;
            cout<<"sending upadated link_state"<<"\n";
            for(int i=0; i<n; i++){
                if(vn_sockets[i] < 0) continue;
                if(!send_ls(i,n,graph,vn_ip,vn_port,vn_sockets)){
                    perror("link_state send error");
                }
            }
        }
    }
    close(sockfd);
}

