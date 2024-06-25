#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_BUFF 2048
#define SEND_BUFF 2048
#define LISTENQ 5

// file status Linked List //
struct Node {
	struct dirent *entry;
	struct stat *status;
	struct Node *next;										// pointer to next node
};
void insert(struct dirent *entry, struct stat *status);
void deconstruct();
struct Node *list = NULL;										// pointer to first item of Linked List

// child process Linked List //
struct ProcessNode {
	pid_t pid;
	int port;
	time_t start_time;
	struct ProcessNode *next;									// pointer to next node
};
void p_insert(pid_t pid, int port, time_t start_time);
void p_delete(pid_t pid);
void p_print(time_t current_time);									// print current connected clients
void p_deconstruct();
struct ProcessNode *pHead = NULL, *pTail = NULL;
int nodeNum = 0;												// number of nodes of child process linked list

void sh_chld(int signum);											// Signal handler for SIGCHLD
void sh_alrm(int signum);											// Signal handler for SIGALRM
void sh_int(int signum);											// Signal handler for SIGINT
int client_info(struct sockaddr_in *client_addr);
int cmd_process();
int ls(int a, int l, char *path);

char *result_buff = NULL, *buff = NULL;
int connfd;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// main     													//
// ============================================================================================================ //
// Input: argc -> number of arguments										//
// 	  **argv -> argumnets array										//
// output: int	0 success											//
// Purpose: accept the connection with client and check command and execute it					//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
	int n, len;
	struct sockaddr_in server_addr, client_addr;							// address of server and client
	pid_t pid;
	int listen_fd;

	if (argc != 2)										// argument error
	{
		write(STDERR_FILENO, "Error: usage - ./srv <PORT>\n", strlen("Error: usage - ./srv <PORT>\n"));
		exit(1);
	}

	signal(SIGCHLD, sh_chld);									// Apply signal handler
	signal(SIGALRM, sh_alrm);
	signal(SIGINT, sh_int);

///////////////////////////////////////// Open socket and listen //////////////////////////////////////////////////
	if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)						// create socket
	{
		write(STDERR_FILENO, "Error: Server - Can't open stream socket.\n", sizeof("Error: Server - Can't open stream socket.\n"));	// error occur -> terminate
		exit(1);
	} // end of if

	memset((char*)&server_addr, '\0', sizeof(server_addr));						// initialze socket address
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(atoi(argv[1]));

	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)			// bind socket descripter and server address
	{
		write(STDERR_FILENO, "Error: Server - Can't bind local address.\n", sizeof("Error: Server - Can't bind local address.\n"));	// error occur -> terminate
		exit(1);
	} // end of if

	if (listen(listen_fd, LISTENQ) < 0)									// listen
	{
		write(STDERR_FILENO, "Error: Server - Can't listen.\n", sizeof("Error: Server - Can't listen.\n"));	// error occur -> terminate
		exit(1);
	} // end of if
////////////////////////////////////// end of Open socket and listen //////////////////////////////////////////////

	alarm(10);											// generate SIGALRM signal 10 seconds later

