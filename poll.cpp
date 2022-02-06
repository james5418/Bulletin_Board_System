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
#include <ctime>

#define INFTIM -1

using namespace std;

struct User{
    string name;
    string password;
	bool L;
};

struct Board{
	string name;
	string moderator;
};

struct Post{
	string sn;
	string title;
	string author;
	string date;
	string content;
	string board;
	vector< pair<string,string> > comment;
};

vector<User> user_list;
vector<string> name_list;
vector<Board> board_list;
vector<string> board_name_list;
vector<Post> post_list;
vector<string> post_num_list;
string client_login_name[100]; 
int postSN = 1;

int i, maxi, listenfd, connfd, sockfd, sock, on=1;
int nready;
int portNum = 1234;
ssize_t n;
char buf[40960];
struct pollfd client[100];
struct sockaddr_in cliaddr, servaddr;
socklen_t clilen = sizeof(cliaddr);

void Send(string m, int fd){
	send(fd, m.c_str(), m.length(), 0);
}

string Date(){
    time_t now = time(0);
    char* dt = ctime(&now);
    vector<string> today;

    char * token = strtok(dt, " ");
	while( token != NULL ) {
		today.push_back(token);
		token = strtok(NULL, " ");
	}

    string dd = "JanFebMarAprMayJunJulAugSepOctNovDec";
    size_t found = dd.find(today[1]);

    string day;
    day += to_string((found+2)/3+1);
    day += "/";
    day += today[2];

    return day;
}

bool isNumber(const string& s){
    for (char const &ch : s) {
        if (std::isdigit(ch) == 0) 
            return false;
    }
    return true;
}

bool no_title_or_content(vector<string> line){
	for(int i=0;i<line.size()-1;i++){
		if(line[i]=="--title" && line[i+1]=="--content"){
			return true;
		}
		else if(line[i]=="--content" && line[i+1]=="--title"){
			return true;
		}
	}
	return false;
}

bool create_post_fail(vector<string> line){
	if(line.size() < 6 && (line[0] == "create-post\n" || line[0] == "create-post")){
		return true;
	}
	else if((line[0] == "create-post\n" || line[0] == "create-post") && (count(line.begin(), line.end(), "--title")==0 || count(line.begin(), line.end(), "--content")==0)){
		return true;
	}
	else if((line[0] == "create-post\n" || line[0] == "create-post") && (line[1]=="--title" || line[1]=="--content")){
		return true;
	}
	else if((line[0] == "create-post\n" || line[0] == "create-post") && no_title_or_content(line)){
		return true;
	}
	else if((line[0] == "create-post\n" || line[0] == "create-post") && (line[line.size()-1]=="--content" || line[line.size()-1]=="--title")){
		return true;
	}
	else{
		return false;
	}
}

bool update_post_fail(vector<string> line){
	if(line.size() < 4 && (line[0] == "update-post\n" || line[0] == "update-post")){
		return true;
	}
	else if((line[0] == "update-post\n" || line[0] == "update-post") && line[2]!="--title" && line[2]!="--content"){
		return true;
	}
	else{
		return false;
	}
}

bool comment_fail(vector<string> line){
	if(line.size() < 3 && (line[0] == "comment\n" || line[0] == "comment")){
		return true;
	}
	else if((line[0] == "comment\n" || line[0] == "comment") && !isNumber(line[1])){
		return true;
	}
	else{
		return false;
	}
}

