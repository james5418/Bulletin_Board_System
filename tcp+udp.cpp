#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <limits.h>
#include <sys/errno.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <utility>
#include <sstream>

#define INFTIM -1

using namespace std;

struct a {
    unsigned char flag;
    unsigned char version;
    unsigned char payload[0];
} __attribute__((packed));

struct b {
    unsigned short len;
    unsigned char data[0];
} __attribute__((packed));

struct User{
    string name;
    string password;
	bool L;
    int tcp_fd;
    int clientNum;
    int udp_port;
    int version;
};

vector<User> user_list;
vector<string> name_list;
string client_login_name[100]; 
vector<string> black_list;
string chat_history;

int i, maxi, listenfd, connfd, sockfd, udpfd, sock, on=1;
int nready;
int portNum = 1234;
ssize_t n;
char buf[40960];
struct pollfd client[100];
struct sockaddr_in cliaddr, servaddr, servaddr_udp, cliaddr_udp;
socklen_t clilen = sizeof(cliaddr);

vector<string> filtering_list = {"how", "you", "or", "pek0", "tea", "ha", "kon", "pain", "Starburst Stream"};
vector<string> filtering_replace = {"***", "***", "**", "****", "***", "**", "***", "****", "****************"};

string content_filtering(string input){
    string msg = input;
    for(size_t i=0;i<filtering_list.size();i++){
        while(msg.find(filtering_list[i]) != std::string::npos){
            msg=msg.replace(msg.find(filtering_list[i]),filtering_list[i].size(),filtering_replace[i]);
        }
    }
    return msg;
}

const string base64_encoding_table ="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

string base64_encode(string input){
    string output;
    int idx1=0, idx2=-6;
    
    for(size_t i=0;i<input.size();i++){
        idx1 = (idx1 << 8) + (unsigned char)input[i];
        idx2 += 8;
        while(idx2 >= 0){
            output += base64_encoding_table[(idx1>>idx2) & 0x3F];
            idx2 -= 6;
        }
    }
    if(idx2 > -6){
        output += base64_encoding_table[((idx1<<8) >> (idx2+8)) & 0x3F];
    }
    while(output.size()%4){
        output += "=";
    }
    return output;
}

string base64_decode(string input){
    string output;
    int idx1=0, idx2=-8;
    for (size_t i=0;i<input.size();i++) {
        if (base64_encoding_table.find(input[i]) != std::string::npos ){
            idx1 = (idx1 << 6) + base64_encoding_table.find(input[i]);
            idx2 += 6;
            if (idx2 >= 0) {
                output += char((idx1>>idx2) & 0xFF);
                idx2 -= 8;
            }
        }
    }
    return output;
}

void Send(string m, int fd){
	send(fd, m.c_str(), m.length(), 0);
}

bool isNumber(const string& s){
    for (char const &ch : s) {
        if (std::isdigit(ch) == 0) 
            return false;
    }
    return true;
}

bool user_is_login(string name){
	bool flag = false;
	for(size_t i=0;i<user_list.size();i++){
		if(user_list[i].name == name){
			flag = user_list[i].L;
		}
	}
	return flag;
}