///////////////////////////////////////// Communication and execute ///////////////////////////////////////////////
	for ( ; ; )
	{
		len = sizeof(client_addr);
		connfd = accept(listen_fd, (struct sockaddr*)&client_addr, &len);			// accept the connection
		if (connfd < 0)
		{
			write(STDERR_FILENO, "Error: Server - accept failed.\n", sizeof("Error: Server - accept failed.\n"));	// error occur -> terminate
			exit(1);
		} // end of if

		if (client_info(&client_addr) < 0)							// display client IP address and port number
		{
			write(STDERR_FILENO, "Error: client_info() err!!\n", sizeof("Error: client_info() err!!\n"));		// error occur
		}
		
		if ((pid = fork()) == 0)									// Create child process
		{
/////////////////////////////////////////////////// Child process ////////////////////////////////////////////////////////
			close(listen_fd);
			buff = (char*)malloc(MAX_BUFF);							// buffer of recieved data
			result_buff = (char*)malloc(SEND_BUFF);							// buffer of data to be sent
			
			memset(buff, '\0', MAX_BUFF);
			memset(result_buff, '\0', SEND_BUFF);
			int buff_len;	
			while ((buff_len = read(connfd, buff, MAX_BUFF)) > 0)					// Receive string from client
			{
				buff[buff_len] = '\0';

				if (cmd_process(buff) < 0)							// execute command and store result
				{
					write(STDERR_FILENO, "Error: cmd_process() err!!\n", sizeof("Error: cmd_process() err!!\n"));	// error occur
					close(connfd);
					free(buff);
					free(result_buff);
					exit(1);
				} // end of if
					
				if (write(connfd, result_buff, strlen(result_buff)) != strlen(result_buff))		// Send the received string to client
				{
					write(STDERR_FILENO, "Error: write() error!!\n", sizeof("Error: wrtie() error!!\n"));
					close(connfd);
					free(buff);
					free(result_buff);
					exit(1);
				}

				if (strcmp(result_buff, "QUIT\n") == 0)						// terminate program
				{
					break;
				} // end of if
				memset(buff, '\0', MAX_BUFF);	
				memset(result_buff, '\0', SEND_BUFF);	
			} // end of while
			free(buff);
			free(result_buff);
			close(connfd);
			exit(0);
		} // end of if
/////////////////////////////////////////////////// end of Child process ////////////////////////////////////////////////
		else												// parent process part
		{	
			printf("Child Process ID : %d\n", pid);						// Print process ID of Child
			p_insert(pid, ntohs(client_addr.sin_port), time(NULL));				// insert child process into linked list
			alarm(1);									// generate SIGALRM to print child process list
		}

		close(connfd);
	} // end of for

