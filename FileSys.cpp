// CPSC 3500: File System
// Implements the file system commands that are available to the shell.
// Matthew Rutigliano
// March 2nd, 2021

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"

// mounts the file system
void FileSys::mount(int sock) {
  bfs.mount();
  curr_dir = 1; //by default current directory is home directory, in disk block #1
  fs_sock = sock; //use this socket to receive file system operations from the client and send back response messages
}

// unmounts the file system
void FileSys::unmount() {
  bfs.unmount();
  close(fs_sock);
}

// make a directory
void FileSys::mkdir(const char *name) {
	// Check name length
	if ((int) strlen(name) > MAX_FNAME_SIZE) {
		network_send("504 File name is too long");
		return;
	}
	// Check for duplicate
	dirblock_t curr;
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			network_send("502 File exists");
			return;
		}
	}
	// Check if disk is full
	short block = bfs.get_free_block();
	if (!block){
		network_send("505 Disk is full");
		return;
	}
	// Check if directory is full
	if (curr.num_entries == MAX_DIR_ENTRIES){
		network_send("506 Directory is full");
		return;
	}
	// Create directory
	dirblock_t dir = {
		DIR_MAGIC_NUM,	// magic
		0,				// num_entries
	};
	for (int i=0; i<MAX_DIR_ENTRIES; i++)
		dir.dir_entries[i].block_num = 0;
	
	bfs.write_block(block, (void*) &dir);
	
	// Update current directory
	strcpy(curr.dir_entries[curr.num_entries].name, name);
	curr.dir_entries[curr.num_entries].block_num = block;
	curr.num_entries++;
	bfs.write_block(curr_dir, (void*) &curr);
	network_send("200 OK");
}

// switch to a directory
void FileSys::cd(const char *name) {
	dirblock_t curr;
	dirblock_t dir;
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			// Check if file is directory
			if (!is_directory(curr.dir_entries[i].block_num)){
				network_send("500 File is not a directory");
				return;
			}
			curr_dir = curr.dir_entries[i].block_num;
			network_send("200 OK");
			return;
		}
	}
	network_send("503 File does not exist");
}

// switch to home directory
void FileSys::home() {
	curr_dir = 1;
	network_send("200 OK");
}

// remove a directory
void FileSys::rmdir(const char *name){
	dirblock_t curr;
	dirblock_t del;
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			// Check if file is directory
			if (!is_directory(curr.dir_entries[i].block_num)){
				network_send("500 File is not a directory");
				return;
			}
			bfs.read_block(curr.dir_entries[i].block_num, (void*) &del);
			// Check that directory is empty
			if (del.num_entries){
				network_send("507 Directory is not empty");
				return;
			}
			bfs.reclaim_block(curr.dir_entries[i].block_num);
			curr.dir_entries[i].block_num = 0;
			curr.num_entries--;
			bfs.write_block(curr_dir, (void*) &curr);
			network_send("200 OK");
			return;
		}
	}
	network_send("503 File does not exist");
}

// list the contents of current directory
void FileSys::ls(){
	string body;
	dirblock_t curr;
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		body.append(curr.dir_entries[i].name);
		if (is_directory(curr.dir_entries[i].block_num))
			body.append("/");
		body.append("\n");
	}
	network_send("200 OK", body);
}

// create an empty data file
void FileSys::create(const char *name){
	// Check name length
	if ((int) strlen(name) > MAX_FNAME_SIZE) {
		network_send("504 File name is too long");
		return;
	}
	// Check for duplicate
	dirblock_t curr;
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			network_send("502 File exists");
			return;
		}
	}
	// Check if disk is full
	short block = bfs.get_free_block();
	if (!block){
		network_send("505 Disk is full");
		return;
	}
	// Check if directory is full
	if (curr.num_entries == MAX_DIR_ENTRIES){
		network_send("506 Directory is full");
		return;
	}
	
	inode_t node = {
		INODE_MAGIC_NUM,	// magic
		0,					// size
	};
	for(int i=0; i<MAX_DATA_BLOCKS; i++)
		node.blocks[i] = 0;
	
	bfs.write_block(block, (void*) &node);
	
	// Update current directory
	strcpy(curr.dir_entries[curr.num_entries].name, name);
	curr.dir_entries[curr.num_entries].block_num = block;
	curr.num_entries++;
	bfs.write_block(curr_dir, (void*) &curr);
	network_send("200 OK");
}

