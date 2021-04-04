#include <iostream>
#include <string>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "FileSys.h"
using namespace std;

int main(int argc, char* argv[]) {

    //networking part: create the socket and accept the client connection

    int sock = 1; //change this line when necessary!

    //mount the file system
    FileSys fs;
    fs.mount(sock); //assume that sock is the new socket created 
                    //for a TCP connection between the client and the server.   
 
    fs.mkdir("dir1");
	fs.cd("dir1");
	fs.create("file1");
	fs.append("file1", "Hello");
	fs.ls();
	fs.cat("file1");
	fs.head("file1", 2);
	fs.stat("file1");
	fs.home();
	fs.ls();
	fs.stat("dir1");

    //unmout the file system
    fs.unmount();

    return 0;
}