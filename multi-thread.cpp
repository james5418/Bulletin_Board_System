#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <map>
#include <pthread.h>

using namespace std;

struct Message{
    string sender;
    string msg;
};

struct User{
    string name;
    string password;
    vector<Message> mBox;
};

vector<User> user_list;
vector<string> name_list;


void Send(string m, int connfd){
    char output[40960];
    strcpy(output, m.c_str());
    send(connfd, output, m.length(), 0);
}

void *socketThread(void *para){

    string access_name = "";

    int connfd = *(int*)para;

    string welcome = "********************************\n** Welcome to the BBS server. **\n********************************\n";
    Send(welcome, connfd);

        while(true){

            char buf[40960];
            Send("% ", connfd);
            memset(buf, 0, sizeof(buf));

            // receive date from client
            int byteReceived = recv(connfd, buf, 40960, 0);

            if(byteReceived == 0){
                cout << "Disconnected\n";
                break;
            }

            char input[40960];
            char output[40960];
            vector<string> line;
            
            strcpy(input, buf);

            char * token = strtok(input, " ");
            while( token != NULL ) {
                line.push_back(token);
                token = strtok(NULL, " ");
            }


            if(line.size() == 0){
                break;
            }
            else if(line.size() == 1  && line[0] == "exit\n"){
                if(access_name != ""){
                    string m = "Bye, ";
                    m += access_name;
                    m += ".\n";
                    Send(m, connfd);

                    for(int i=0;i<user_list.size();i++){
                        if(user_list[i].name == access_name){
                            access_name = "";
                        }
                    }
                }
                cout << "Client disconnected\n";
                break;
            }
            else if(line.size() < 3 && (line[0] == "register\n" || line[0] == "register")){
                Send("Usage: register <username> <password>\n", connfd);
            }
            else if(line.size() < 3 && (line[0] == "login\n" || line[0] == "login")){
                Send("Usage: login <username> <password>\n", connfd);
            }
            else if(line.size() < 3 && (line[0] == "send\n" || line[0] == "send")){
                Send("Usage: send <username> <message>\n", connfd);
            }
            else if(line.size() < 2 && (line[0] == "receive\n" || line[0] == "receive")){
                Send("Usage: receive <username>\n", connfd);
            }
            else if(line.size() == 3 && line[0] == "register"){
                if(count(name_list.begin(), name_list.end(), line[1])){
                    Send("Username is already used.\n", connfd);
                }
                else{
                    User new_person;
                    new_person.name = line[1];
                    new_person.password = line[2];
                    user_list.push_back(new_person);
                    name_list.push_back(line[1]);

                    Send("Register successfully.\n", connfd);
                }
                
            }
            else if(line.size() == 3 && line[0] == "login"){ 
                if(access_name != ""){
                    Send("Please logout first.\n", connfd);
                }
                else{
                    if(count(name_list.begin(), name_list.end(), line[1])){
                        for(int i=0;i<user_list.size();i++){
                            if(user_list[i].name == line[1]){
                                if(user_list[i].password == line[2]){ 
                                    string m = "Welcome, ";
                                    m += line[1];
                                    m += ".\n";
                                    Send(m, connfd);
                                    access_name = line[1];
                                }
                                else{
                                    Send("Login failed.\n", connfd);
                                }
                            }
                        }
                    }
                    else{
                        Send("Login failed.\n", connfd);
                    }
                }
            }
            else if(line.size() == 1 && line[0] == "whoami\n"){
                if(access_name == ""){
                    Send("Please login first.\n", connfd);
                }
                else{
                    string m = access_name;
                    m += "\n";
                    Send(m, connfd);
                }
            }
            else if(line.size() == 1 && line[0] == "logout\n"){

                for(int i=0;i<user_list.size();i++){
                    if(access_name == user_list[i].name){  
                        string m = "Bye, ";
                        m += user_list[i].name;
                        m += ".\n";
                        Send(m, connfd);
                        access_name = "";
                        break;
                    }
                    else if(access_name == ""){
                        Send("Please login first.\n", connfd);
                        break;
                    }
                }
            }
            else if(line.size() == 1 && line[0] == "list-user\n"){
                sort(name_list.begin(), name_list.end());

                for(int i=0;i<name_list.size();i++){
                    string m = name_list[i];
                    m += "\n";
                    Send(m, connfd);
                }
            }
            else if(line.size() >= 3 && line[0] == "send"){ 
                if(access_name == ""){
                    Send("Please login first.\n", connfd);
                }
                else{
                    if(count(name_list.begin(), name_list.end(), line[1])){ 
                        Message tmp;
                        tmp.sender = access_name;
                        for(int c=2;c<line.size();c++){
                            tmp.msg += line[c];
                            if(c != line.size()-1){
                                tmp.msg += " ";
                            }  
                        }
                        for(int i=0;i<user_list.size();i++){
                            if(user_list[i].name == line[1]){
                                user_list[i].mBox.push_back(tmp);
                            }
                        }
                    }
                    else{
                        Send("User not existed.\n", connfd);
                    }
                }
            }
            else if(line.size() == 1 && line[0] == "list-msg\n"){
                if(access_name == ""){
                    Send("Please login first.\n", connfd);
                }
                else{
                    for(int i=0;i<user_list.size();i++){
                        if(user_list[i].name == access_name){
                            if(user_list[i].mBox.size()==0){
                                Send("Your message box is empty.\n", connfd);
                            }
                            else{
                                map<string, int> message_count;
                                for(int j=0;j<user_list[i].mBox.size();j++){
                                    auto iter = message_count.find(user_list[i].mBox[j].sender);
                                    if(iter != message_count.end()){
                                        message_count[user_list[i].mBox[j].sender]++;
                                    }  
                                    else{
                                        message_count[user_list[i].mBox[j].sender] = 1;
                                    }
                                }
                                for (const auto& s : message_count){
                                    string m;
                                    m += to_string(s.second);
                                    m += " message from ";
                                    m += s.first;
                                    m += ".\n";
                                    Send(m, connfd);
                                }
                            }
                        }
                    }
                }
            }
            else if(line.size() == 2 && line[0] == "receive"){

                line[1].erase(remove(line[1].begin(), line[1].end(), '\n'), line[1].end());

                if(access_name == ""){
                    Send("Please login first.\n", connfd);
                }
                else{
                    if(count(name_list.begin(), name_list.end(), line[1])){
                        for(int i=0;i<user_list.size();i++){
                            if(user_list[i].name == access_name){
                                for(int j=0;j<user_list[i].mBox.size();j++){
                                    if(user_list[i].mBox[j].sender == line[1]){
                                        string m = user_list[i].mBox[j].msg; 
                                        m.erase (m.begin());
                                        m.erase (m.end()-2);
                                        Send(m, connfd);

                                        user_list[i].mBox.erase(user_list[i].mBox.begin()+j);
                                        break;
                                    }  
                                }
                            }
                        }
                    }
                    else{
                        Send("User not existed.\n", connfd);
                    }        
                }
            }
            else{               
                Send("Error!!!\n", connfd);
            }
        }

    close(connfd);
    pthread_exit(NULL);
}


int main(int argc, char *argv[]){

    int listenfd, connfd, sock, on=1;
    int portNum = 1234;
    struct sockaddr_in server_addr, client_addr;
    socklen_t clientSize = sizeof(client_addr);
	
    if(argc>1){
        portNum = stoi(argv[1]);
    }

    pthread_t tid[60], recv_tid;
    int x = 0;


    while(true){

        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        sock = setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
        bzero(&server_addr, sizeof(server_addr));

        if(listenfd < 0){
            cout << "Error establishing connection\n";
            exit(1);
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        server_addr.sin_port = htons(portNum);    


        if(bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
            cout << "Error binding socket\n";
            exit(1);
        }

        listen(listenfd,10);
            
        connfd = accept(listenfd, (struct sockaddr*)&client_addr, &clientSize);

        if(connfd < 0){
            cout << "Error on accepting\n";
            exit(1);
        }
        else{
            cout << "Client connected\n";
        }

        close(listenfd);

        if( pthread_create(&tid[x++], NULL, socketThread, &connfd) != 0 ){
            cout << "Failed to create thread\n";
        }

    }   
    
	return 0;
}