// append data to a data file
void FileSys::append(const char *name, const char *data){
	dirblock_t curr;
	inode_t file;
	size_t len = strlen(data);
	// Finding file
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			// Check if file is directory
			if (is_directory(curr.dir_entries[i].block_num)){
				network_send("501 File is a directory");
				return;
			}
			bfs.read_block(curr.dir_entries[i].block_num, (void*) &file);
			// Checking filesize
			if ((file.size + (int) len) > MAX_FILE_SIZE){
				network_send("508 Append exceeds maximum file size");
				return;
			}
			int curr_block = file.size/BLOCK_SIZE;
			int head = file.size - (BLOCK_SIZE * curr_block);
			datablock_t write;
			// Load block if it has been allocated
			if (file.blocks[curr_block])
				bfs.read_block(file.blocks[curr_block],(void*) &write);
			
			for(int j=0; j<(int) len; j++){
				// If block is full
				if (head == BLOCK_SIZE){
					head = 0;
					if (!file.blocks[curr_block]){
						file.blocks[curr_block] = bfs.get_free_block();
						// Check if disk is full
						if (!file.blocks[curr_block]){
							network_send("505 Disk is full");
							return;
						}
					}
					bfs.write_block(file.blocks[curr_block], (void*) &write);
					curr_block++;
				}
				write.data[head] = data[j];
				head++;
				file.size++;
			}
			// Final write
			if (!file.blocks[curr_block]){
				file.blocks[curr_block] = bfs.get_free_block();
				// Check if disk is full
				if (!file.blocks[curr_block]){
					network_send("505 Disk is full");
					return;
				}
			}
			bfs.write_block(file.blocks[curr_block], (void*) &write);
			bfs.write_block(curr.dir_entries[i].block_num, (void*) &file);
			network_send("200 OK");
			return;
		}
	}
	network_send("503 File does not exist");
}

// display the contents of a data file
void FileSys::cat(const char *name){
	dirblock_t curr;
	inode_t file;
	datablock_t read;
	string body;
	// Finding file
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			// Check if file is directory
			if (is_directory(curr.dir_entries[i].block_num)){
				network_send("501 File is a directory");
				return;
			}
			bfs.read_block(curr.dir_entries[i].block_num, (void*) &file);
			int block = 0;
			for(int j=0; j<file.size; j++){
				if (!(j%BLOCK_SIZE))
					bfs.read_block(file.blocks[block++], (void*) &read);
				body.append(1, read.data[j%BLOCK_SIZE]);
			}
			network_send("200 OK", body);
			return;
		}
	}
	network_send("503 File does not exist");
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n){
	string body;
	dirblock_t curr;
	inode_t file;
	datablock_t read;
	// Finding file
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		// Check if file is directory
		if (!strcmp(curr.dir_entries[i].name, name)){
			if (is_directory(curr.dir_entries[i].block_num)){
				network_send("501 File is a directory");
				return;
			}
			bfs.read_block(curr.dir_entries[i].block_num, (void*) &file);
			int block = 0;
			if (n > file.size)
				n = file.size;
			for(int j=0; j<n; j++){
				if (!(j%BLOCK_SIZE))
					bfs.read_block(file.blocks[block++], (void*) &read);
				body.append(1, read.data[j%BLOCK_SIZE]);
			}
			network_send("200 OK", body);
			return;
		}
	}
	network_send("503 File does not exist");
}