////////////////////////////////////// end of Communication and execute ///////////////////////////////////////////
	
	if (pHead != NULL) p_deconstruct();
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// client_info 													//
// ============================================================================================================ //
// Input: *client_addr -> address of client									//
// output: int	0 success											//
// 		-1 fail												//
// Purpose: Print IP address and port number of Client								//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int client_info(struct sockaddr_in *client_addr)
{
///////////////////////////////////////////////// Print information //////////////////////////////////////////////
	char *ip = inet_ntoa(client_addr->sin_addr);						// get ip address of client
	if (ip == NULL) return -1;								// error occur
	char *port = (char*)malloc(MAX_BUFF);
	sprintf(port, "%d", ntohs(client_addr->sin_port));					// get port number of client

	write(STDOUT_FILENO, "==========Client info==========\n", sizeof("==========Client info=========="));
	write(STDOUT_FILENO, "client IP: ", sizeof("client IP: "));
	if (write(STDOUT_FILENO, ip, strlen(ip)) != strlen(ip))
	{
		return -1;									// error occur	
	} // end of if	
	write(STDOUT_FILENO, "\nclient port: ", sizeof("\nclient port: "));
	if (write(STDOUT_FILENO, port, strlen(port)) != strlen(port))
	{
		return -1;									// error occur
	} // end of if
	write(STDOUT_FILENO, "\n================================\n", sizeof("\n================================\n"));
/////////////////////////////////////////// end of Print information /////////////////////////////////////////////
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// cmd_process 													//
// ============================================================================================================ //
// Input: void													//
// output: int	0 success											//
// 		-1 fail												//
// Purpose: process FTP command and store result into result_buff						//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int cmd_process()
{
	if (strlen(buff) >= 7)
	{
		if (strncmp(buff, "Error: ", 7) == 0)		// invalid option or argument exeption
		{
			write(STDOUT_FILENO, buff, strlen(buff));
			strcpy(result_buff, buff);
			return 0;
		} // end of if
	} // end of if

	write(STDOUT_FILENO, "> ", strlen("> "));
	write(STDOUT_FILENO, buff, strlen(buff));	//write command
	printf("\t[%d]\n", getpid());			// write process id
////////////////////////////////////////////// Parsing command ///////////////////////////////////////////////////
	char *p = strtok(buff, " ");				// parse string -> get command
//////////////////////////////////////////////////// NLST ////////////////////////////////////////////////////////
	if (strcmp(p, "NLST") == 0)				// NLST
	{
		int a = 0, l = 0;
		char *path = NULL;
			
		path = strtok(NULL, " ");
			
		if (path != NULL)		//argument or option exist
		{
			if (path[0] == '-')	//option exist
			{
				if (strlen(path) == 2)	//option -a or -l -> increase flag
				{
					if (path[1] == 'a') a++;
					else l++;
				} // end of if
				else			// option -al
				{
					a++;
					l++;
				} // end of else
				path = strtok(NULL, " ");
			} // end of if
		} // end of if

		if (ls(a, l, path) < 0)		//call ls function with option and argument
		{
			return -1;
		}
	} // end of if
//////////////////////////////////////////////// end of NLST /////////////////////////////////////////////////////
/////////////////////////////////////////////////// LIST /////////////////////////////////////////////////////////
	else if (strcmp(p, "LIST") == 0)
	{
		char *path = NULL;
		path = strtok(NULL, " ");

		ls(1, 1, path);				// ls execute with -al option
	} // end of else if
//////////////////////////////////////////////// end of LIST /////////////////////////////////////////////////////
/////////////////////////////////////////////////// PWD /////////////////////////////////////////////////////////
	else if (strcmp(p, "PWD") == 0)
	{
		char *path = (char*)malloc(512);

		if (getcwd(path, 512) == NULL)	// get current working directory
		{
			write(STDERR_FILENO, "Error: getcwd() error\n", strlen("Error: getcwd() error\n"));	
			return -1;
		}

		strcpy(result_buff, "\"");
		strcat(result_buff, path);		// set path
		strcat(result_buff, "\" is current directory\n");

		free(path);
	} // end of else if
//////////////////////////////////////////////// end of PWD /////////////////////////////////////////////////////
/////////////////////////////////////////////////// CWD /////////////////////////////////////////////////////////
	else if (strcmp(p, "CWD") == 0)
	{
		char *path = NULL;
		path = strtok(NULL, " ");

		if (chdir(path) == -1)	// change current working directory
		{
			strcpy(result_buff, "Error: directory not found\n");
			write(STDOUT_FILENO, result_buff, strlen(result_buff));
			return 0;
		}

		path = (char*)malloc(512);
		getcwd(path, 512);
		strcpy(result_buff, "\"");
		strcat(result_buff, path);		// set path
		strcat(result_buff, "\" is current directory\n");

		free(path);
	} // end of else if
//////////////////////////////////////////////// end of CWD /////////////////////////////////////////////////////
/////////////////////////////////////////////////// CDUP /////////////////////////////////////////////////////////
	else if (strcmp(p, "CDUP") == 0)
	{
		char *path = NULL;	
		
		if (chdir("..") == -1)	// change current working directory
		{
			strcpy(result_buff, "Error: directory not found\n");
			write(STDOUT_FILENO, result_buff, strlen(result_buff));
			return 0;
		}

		path = (char*)malloc(512);
		getcwd(path, 512);
		strcpy(result_buff, "\"");
		strcat(result_buff, path);		// set path
		strcat(result_buff, "\" is current directory\n");

		free(path);
	} // end of else if
//////////////////////////////////////////////// end of CDUP /////////////////////////////////////////////////////
/////////////////////////////////////////////////// MKD /////////////////////////////////////////////////////////
	else if (strcmp(p, "MKD") == 0)
	{
		char *path = NULL;

		strcpy(result_buff, "");

		while ((path = strtok(NULL, " ")) != NULL)	// get arguments
		{
			if (mkdir(path, 0775) == -1)		// make new directory
			{
				strcat(result_buff, "Error: cannot create directory \'");
				strcat(result_buff, path);
				strcat(result_buff, "\': File exists\n");
				write(STDOUT_FILENO, result_buff, strlen(result_buff));
			}
			else
	 		{
				// store success message into result buffer
				strcat(result_buff, "MKD ");
				strcat(result_buff, path);
				strcat(result_buff, "\n");
			}			
		} // end of while
	} // end of else if
//////////////////////////////////////////////// end of MKD /////////////////////////////////////////////////////
/////////////////////////////////////////////////// DELE /////////////////////////////////////////////////////////
	else if (strcmp(p, "DELE") == 0)
	{
		char *path = NULL;

		strcpy(result_buff, "");

		while ((path = strtok(NULL, " ")) != NULL)	// get arguments
		{
			if (unlink(path) == -1)			// delete file (unlink)
			{
				strcat(result_buff, "Error: failed to delete \'");
				strcat(result_buff, path);
				strcat(result_buff, "\'\n");
				write(STDOUT_FILENO, result_buff, strlen(result_buff));
			}
			else
			{
				// store success message into result buffer
				strcat(result_buff, "DELE ");
				strcat(result_buff, path);
				strcat(result_buff, "\n");
			}			
		} // end of while
	} // end of else if
//////////////////////////////////////////////// end of DELE /////////////////////////////////////////////////////
/////////////////////////////////////////////////// RMD /////////////////////////////////////////////////////////
	else if (strcmp(p, "RMD") == 0)
	{
		char *path = NULL;

		strcpy(result_buff, "");

		while ((path = strtok(NULL, " ")) != NULL)	// get arguments
		{
			if (rmdir(path) == -1)			// remove directory
			{
				strcat(result_buff, "Error: failed to remove \'");
				strcat(result_buff, path);
				strcat(result_buff, "\'\n");
				write(STDOUT_FILENO, result_buff, strlen(result_buff));
			}
			else
			{
				// store success message into result buffer
				strcat(result_buff, "RMD ");
				strcat(result_buff, path);
				strcat(result_buff, "\n");
			}			
		} // end of while
	} // end of else if
//////////////////////////////////////////////// end of RMD /////////////////////////////////////////////////////
//////////////////////////////////////////////// RNFR & RNTO /////////////////////////////////////////////////////////
	else if (strcmp(p, "RNFR") == 0)
	{
		char *oldName = strtok(NULL, " ");	// get old name
	
		char *newName = strtok(NULL, " ");	// RNTO
		newName = strtok(NULL, " ");	// get new name
		
		if (rename(oldName, newName) == -1)		// rename old name into new name
		{	
			strcpy(result_buff, "Error: name to change already exists\n");
			write(STDOUT_FILENO, result_buff, strlen(result_buff));
			return 0;
		}
		else
		{
			strcpy(result_buff, "RNFR ");		// store success message into result buffer
			strcat(result_buff, oldName);
			strcat(result_buff, "\n");
			strcat(result_buff, "RNTO ");
			strcat(result_buff, newName);
			strcat(result_buff, "\n");
		}
	} // end of else if
//////////////////////////////////////////////// end of RMD /////////////////////////////////////////////////////
/////////////////////////////////////////////////// QUIT /////////////////////////////////////////////////////////
	else							// QUIT
	{
		strcpy(result_buff, "QUIT");			// store "QUIT" string into result_buff
	} // end of else
//////////////////////////////////////////////// end of QUIT /////////////////////////////////////////////////////
/////////////////////////////////////////// end of Parsing command ///////////////////////////////////////////////

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ls														//
// ============================================================================================================ //
// Input: a -> a option flag											//
// 	  l -> l option flag											//
// 	  *path -> path of directory to perform ls								//
// Output: int	 0 success											//
// 		-1 fail												//
// Purpose: function that perform ls command									//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int ls(int a, int l, char *path)
{
	if (path == NULL) path = ".";	// path is NULL -> mean current directory

/////////////////////////////////////////// error check //////////////////////////////////////////////////////////
	if (access(path, F_OK) < 0)
	{
		strcpy(result_buff, "Error: No such file or directory\n");	//wrong file or directory path error
		write(STDOUT_FILENO, result_buff, strlen(result_buff));
		return 0;
	} // end of if
	if (access(path, R_OK) < 0)				// pormission denied error
	{
		strcpy(result_buff, "Error: cannot access\n");
		write(STDOUT_FILENO, result_buff, strlen(result_buff));
		return 0;
	} // end of if
///////////////////////////////////////// end of check error /////////////////////////////////////////////////////

	struct stat pathStatus;
	lstat(path, &pathStatus);	//get status of 'path' file or directory
	
/////////////////////////////////////////////////// ls directory /////////////////////////////////////////////////
	if (S_ISDIR(pathStatus.st_mode))
	{
		int buffer_size = SEND_BUFF;

		DIR *dp;
		struct dirent *dirp;

		dp = opendir(path);				// open directory
		
		char *source = (char*)malloc(MAX_BUFF);
		while ((dirp = readdir(dp)) != NULL)		// read directory entry until all entry is read
		{
			if (dirp->d_ino != 0)
			{
				if (a > 0 || dirp->d_name[0] != '.')	// if option -a is activated, do not show hidden files
				{
					struct stat *status = (struct stat*)malloc(sizeof(struct stat));				
					sprintf(source, "%s/%s", path, dirp->d_name);	//path of directory entry file
					lstat(source, status);				// get status
					insert(dirp, status);				// insert into linked list by sorting ascending order
				} // end of if
			} //end of if
		} // end of while
		free(source);

/////////////////////////////////////////// -l option: detail information /////////////////////////////////////////
		if (l > 0)
		{
			for (struct Node *cur = list; cur != NULL; cur = cur->next)	//print all  directory entry
			{
				/////////////////// file type ////////////////////
				char *type;
				if (S_ISREG(cur->status->st_mode)) type = "-";		//regular file
				else if (S_ISDIR(cur->status->st_mode)) type = "d";	// directory
				else if (S_ISBLK(cur->status->st_mode)) type = "b";	// block special file
				else if (S_ISCHR(cur->status->st_mode)) type = "c";	// character special file
				else if (S_ISLNK(cur->status->st_mode)) type = "l";	// soft link
				else if (S_ISFIFO(cur->status->st_mode)) type = "p";	// fifo
				else if (S_ISSOCK(cur->status->st_mode)) type = "s";	// socket
				else type = "-";
				
				if (cur == list) // first entry
				{
					strcpy(result_buff, type);
				} // end of if
				else
				{
					strcat(result_buff, type);
				} // end of else

				//////////////// end of file type ////////////////

				//////////////////////////// file permission ///////////////////////////
				if (cur->status->st_mode & S_IRUSR) strcat(result_buff, "r");		//user read
				else strcat(result_buff, "-");	//not
				if (cur->status->st_mode & S_IWUSR) strcat(result_buff, "w");		//user write
				else strcat(result_buff, "-");	//not
				if (cur->status->st_mode & S_IXUSR)					//user execute
				{
					if (cur->status->st_mode & S_ISUID) strcat(result_buff, "s"); //set uid on
					else strcat(result_buff, "x");	
				}
				else //not
				{
					if (cur->status->st_mode & S_ISUID) strcat(result_buff, "S"); //set uid on
					else strcat(result_buff, "-");
				}

				if (cur->status->st_mode & S_IRGRP) strcat(result_buff, "r");		//group r
				else strcat(result_buff, "-"); //not
				if (cur->status->st_mode & S_IWGRP) strcat(result_buff, "w");		//group w
				else strcat(result_buff, "-"); //not
				if (cur->status->st_mode & S_IXGRP)					//group x
				{
					if (cur->status->st_mode & S_ISGID) strcat(result_buff, "s"); //set group id on
					else strcat(result_buff, "x");
				}
				else //not
				{
					if (cur->status->st_mode & S_ISGID) strcat(result_buff, "S"); //set group id on
					else strcat(result_buff, "-");
				}

				if (cur->status->st_mode & S_IROTH) strcat(result_buff, "r");		//other r
				else strcat(result_buff, "-"); //not
				if (cur->status->st_mode & S_IWOTH) strcat(result_buff, "w");		//other w
				else strcat(result_buff, "-"); //not
				if (cur->status->st_mode & S_IXOTH) //other x
				{
					if (cur->status->st_mode & S_ISVTX) strcat(result_buff, "t "); //sticky bit
					else strcat(result_buff, "x ");
				}
				else //not
				{
					if (cur->status->st_mode & S_ISVTX) strcat(result_buff, "T "); //sticky bit
					else strcat(result_buff, "- ");
				}
				//////////////////////////// end of file permission ////////////////////////////

				char *value = (char*)malloc(MAX_BUFF);
				// number of links
				sprintf(value, "%2d ", (int)cur->status->st_nlink);
				strcat(result_buff, value);
				
				// owner
				struct passwd *user = getpwuid(cur->status->st_uid);		// get user ID
				strcat(result_buff, user->pw_name);
				strcat(result_buff, " ");
				struct group *owner_group = getgrgid(cur->status->st_gid);	// get group ID
				strcat(result_buff, owner_group->gr_name);

				// file size
				sprintf(value, " %6d ", (int)cur->status->st_size);
				strcat(result_buff, value);
				
				// last modified time
				struct tm *last_time = localtime(&(cur->status->st_mtime));
				strftime(value, MAX_BUFF, "%b %d ", last_time);				//write month, day
				strcat(result_buff, value);				
				sprintf(value, "%02d:%02d ", last_time->tm_hour, last_time->tm_min);	//write hour, minute
				strcat(result_buff, value);

				//file name
				strcat(result_buff, cur->entry->d_name);
				if (S_ISDIR(cur->status->st_mode)) strcat(result_buff, "/");	// if directory
				strcat(result_buff, "\n");

				free(value);
				
				int current_size = strlen(result_buff);
				if (buffer_size - current_size <= 500) {
					current_size *= 2;
					result_buff = (char*)realloc(result_buff, current_size);		// if result_buff is insufficient in size, expand size.
					buffer_size = current_size;
				} // end of if
			}
		} //end of if
/////////////////////////////////////////// end of -l option /////////////////////////////////////////////
/////////////////////////////////////////// not -l option /////////////////////////////////////////
		else
		{
			struct Node *cur = list;
			int line = 0;

			while (cur != NULL)	//print all directory entries in linked list
			{
				if (cur == list) strcpy(result_buff, cur->entry->d_name);			//write file name - first node
				else strcat(result_buff, cur->entry->d_name);					//write file name
				
				if (S_ISDIR(cur->status->st_mode)) strcat(result_buff, "/");			// if directory
				
				if (line < 4)
				{
					strcat(result_buff, "\t");
				}
				else
				{
					strcat(result_buff, "\n");						// when number of printed file name reach to 5 -> change line
				}

				line = (line + 1) % 5;
				cur = cur->next;	//next entry

				int current_size = strlen(result_buff);
				if (buffer_size - current_size <= 500) {
					current_size *= 2;
					result_buff = (char*)realloc(result_buff, current_size);		// if result_buff is insufficient in size, expand size.
					buffer_size = current_size;
				} // end of if
			} // end of while

			if (line > 0)
			{
				strcat(result_buff, "\n");
			}
		}
	
		deconstruct();	//unallocate memory of linked list
		closedir(dp);	//close directory
	}
	////////////////////////////////////// end of ls directory ////////////////////////////////////
	else
	{
		strcpy(result_buff, "Error: Not directory\n");	//not directory path error
		write(STDOUT_FILENO, result_buff, strlen(result_buff));
		return 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// insert													//
// ============================================================================================================ //
// input: *entry -> directory entry by readdir function								//
// 	  *status -> status of file										//
// output: void													//
// Purpose: insert new node into linked list by sorting acsending order						//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void insert(struct dirent *entry, struct stat *status)
{
	struct Node *new_node = (struct Node*)malloc(sizeof(struct Node));	// allocate new node
	
	new_node->entry = entry;
	new_node->status = status;
	new_node->next = NULL;

	if (list == NULL)	//first insertion
	{
		list = new_node;
	} // end of if
	else
	{
		struct Node *prev = NULL;
		for (struct Node *cur = list; cur != NULL; cur = cur->next)	//search the insertion position by acsending order
		{
			if (strcmp(new_node->entry->d_name, cur->entry->d_name) < 0)	// meet larger value -> insertion point
			{
				if (prev == NULL) list = new_node;	// first node
				else prev->next = new_node;

				new_node->next = cur;			//link together
				break;
			}
			prev = cur;
		}

		if (new_node->next == NULL)	//insert into last position
		{
			prev->next = new_node;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deconstruct													//
// ============================================================================================================ //
// Input: void													//
// Output: void													//
// Purpose: unallocate memory of linked list									//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void deconstruct()
{
	struct Node *prev = NULL;
	while (list != NULL)		//until reach to last node
	{
		prev = list;
		list = list->next;	//move to next
		if (prev->status != NULL) free(prev->status);
		free(prev);		//unallocate memory
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sh_chld     														//
// ============================================================================================================ 	//
// Input: signum -> type of signal											//
// output: void														//
// Purpose: Signal handler for SIGCHLD											//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sh_chld(int signum)
{
	pid_t pid = wait(NULL);									// get terminated child process id
	printf("Client(%d)'s Release\n", pid);							// print
	p_delete(pid);										// delete from linked list
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sh_alrm     														//
// ============================================================================================================ 	//
// Input: signum -> type of signal											//
// output: void														//
// Purpose: Signal handler for SIGALRM											//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sh_alrm(int signum)
{
	p_print(time(NULL));									// print all child process
	alarm(10);										// generate SIGALRM 10 seconds later
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sh_int     														//
// ============================================================================================================ 	//
// Input: signum -> type of signal											//
// output: void														//
// Purpose: Signal handler for SIGINT											//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sh_int(int signum)
{
	if (buff != NULL) free(buff);
	if (result_buff != NULL) free(result_buff);
	if (pHead != NULL) p_deconstruct();
	close(connfd);		// close socket
	exit(0);		//terminate process
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// p_insert													//
// ============================================================================================================ //
// input: pid -> child process id										//
// 	  port -> port number of client										//
// 	  start_time -> start time(second) of child process							//
// output: void													//
// Purpose: insert child process node into linked list at last position						//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void p_insert(pid_t pid, int port, time_t start_time)
{
	struct ProcessNode *newNode = (struct ProcessNode*)malloc(sizeof(struct ProcessNode));						// allocate new node

	newNode->pid = pid;														// setting values
	newNode->port = port;
	newNode->start_time = start_time;
	newNode->next = NULL;

	if (pHead == NULL)														// first insertion
	{
		pHead = newNode;
		pTail = newNode;
	} // end of if
	else																// insert into last position
	{
		pTail->next = newNode;
		pTail = newNode;
	} // end of else

	nodeNum++;															// increase number of node
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// p_delete													//
// ============================================================================================================ //
// input: pid -> child process id										//
// output: void													//
// Purpose: delete child process node of pid in linked list							//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void p_delete(pid_t pid)
{
	struct ProcessNode *cur = pHead;
	struct ProcessNode *prev = NULL;

	while (cur != NULL)														// search the node that has pid
	{
		if (cur->pid == pid)													// search success -> delete this node
		{
			if (cur == pHead)												// delete first node
			{
				pHead = cur->next;
				if (pHead == NULL) pTail = NULL;
				free(cur);
			}
			else
			{
				prev->next = cur->next;											// connect toghter
				if (prev->next == NULL) pTail = prev;
				free(cur);
			}

			nodeNum--;													// decrease number of node
			break;
		} // end of if

		prev = cur;
		cur = cur->next;													// move to next node
	} // end of while
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// p_print													//
// ============================================================================================================ //
// Input: current_time -> current time(second) used to get child process time					//
// output: void													//
// Purpose: print all child process information									//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void p_print(time_t current_time)
{
	printf("Current Number of Client :  %d\n", nodeNum);										// print current number of client
	printf("  PID\t PORT\t TIME\n");

	struct ProcessNode *cur = pHead;
	while (cur != NULL)
	{
		printf("%5d\t", cur->pid);
		printf("%5d\t", cur->port);
		printf("%5ld\n", current_time - cur->start_time);

		cur = cur->next;													// move to next node
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deconstruct													//
// ============================================================================================================ //
// Input: void													//
// Output: void													//
// Purpose: unallocate memory of child process linked list							//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void p_deconstruct()
{
	struct ProcessNode *prev = NULL;
	while (pHead != NULL)		//until reach to last node
	{
		prev = pHead;
		pHead = pHead->next;	//move to next
		free(prev);		//unallocate memory
	}
}