void exe(char buf[], int fd, int clientNum){
	char input[40960];
	vector<string> line;
	
	strcpy(input, buf);
	char * token = strtok(input, " ");
	while( token != NULL ) {
		line.push_back(token);
		token = strtok(NULL, " ");
	}

	if(line[line.size()-1] == "\n"){
        line[line.size()-2] += "\n";
		line.pop_back();
    }


	if(line.size() == 0){
		return;
	}
	else if(line.size() != 3 && (line[0] == "register\n" || line[0] == "register")){
		Send("Usage: register <username> <password>\n", fd);
	}
	else if(line.size() != 3 && (line[0] == "login\n" || line[0] == "login")){
		Send("Usage: login <username> <password>\n", fd);
	}
    else if(line.size() != 1 && (line[0] == "logout\n" || line[0] == "logout")){
		Send("Usage: logout\n", fd);
	}
    else if(line.size() != 1 && (line[0] == "exit\n" || line[0] == "exit")){
		Send("Usage: exit\n", fd);
	}
    else if(line.size() != 3 && (line[0] == "enter-chat-room\n" || line[0] == "enter-chat-room")){
		Send("Usage: enter-chat-room <port> <version>\n", fd);
	}
    else if(line.size() == 1  && line[0] == "exit\n"){
		if(client_login_name[clientNum] != ""){
			string m = "Bye, ";
			m += client_login_name[clientNum];
			m += ".\n";
			Send(m, fd);

			for(size_t i=0;i<user_list.size();i++){
				if(user_list[i].name == client_login_name[clientNum]){
					user_list[i].L = false;
                    user_list[i].udp_port = 0;
				}
			}
			client_login_name[clientNum] = "";
		}
		cout << "Client disconnected\n";

		close(fd);
		client[clientNum].fd = -1;
		return;
	}
	else if(line.size() == 3 && line[0] == "register"){
		if(count(name_list.begin(), name_list.end(), line[1])){
			Send("Username is already used.\n", fd);
		}
		else{
			User new_person;
			new_person.name = line[1];
			new_person.password = line[2];
			new_person.L = false;
            new_person.udp_port = 0;
			user_list.push_back(new_person);
			name_list.push_back(line[1]);

			Send("Register successfully.\n", fd);
		}
	}
	else if(line.size() == 3 && line[0] == "login"){
		if(client_login_name[clientNum] != "" || user_is_login(line[1])){
			Send("Please logout first.\n", fd);
		}
		else{
			if(count(name_list.begin(), name_list.end(), line[1])){
				for(size_t i=0;i<user_list.size();i++){
					if(user_list[i].name == line[1]){
                        if(count(black_list.begin(), black_list.end(), line[1]) >= 3){
                            string m = "We don't welcome ";
                            m += line[1];
                            m += "!\n";
                            Send(m, fd);
                        }
                        else{
                            if(user_list[i].password == line[2]){
                                string m = "Welcome, ";
                                m += line[1];
                                m += ".\n";
                                Send(m, fd);

                                client_login_name[clientNum] = line[1];
                                user_list[i].L = true;
                            }
                            else{
                                Send("Login failed.\n", fd);
                            }
                        }
					}
				}
			}
			else{
				Send("Login failed.\n", fd);
			}
		}
	}
	else if(line.size() == 1 && line[0] == "logout\n"){
		for(size_t i=0;i<user_list.size();i++){
			if(client_login_name[clientNum] == user_list[i].name){  
				string m = "Bye, ";
				m += user_list[i].name;
				m += ".\n";
				Send(m, fd);

				client_login_name[clientNum] = "";
				user_list[i].L = false;
                user_list[i].udp_port = 0;
				break;
			}
			else if(client_login_name[clientNum] == ""){
				Send("Please login first.\n", fd);
				break;
			}
		}
	}
    else if(line.size() == 3 && line[0] == "enter-chat-room"){
        line[2].erase(remove(line[2].begin(), line[2].end(), '\n'), line[2].end());

        if(isNumber(line[1]) && stoi(line[1])>=1 && stoi(line[1])<=65535){
            if(isNumber(line[2]) && (line[2]=="1" || line[2]=="2")){
                if(client_login_name[clientNum] == ""){
                    Send("Please login first.\n", fd);
                }
                else{
                    string m = "Welcome to public chat room.\n";
                    m += "Port:" + line[1] + "\n";
                    m += "Version:" + line[2] + "\n";
                    m += chat_history;
                    Send(m, fd);
                    
                    for(size_t k=0;k<user_list.size();k++){
                        if(user_list[k].name == client_login_name[clientNum]){
                            user_list[k].clientNum = clientNum;
                            user_list[k].tcp_fd  = fd;
                            user_list[k].udp_port = stoi(line[1]);
                            user_list[k].version = stoi(line[2]);
                        }
                    }
                }
            }
            else{
                string m = "Version ";
                m += line[2];
                m += " is not supported.\n";
                Send(m, fd);
            }
        }
        else{
            string m = "Port ";
            m += line[1];
            m += " is not valid.\n";
            Send(m, fd);
        }
    }
}