// delete a data file
void FileSys::rm(const char *name){
	dirblock_t curr;
	inode_t del;
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			// Check if file is directory
			if (is_directory(curr.dir_entries[i].block_num)){
				network_send("501 File is a directory");
				return;
			}
			bfs.read_block(curr.dir_entries[i].block_num, (void*) &del);
			// Delete Blocks
			for (int j=0; j<(del.size/BLOCK_SIZE)+1; j++){
				bfs.reclaim_block(del.blocks[j]);
			}
			// Delete inode
			bfs.reclaim_block(curr.dir_entries[i].block_num);
			curr.dir_entries[i].block_num = 0;
			curr.num_entries--;
			bfs.write_block(curr_dir, (void*) &curr);
			network_send("200 OK");
			return;
		}
	}
	network_send("503 File does not exist");
}

// display stats about file or directory
void FileSys::stat(const char *name){
	string body;
	dirblock_t curr;
	bfs.read_block(curr_dir, (void*) &curr);
	for(int i=0; i<curr.num_entries; i++){
		if (!strcmp(curr.dir_entries[i].name, name)){
			// Directory
			if (is_directory(curr.dir_entries[i].block_num)){
				body.append("Directory name: ");
				body.append(curr.dir_entries[i].name);
				body.append("/\nDirectory block: " + to_string(curr.dir_entries[i].block_num) + "\n");
			}
			// File
			else {
				inode_t node;
				bfs.read_block(curr.dir_entries[i].block_num, (void*) &node);
				body.append("Inode block: " + to_string(curr.dir_entries[i].block_num) + "\nBytes in file: " + to_string(node.size) + "\nNumber of blocks: ");
				if (node.blocks[0])
					body.append(to_string(node.size/BLOCK_SIZE + 2));
				else
					body.append("1");
				body.append("\nFirst block: " + to_string(node.blocks[0]) + "\n");
			}
			network_send("200 OK", body);
			return;
		}
	}
	network_send("503 File does not exist");
}

// HELPER FUNCTIONS (optional)
bool FileSys::is_directory(short block){
	dirblock_t dir;
	bfs.read_block(block, (void*) &dir);
	return dir.magic == DIR_MAGIC_NUM;
}

// Send response with no body
void FileSys::network_send(string message){
	message.append("\r\n");
	string length = "Length:0\r\n\r\n";
	int numbytes, x;
	numbytes = 0;
	while (numbytes < (int) message.length()){
		x = send(fs_sock, (void*) message.c_str(), message.length(), 0);
		if (x == -1)
			perror("send");
		else if (x == 0){
			unmount();
			exit(0);
		}
		numbytes += x;
	}
	numbytes = 0;
	while (numbytes < (int) length.length()){
		x = send(fs_sock, (void*) length.c_str(), length.length(), 0);
		if (x == -1)
			perror("send");
		else if (x == 0){
			unmount();
			exit(0);
		}
		numbytes += x;
	}
}

// Send response with a body
void FileSys::network_send(string message, string body){
	message.append("\r\n");
	string length = "Length:" + to_string((int) body.length()) + "\r\n\r\n";
	int numbytes, x;
	numbytes = 0;
	while (numbytes < (int) message.length()){
		x = send(fs_sock, (void*) message.c_str(), message.length() - numbytes, 0);
		if (x == -1)
			perror("send");
		else if (x == 0){
			unmount();
			exit(0);
		}
		numbytes += x;
	}
	numbytes = 0;
	while (numbytes < (int) length.length()){
		x = send(fs_sock, (void*) length.c_str(), length.length() - numbytes, 0);
		if (x == -1)
			perror("send");
		else if (x == 0){
			unmount();
			exit(0);
		}
		numbytes += x;
	}
	numbytes = 0;
	while (numbytes < (int) body.length()){
		x = send(fs_sock, (void*) body.c_str(), body.length() - numbytes, 0);
		if (x == -1)
			perror("send");
		else if (x == 0){
			unmount();
			exit(0);
		}
		numbytes += x;
	}
}
