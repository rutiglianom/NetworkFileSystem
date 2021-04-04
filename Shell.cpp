// CPSC 3500: Shell
// Implements a basic shell (command line interface) for the file system

#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#include "Shell.h"

static const string PROMPT_STRING = "NFS> ";	// shell prompt

// Mount the network file system with server name and port number in the format of server:port
void Shell::mountNFS(string fs_loc) {
	//create the socket cs_sock and connect it to the server and port specified in fs_loc
	//if all the above operations are completed successfully, set is_mounted to true  
	size_t divider = fs_loc.find(':');
	char ip_addr[(int) divider];
	char port[(int)fs_loc.length() - (int) divider];
	strcpy(ip_addr, (fs_loc.substr(0, divider)).c_str());
	strcpy(port, (fs_loc.substr(divider+1)).c_str());
	
	sockaddr_storage their_addr;
    socklen_t addr_size;
	addrinfo hints, *res;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    if(int rv = getaddrinfo(ip_addr, port, &hints, &res)){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

    // make a socket, connect to it
    cs_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (cs_sock == -1){
		perror("socket");
		exit(errno);
	}
    if(connect(cs_sock, res->ai_addr, res->ai_addrlen) == -1){
		close(cs_sock);
		perror("connect");
		exit(errno);
	}
	sockaddr_in* addr = (sockaddr_in*) res->ai_addr;
	cout << "Client: connecting to " << inet_ntoa((in_addr)addr->sin_addr) << endl;
	freeaddrinfo(res);
	is_mounted = true;
	cout << "Connected!\n";
}

// Unmount the network file system if it was mounted
void Shell::unmountNFS() {
	// close the socket if it was mounted
	close(cs_sock);
	is_mounted = false;
}

// Remote procedure call on mkdir
void Shell::mkdir_rpc(string dname) {
	dname.append("\r\n");
	string com = "mkdir ";
	com = com + dname;
	network_send(com);
	network_receive();
}

// Remote procedure call on cd
void Shell::cd_rpc(string dname) {
	dname.append("\r\n");
	string com = "cd ";
	com = com + dname;
	network_send(com);
	network_receive();
}

// Remote procedure call on home
void Shell::home_rpc() {
	network_send("home\r\n");
	network_receive();
}

// Remote procedure call on rmdir
void Shell::rmdir_rpc(string dname) {
	dname.append("\r\n");
	string com = "rmdir ";
	com = com + dname;
	network_send(com);
	network_receive();
}

// Remote procedure call on ls
void Shell::ls_rpc() {
	network_send("ls\r\n");
	network_receive();
}

// Remote procedure call on create
void Shell::create_rpc(string fname) {
	fname.append("\r\n");
	string com = "create ";
	com = com + fname;
	network_send(com);
	network_receive();
}

// Remote procedure call on append
void Shell::append_rpc(string fname, string data) {
	data.append("\r\n");
	fname.append(" ");
	string com = "append ";
	com = com + fname + data;
	network_send(com);
	network_receive();
}

// Remote procesure call on cat
void Shell::cat_rpc(string fname) {
	fname.append("\r\n");
	string com = "cat ";
	com = com + fname;
	network_send(com);
	network_receive();
}

// Remote procedure call on head
void Shell::head_rpc(string fname, int n) {
	fname.append(" " + to_string(n) + "\r\n");
	string com = "head ";
	com = com + fname;
	network_send(com);
	network_receive();
}

// Remote procedure call on rm
void Shell::rm_rpc(string fname) {
	fname.append("\r\n");
	string com = "rm ";
	com = com + fname;
	network_send(com);
	network_receive();
}

// Remote procedure call on stat
void Shell::stat_rpc(string fname) {
	fname.append("\r\n");
	string com = "stat ";
	com = com + fname;
	network_send(com);
	network_receive();
}

// Executes the shell until the user quits.
void Shell::run()
{
  // make sure that the file system is mounted
  if (!is_mounted)
 	return; 
  
  // continue until the user quits
  bool user_quit = false;
  while (!user_quit) {

    // print prompt and get command line
    string command_str;
    cout << PROMPT_STRING;
    getline(cin, command_str);

    // execute the command
    user_quit = execute_command(command_str);
  }

  // unmount the file system
  unmountNFS();
}

// Execute a script.
void Shell::run_script(char *file_name)
{
  // make sure that the file system is mounted
  if (!is_mounted)
  	return;
  // open script file
  ifstream infile;
  infile.open(file_name);
  if (infile.fail()) {
    cerr << "Could not open script file" << endl;
    return;
  }


  // execute each line in the script
  bool user_quit = false;
  string command_str;
  getline(infile, command_str, '\n');
  while (!infile.eof() && !user_quit) {
    cout << PROMPT_STRING << command_str << endl;
    user_quit = execute_command(command_str);
    getline(infile, command_str);
  }

  // clean up
  unmountNFS();
  infile.close();
}


// Executes the command. Returns true for quit and false otherwise.
bool Shell::execute_command(string command_str)
{
  // parse the command line
  struct Command command = parse_command(command_str);

  // look for the matching command
  if (command.name == "") {
    return false;
  }
  else if (command.name == "mkdir") {
    mkdir_rpc(command.file_name);
  }
  else if (command.name == "cd") {
    cd_rpc(command.file_name);
  }
  else if (command.name == "home") {
    home_rpc();
  }
  else if (command.name == "rmdir") {
    rmdir_rpc(command.file_name);
  }
  else if (command.name == "ls") {
    ls_rpc();
  }
  else if (command.name == "create") {
    create_rpc(command.file_name);
  }
  else if (command.name == "append") {
    append_rpc(command.file_name, command.append_data);
  }
  else if (command.name == "cat") {
    cat_rpc(command.file_name);
  }
  else if (command.name == "head") {
    errno = 0;
    unsigned long n = strtoul(command.append_data.c_str(), NULL, 0);
    if (0 == errno) {
      head_rpc(command.file_name, n);
    } else {
      cerr << "Invalid command line: " << command.append_data;
      cerr << " is not a valid number of bytes" << endl;
      return false;
    }
  }
  else if (command.name == "rm") {
    rm_rpc(command.file_name);
  }
  else if (command.name == "stat") {
    stat_rpc(command.file_name);
  }
  else if (command.name == "quit") {
    return true;
  }

  return false;
}

// Parses a command line into a command struct. Returned name is blank
// for invalid command lines.
Shell::Command Shell::parse_command(string command_str)
{
  // empty command struct returned for errors
  struct Command empty = {"", "", ""};

  // grab each of the tokens (if they exist)
  struct Command command;
  istringstream ss(command_str);
  int num_tokens = 0;
  if (ss >> command.name) {
    num_tokens++;
    if (ss >> command.file_name) {
      num_tokens++;
      if (ss >> command.append_data) {
        num_tokens++;
        string junk;
        if (ss >> junk) {
          num_tokens++;
        }
      }
    }
  }

  // Check for empty command line
  if (num_tokens == 0) {
    return empty;
  }
    
  // Check for invalid command lines
  if (command.name == "ls" ||
      command.name == "home" ||
      command.name == "quit")
  {
    if (num_tokens != 1) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "mkdir" ||
      command.name == "cd"    ||
      command.name == "rmdir" ||
      command.name == "create"||
      command.name == "cat"   ||
      command.name == "rm"    ||
      command.name == "stat")
  {
    if (num_tokens != 2) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "append" || command.name == "head")
  {
    if (num_tokens != 3) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else {
    cerr << "Invalid command line: " << command.name;
    cerr << " is not a command" << endl; 
    return empty;
  } 

  return command;
}

void Shell::network_send(string message){
	int numbytes, x;
	numbytes = 0;
	while (numbytes < (int) message.length()){
		x = send(cs_sock, (void*) message.c_str(), message.length(), 0);
		if (x == -1)
			perror("send");
		else if (x == 0){
			unmountNFS();
			exit(0);
		}
		numbytes += x;
	}
}

void Shell::network_receive(){
	int status_len = 100;
	int length_len = 25;
	char* status;
	char* length;
	char* body = nullptr;
	int numbytes;
	int x;
	// Receive Status
	status = new char(status_len);
	length = new char(length_len);
	numbytes = 0;
	while(status[-2] != '\r' && status[-1] != '\n'){
		x = recv(cs_sock, (void*) status, (size_t) status_len - numbytes, 0);
		if (x == -1){
			perror("recv");
		}
		// Client Disconnected
		else if (!x){
			unmountNFS();
			close(cs_sock);
			exit(0);
		}
		status += x;
		numbytes += x;
	}
	status -= numbytes;
	status[numbytes] = '\0';
	numbytes = 0;
	
	// Receive Length
	while(length[-4] != '\r' && length[-3] != '\n' && length[-2] != '\r' && length[-1] != '\n'){
		x = recv(cs_sock, (void*) length, (size_t) length_len - numbytes, 0);
		if (x == -1){
			perror("recv");
		}
		// Client Disconnected
		else if (!x){
			unmountNFS();
			close(cs_sock);
			exit(0);
		}
		length += x;
		numbytes += x;
	}
	// Pull length value from buffer
	length -= (numbytes - 7);
	char len_val[numbytes-11];
	for(int j=0; j< numbytes-11; j++)
		len_val[j] = length[j];
	length -= 7;
	int body_length = atoi(len_val);
	
	// Receive Body
	if (body_length){
		numbytes = 0;
		body = new char(body_length+50);
		while (numbytes < body_length){
			x = recv(cs_sock, (void*) body, (size_t) body_length - numbytes, 0);
			if (x == -1)
				perror("send");
			else if (x == 0){
				unmountNFS();
				exit(0);
			}
			numbytes += x;
			body += x;
		}
		body -= numbytes;
	}
	
	string total;
	total.append(status);
	if (body_length){
		total.append(body);
	}
	cout << total;
	
	delete [] status;
	delete [] length;
	if (body_length){
		delete [] body;
	}
}