bool user_is_login(string name){
	bool flag = false;
	for(int i=0;i<user_list.size();i++){
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

	// 處理字尾空格
	if(line[line.size()-1] == "\n"){
        line[line.size()-2] += "\n";
		line.pop_back();
    }

	if(line.size() == 0){
		return;
	}
	else if(line.size() == 1  && line[0] == "exit\n"){
		if(client_login_name[clientNum] != ""){
			string m = "Bye, ";
			m += client_login_name[clientNum];
			m += ".\n";
			Send(m, fd);

			for(int i=0;i<user_list.size();i++){
				if(user_list[i].name == client_login_name[clientNum]){
					user_list[i].L = false;
				}
			}
			client_login_name[clientNum] = "";
		}
		cout << "Client disconnected\n";

		close(fd);
		client[clientNum].fd = -1;
		return;
	}
	else if(line.size() < 3 && (line[0] == "register\n" || line[0] == "register")){
		Send("Usage: register <username> <password>\n", fd);
	}
	else if(line.size() < 3 && (line[0] == "login\n" || line[0] == "login")){
		Send("Usage: login <username> <password>\n", fd);
	}
	else if(line.size() < 2 && (line[0] == "create-board\n" || line[0] == "create-board")){
		Send("Usage: create-board <name>\n", fd);
	}
	else if(create_post_fail(line)){
		Send("Usage: create-post <board-name> --title <title> --content <content>\n", fd);
	}
	else if(line.size() < 2 && (line[0] == "list-post\n" || line[0] == "list-post")){
		Send("Usage: list-post <board-name>\n", fd);
	}
	else if(line.size() < 2 && (line[0] == "read\n" || line[0] == "read")){
		Send("Usage: read <post-S/N>\n", fd);
	}
	else if(line.size() < 2 && (line[0] == "delete-post\n" || line[0] == "delete-post")){
		Send("Usage: delete-post <post-S/N>\n", fd);
	}
	else if(update_post_fail(line)){
		Send("Usage: update-post <post-S/N> --title/content <new>\n", fd);
	}
	else if(comment_fail(line)){
		Send("Usage: comment <post-S/N> <comment>\n", fd);
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
				for(int i=0;i<user_list.size();i++){
					if(user_list[i].name == line[1]){
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
			else{
				Send("Login failed.\n", fd);
			}
		}
	}
	else if(line.size() == 1 && line[0] == "logout\n"){
		for(int i=0;i<user_list.size();i++){
			if(client_login_name[clientNum] == user_list[i].name){  
				string m = "Bye, ";
				m += user_list[i].name;
				m += ".\n";
				Send(m, fd);

				client_login_name[clientNum] = "";
				user_list[i].L = false;
				break;
			}
			else if(client_login_name[clientNum] == ""){
				Send("Please login first.\n", fd);
				break;
			}
		}
	}
	else if(line.size() == 2 && line[0] == "create-board"){
		line[1].erase(remove(line[1].begin(), line[1].end(), '\n'), line[1].end());

		if(client_login_name[clientNum] == ""){
			Send("Please login first.\n", fd);
		}
		else{
			if(count(board_name_list.begin(), board_name_list.end(), line[1])){
				Send("Board already exists.\n", fd);
			}
			else{
				Board new_board;
				new_board.name = line[1];
				new_board.moderator = client_login_name[clientNum];
				board_list.push_back(new_board);
				board_name_list.push_back(line[1]);

				Send("Create board successfully.\n", fd);
			}
		}
	}
	else if(line.size() == 1 && line[0] == "list-board\n"){
		string m = "Index Name Moderator\n";
		for(int i=0;i<board_list.size();i++){
			m += to_string(i+1);
			m += " ";
			m += board_list[i].name;
			m += " ";
			m += board_list[i].moderator;
			m += "\n";
		}
		Send(m, fd);
	}
	else if(line.size() >= 6 && line[0] == "create-post"){
		if(client_login_name[clientNum] == ""){
			Send("Please login first.\n", fd);
		}
		else{
			if(count(board_name_list.begin(), board_name_list.end(), line[1])==0){
				Send("Board does not exist.\n", fd);
			}
			else{
				string title, content;
				int c;

				if(line[2]=="--title"){
					for(c=3;line[c]!="--content";c++){
						title += line[c];
						title += " ";
					}
					title.erase(title.end()-1);

					for(c++;c<line.size();c++){
						if(line[c].find("<br>") != std::string::npos){
							line[c]=line[c].replace(line[c].find("<"),4,"\n");
						}		
						content += line[c];
						if(c != line.size()-1){
							content += " ";
						}
					}
				}
				else if(line[2] == "--content"){
					for(c=3;line[c]!="--title";c++){
						if(line[c].find("<br>") != std::string::npos){
							line[c]=line[c].replace(line[c].find("<"),4,"\n");
						}		
						content += line[c];
						content += " ";
					}
					content.erase(content.end()-1);
					content += "\n";

					for(c++;c<line.size();c++){
						title += line[c];
						if(c != line.size()-1){
							title += " ";
						}
					}
					title.erase(remove(title.begin(), title.end(), '\n'), title.end());
				}

				Post new_post;
				new_post.author = client_login_name[clientNum];
				new_post.sn = to_string(postSN);
				new_post.title = title;
				new_post.content = content;
				new_post.date = Date();
				new_post.board = line[1];
				post_list.push_back(new_post);
				post_num_list.push_back(to_string(postSN));

				postSN++;
				Send("Create post successfully.\n", fd);
			}
		}
	}
	else if(line.size() == 2 && line[0] == "list-post"){
		line[1].erase(remove(line[1].begin(), line[1].end(), '\n'), line[1].end());

		if(count(board_name_list.begin(), board_name_list.end(), line[1])==0){
			Send("Board does not exist.\n", fd);
		}
		else{
			string m = "S/N Title Author Date\n";
			for(int i=0;i<post_list.size();i++){
				if(post_list[i].board == line[1]){
					m += post_list[i].sn;
					m += " ";
					m += post_list[i].title;
					m += " ";
					m += post_list[i].author;
					m += " ";
					m += post_list[i].date;
					m += "\n";	
				}
			}
			Send(m, fd);
		}
	}
	else if(line.size() == 2 && line[0] == "read"){
		line[1].erase(remove(line[1].begin(), line[1].end(), '\n'), line[1].end());

		if(count(post_num_list.begin(), post_num_list.end(), line[1])==0){
			Send("Post does not exist.\n", fd);
		}
		else{
			for(int i=0;i<post_list.size();i++){
				if(post_list[i].sn == line[1]){
					string m;
					m += "Author: " + post_list[i].author + "\n";
					m += "Title: " + post_list[i].title + "\n";
					m += "Date: " + post_list[i].date + "\n";
					m += "--\n";
					m += post_list[i].content;
					m += "--\n";
					
					for(int j=0;j<post_list[i].comment.size();j++){
						m += post_list[i].comment[j].first + ": " + post_list[i].comment[j].second;
					}
					
					Send(m, fd);
					break;
				}
			}
		}
	}
	else if(line.size() == 2 && line[0] == "delete-post"){
		line[1].erase(remove(line[1].begin(), line[1].end(), '\n'), line[1].end());

		if(client_login_name[clientNum] == ""){
			Send("Please login first.\n", fd);
		}
		else{
			if(count(post_num_list.begin(), post_num_list.end(), line[1])==0){
				Send("Post does not exist.\n", fd);
			}
			else{
				for(int i=0;i<post_list.size();i++){
					if(post_list[i].sn == line[1]){
						if(post_list[i].author == client_login_name[clientNum]){
							post_list.erase(post_list.begin()+i);
							post_num_list.erase(remove(post_num_list.begin(),post_num_list.end(), line[1]), post_num_list.end());
							Send("Delete successfully.\n", fd);
						}
						else{
							Send("Not the post owner.\n", fd);
						}
					}
				}
			}
		}
	}
	else if(line.size() >= 4 && line[0] == "update-post" && (line[2] == "--title" || line[2] == "--content")){
		if(client_login_name[clientNum] == ""){
			Send("Please login first.\n", fd);
		}
		else{
			if(count(post_num_list.begin(), post_num_list.end(), line[1])==0){
				Send("Post does not exist.\n", fd);
			}
			else{
				for(int i=0;i<post_list.size();i++){
					if(post_list[i].sn == line[1]){
						if(post_list[i].author == client_login_name[clientNum]){

							if(line[2] == "--title"){
								string tmp;
								for(int c=3;c<line.size();c++){
									tmp += line[c];
									if(c != line.size()-1){
										tmp += " ";
									}  
								}
								tmp.erase(remove(tmp.begin(), tmp.end(), '\n'), tmp.end());
								post_list[i].title = tmp;
							}
							else if(line[2] == "--content"){
								string tmp;
								for(int c=3;c<line.size();c++){
									if(line[c].find("<br>") != std::string::npos){
										line[c]=line[c].replace(line[c].find("<"),4,"\n");
									}
									
									tmp += line[c];
									if(c != line.size()-1){
										tmp += " ";
									}
								}
								post_list[i].content = tmp;
							}
							
							Send("Update successfully.\n", fd);
						}
						else{
							Send("Not the post owner.\n", fd);
						}
					}
				}
			}
		}
	}
	else if(line.size() >= 3 && line[0] == "comment"){
		if(client_login_name[clientNum] == ""){
			Send("Please login first.\n", fd);
		}
		else{
			if(count(post_num_list.begin(), post_num_list.end(), line[1])==0){
				Send("Post does not exist.\n", fd);
			}
			else{
				for(int i=0;i<post_list.size();i++){
					if(post_list[i].sn == line[1]){
						
						string tmp;
						for(int c=2;c<line.size();c++){
							tmp += line[c];
							if(c != line.size()-1){
								tmp += " ";
							}  
						}
									
						pair <string, string> cm;
						cm.first = client_login_name[clientNum];
						cm.second = tmp;
						post_list[i].comment.push_back(cm);
					}
				}
				Send("Comment successfully.\n", fd);
			}
		}
	}
	else{               
		Send("Error!!!\n", fd);
	}
}

int main(int argc, char *argv[]){
	
	if(argc>1){
        portNum = stoi(argv[1]);
    }

	for(int k=0;k<100;k++){
		client_login_name[k] = "";
	}

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
	for(i=1;i<100;i++){
		client[i].fd = -1;
	}
	maxi = 0;

	
	while(true){
		nready = poll(client, maxi+1, INFTIM);	

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

			for(i=1;i<100;i++){
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
	
		for(i=1;i<=maxi;i++){	
			if((sockfd = client[i].fd) < 0){
				continue;
			}
					
			if(client[i].revents & (POLLRDNORM | POLLERR)) {

				if((n = recv(sockfd, buf, 40960, 0)) < 0) {
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
	return 0;
}