int main(int argc, char *argv[]){
	
	if(argc>1){
        portNum = stoi(argv[1]);
    }

	for(int k=0;k<100;k++){
		client_login_name[k] = "";
	}

    // UDP
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    sock = setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr_udp, sizeof(servaddr_udp));
    servaddr_udp.sin_family = AF_INET;
	servaddr_udp.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr_udp.sin_port = htons(portNum);
    bind(udpfd, (struct sockaddr*)&servaddr_udp, sizeof(servaddr_udp));

    // TCP
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	sock = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	bzero(&servaddr, sizeof(servaddr));

	if(listenfd < 0){
		cout << "Error establishing connection\n";
		exit(1);
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(portNum);

	if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		cout << "Error binding socket\n";
		exit(1);
	}

    listen(listenfd,100);

	client[0].fd = listenfd;
	client[0].events = POLLRDNORM;

    client[1].fd = udpfd;
	client[1].events = POLLRDNORM;

	for(i=2;i<100;i++){
		client[i].fd = -1;
	}
	maxi = 0;
    
	
	while(true){
        
		nready = poll(client, maxi+2, INFTIM);	

		if(client[0].revents & POLLRDNORM){

			connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
			if(connfd < 0){
            	cout << "Error on accepting\n";
				exit(1);
			}
			else{
				cout << "Client connected\n";
			}
			
			string welcome = "********************************\n** Welcome to the BBS server. **\n********************************\n";
			Send(welcome, connfd);
			Send("% ", connfd);
			memset(buf, 0, sizeof(buf));

			for(i=2;i<100;i++){
				if (client[i].fd < 0) {
					client[i].fd = connfd;
					break;
				}
			}
			client[i].events = POLLRDNORM;

			if(i > maxi){
				maxi = i;
			}
			if(--nready <= 0){
				continue;
			}
		}

        if(client[1].revents & (POLLRDNORM | POLLERR)){
            char c[40960];
            string name, msg;
            socklen_t len = sizeof(cliaddr_udp);

            int x = recvfrom(udpfd, c, 40960, 0, (struct sockaddr*)&cliaddr_udp, &len);
            c[x] = '\0';

            struct a *pa = (struct a*) c;

            if(pa->flag != 0x01){
                continue;
            }

            if(pa->version == 0x01){
                struct b *pb1 = (struct b*) (c + sizeof(struct a));
                struct b *pb2 = (struct b*) (c + sizeof(struct a) + sizeof(struct b) + pb1->len/256);

                stringstream ss_name, ss_msg;
                ss_name << pb1->data;
                name = ss_name.str();
                ss_msg << pb2->data;
                msg = ss_msg.str();
            }
            else if(pa->version == 0x02){
                string b64 = &c[2];
                char name_pkg[40960], msg_pkg[40960];

                sscanf(b64.c_str(), "%s\n%s\n", name_pkg, msg_pkg);
                name = name_pkg;
                msg = msg_pkg;
                name = base64_decode(name);
                msg = base64_decode(msg);
            }
            else{
                continue;
            }
            
            if(msg != content_filtering(msg)){
                black_list.push_back(name);
                msg = content_filtering(msg);
            }

            for(size_t k=0;k<user_list.size();k++){
                if(user_list[k].name == name && user_list[k].L==true){
                    int tcp_fd, client_Num;
                    tcp_fd = user_list[k].tcp_fd;
                    client_Num = user_list[k].clientNum;

                    if(count(black_list.begin(), black_list.end(), name) < 3){
                        chat_history += name + ":" + msg + "\n";
                    }
                    else if(count(black_list.begin(), black_list.end(), name) == 3){
                        string m = "Bye, ";
                        m += name;
                        m += ".\n";
                        Send(m, tcp_fd);
                        Send("% ", tcp_fd);

                        chat_history += name + ":" + msg + "\n";
                        
                        client_login_name[client_Num] = ""; 
                        user_list[k].L = false;
                        user_list[k].udp_port = 0;
                    }
                }
            }

            size_t v1_pkg_len, v2_pkg_len;
            
            // encode v1
            char packet_v1[40960];
            uint16_t name_len = (uint16_t)(strlen(name.c_str()));
            uint16_t msg_len = (uint16_t)(strlen(msg.c_str()));
            struct a *pa_v1 = (struct a*) packet_v1;
            struct b *pb1_v1 = (struct b*) (packet_v1 + sizeof(struct a));
            struct b *pb2_v1 = (struct b*) (packet_v1 + sizeof(struct a) + sizeof(struct b) + name_len);
            pa_v1->flag = 0x01;
            pa_v1->version = 0x01;
            pb1_v1->len = htons(name_len);
            memcpy(pb1_v1->data, name.c_str(), name_len);
            pb2_v1->len = htons(msg_len);
            memcpy(pb2_v1->data, msg.c_str(), msg_len);
            sprintf(packet_v1, "%s%s%s", pa_v1, pb1_v1, pb2_v1);
            v1_pkg_len = 6 + name.size() + msg.size();
            
            // encode v2
            char packet_v2[40960];
            name = base64_encode(name);
            msg = base64_encode(msg);
            sprintf(packet_v2, "\x01\x02%s\n%s\n", name.c_str(), msg.c_str());
            v2_pkg_len = 4 + name.size() + msg.size();
            

            for(size_t n=0;n<user_list.size();n++){
                if(user_list[n].udp_port != 0){
                    cliaddr_udp.sin_port = htons(user_list[n].udp_port);
                    if(user_list[n].version == 1){
                        sendto(udpfd, packet_v1, v1_pkg_len, 0, (struct sockaddr*)&cliaddr_udp, len);
                    }
                    else if(user_list[n].version == 2){
                        sendto(udpfd, packet_v2, v2_pkg_len, 0, (struct sockaddr*)&cliaddr_udp, len);
                    }
                }
            }

            memset(c, 0, sizeof(c));

            if(--nready <= 0){
				continue;
			}

        }
	
		for(i=2;i<=maxi;i++){
			if((sockfd = client[i].fd) < 0){
				continue;
			}
					
			if(client[i].revents & (POLLRDNORM | POLLERR)){
                
                n = recv(sockfd, buf, 40960, 0);
				if(n < 0){
					if (errno == ECONNRESET) {
						close(sockfd);
						client[i].fd = -1;
					} 
				}
				else if(n == 0){
					close(sockfd);
					client[i].fd = -1;
				}
				else{
					exe(buf, sockfd, i);
					Send("% ", sockfd);
					memset(buf, 0, sizeof(buf));
				}
					
				if(--nready <= 0){
					break;
				}
			}
            
		}
	}

    close(udpfd);
	return 0;
}
