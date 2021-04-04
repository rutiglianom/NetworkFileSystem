// Matthew Rutigliano
// CPSC 3500 Computing Systems
// 6 March 2021

#include <iostream>
#include <cstring>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include "FileSys.h"
using namespace std;

void cleanExit(){exit(0);}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Usage: ./nfsserver port#\n";
        return -1;
    }

	sockaddr_storage their_addr;
    socklen_t addr_size;
	addrinfo hints, *res, *p;
    int sockfd, sock;
	int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
	

    if(int rv = getaddrinfo(NULL, argv[1], &hints, &res)){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

    // make a socket, bind it, and listen on it:
	// loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }
	sockaddr_in* addr = (sockaddr_in*) res->ai_addr;
	cout << "Address: " << inet_ntoa((in_addr)addr->sin_addr) << endl;
	freeaddrinfo(res);
    if (listen(sockfd, 1) == -1){
		perror("listen");
		exit(1);
	}

    // now accept an incoming connection:
    addr_size = sizeof their_addr;
    sock = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
	if (sock == -1){
		perror("socket");
		exit(errno);
	}
	
    //close the listening socket
	close(sockfd);

    // mount the file system
    FileSys fs;
    fs.mount(sock); //assume that sock is the new socket created 
                    //for a TCP connection between the client and the server.   
 
    //loop: get the command from the client and invoke the file
    //system operation which returns the results or error messages back to the clinet
    //until the client closes the TCP connection.
	int req_len = 200;
	char* req;
	string command;
	string arg;
	string arg2;
	int numbytes;
	int x, i;
	while(1){
		// Receive Message
		req = new char(req_len);
		numbytes = 0;
		while(req[-2] != '\r' && req[-1] != '\n'){
			x = recv(sock, (void*) req, (size_t) req_len, 0);
			if (x == -1){
				perror("recv");
			}
			// Client Disconnected
			else if (!x){
				fs.unmount();
				close(sock);
				exit(0);
			}
			req += x;
			numbytes += x;
		}
		i=0;
		req -= numbytes;
		
		// Get Command
		while((req[i] != ' ') && (req[i] != '\r')){
			command.append(1, req[i]);
			i++;
		}
		// Get Argument 1
		if (req[i] == ' ')
			i++;
		while((req[i] != ' ') && (req[i] != '\r')){
			arg.append(1, req[i]);
			i++;
		}
		// Get Argument 2
		if (req[i] == ' ')
			i++;
		while(req[i] != '\r'){
			arg2.append(1, req[i]);
			i++;
		}
		
		// Execute Command
		if (command == "mkdir")
			fs.mkdir(arg.c_str());
		else if (command == "ls")
			fs.ls();
		else if (command == "cd")
			fs.cd(arg.c_str());
		else if (command == "home")
			fs.home();
		else if (command == "rmdir")
			fs.rmdir(arg.c_str());
		else if (command == "create")
			fs.create(arg.c_str());
		else if (command == "append")
			fs.append(arg.c_str(), arg2.c_str());
		else if (command == "stat")
			fs.stat(arg.c_str());
		else if (command == "cat")
			fs.cat(arg.c_str());
		else if (command == "head") {
			fs.head(arg.c_str(), stoul(arg2));
		}
		else if (command == "rm")
			fs.rm(arg.c_str());
		else
			cout << "I got nothing\n";
		
		delete [] req;
		command.clear();
		arg.clear();
		arg2.clear();
	}

    //unmout the file system
    fs.unmount();
	
	signal(SIGTERM, (sighandler_t) cleanExit);
	signal(SIGINT, (sighandler_t) cleanExit);

    return 0;
}
