
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netinet/in.h>
#include <signal.h>

#define MAX_BUFF 2048									// Max size of command string buffer
#define RCV_BUFF 12001									// Max size of recieved data buffer

int conv_cmd(char *buff, char *cmd_buff);
int process_result(char *rcv_buff);
void sh_int(int signum);								// signal handler for SIGINT

int sockfd;										// socket descripter

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// main															//
// ==================================================================================================================== //
// Input: argc -> number of parameters											//
// 	  **argv -> string array of parameters										//
// Output: int 0 success												//
// Purpose: Connect to server and receive command from user								//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
	char *buff = (char*)malloc(MAX_BUFF);							// user command buffer
	char *cmd_buff = (char*)malloc(MAX_BUFF);						// FTP command buffer
	char *rcv_buff = (char*)malloc(RCV_BUFF);						// buffer of recieved data from server
	int n;
	struct sockaddr_in server_addr;							// server address
	int error = 0;

	if (argc != 3)										// argument error
	{
		write(STDERR_FILENO, "Error: wrong usage - ./cli <IP> <PORT>\n", strlen("Error: wrong usage - ./cli <IP> <PORT>\n"));
		free(buff);
		free(cmd_buff);
		free(rcv_buff);
		exit(1);
	}
	
	signal(SIGINT, sh_int);									// apply signal handler

/////////////////////////////////////// Open socket & Connect to server //////////////////////////////////////////////////
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0)				// Create Socket
	{
		write(STDERR_FILENO, "Error: can't create socket.\n", sizeof("Error: can't create socket.\n"));		// error occur -> print error message and terminate program
		free(buff);
		free(cmd_buff);
		free(rcv_buff);
		exit(1);
	} // end of if

	memset((char*)&server_addr, '\0', sizeof(server_addr));				// initialze server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	server_addr.sin_port = htons(atoi(argv[2]));

	if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
	{
		write(STDERR_FILENO, "Error: can't connect.\n", sizeof("Error: can't connect.\n"));			// error occur -> print error message and terminate program
		free(buff);
		free(cmd_buff);
		free(rcv_buff);
		exit(1);
	} // end of if	
////////////////////////////////// end of Open socket & Connect to server ////////////////////////////////////////////////

////////////////////////////////////////// Communication with server /////////////////////////////////////////////////////
	while (1)
	{
		memset(buff, '\0', sizeof(buff));						// initialze buffers
		memset(cmd_buff, '\0', sizeof(cmd_buff));
		memset(rcv_buff, '\0', sizeof(rcv_buff));
	
		write(STDOUT_FILENO, "> ", sizeof("> "));
		int len = read(STDIN_FILENO, buff, MAX_BUFF);			// read user command
		if (buff[len - 1] == '\n') buff[len - 1] = '\0';

		if (strlen(buff) == 0)							// no command entered -> continue
		{
			write(STDERR_FILENO, "Error: no command\n\n", sizeof("Error: no command\n\n"));
			continue;
		}

		int value = conv_cmd(buff, cmd_buff);					// convert user command to FTP command
		if (value < 0)								// error occur
		{
			write(STDERR_FILENO, "Error: conv_cmd() error!!\n", sizeof("Error: conv_cmd() error!!\n"));
			error = -1;
			break;
		} // end of if

		n = strlen(cmd_buff);

		if (write(sockfd, cmd_buff, n) != n)					// send FTP command to server
		{
			write(STDERR_FILENO, "Error: write() error!!\n", sizeof("Error: write() error!!\n"));		// error occur -> print error message and terminate program
			error = -1;
			break;
		} // end of if
		
		if ((n = read(sockfd, rcv_buff, RCV_BUFF - 1)) < 0)			// recieve result from server
		{
			write(STDERR_FILENO, "Error: read() error!!\n", sizeof("Error: read() error!!\n"));		// error occur -> print error message and terminate program
			error = -1;
			break;
		} // end of if

		rcv_buff[n] = '\0';

		if (!strcmp(rcv_buff, "QUIT"))						// QUIT command -> terminate program
		{
			write(STDOUT_FILENO, "QUIT success\n", sizeof("QUIT success\n"));
			break;
		} // end of if

		if (process_result(rcv_buff) < 0)					// display ls command result
		{
			write(STDOUT_FILENO, "Error: process_result() error!!\n", sizeof("Error: process_result() error!!\n"));	// error occur -> print error message and terminate program
			error = -1;
			break;
		}
	} // end of while
///////////////////////////////////// end of Communication with server ///////////////////////////////////////////////////
	
	close(sockfd);
	if (buff != NULL) free(buff);
	if (cmd_buff != NULL) free(cmd_buff);
	if (rcv_buff != NULL) free(rcv_buff);
	
	if (error == -1) exit(1);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// conv_cmd														//
// ==================================================================================================================== //
// Input: *buff -> user command to be converted to FTP commend								//
// 	  *cmd_buff -> converted FTP command										//
// Output: int	 0 success												//
// 		-1 fail													//
// Purpose: convert user command to FTP command										//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int conv_cmd(char *buff, char *cmd_buff)
{
////////////////////////////////////// Check for options and convert command /////////////////////////////////////////////
	int aflag = 0, lflag = 0;
	char *argument = NULL;
	char *token = strtok(buff, " ");						// token of buff
///////////////////////////////////////////////////////// ls /////////////////////////////////////////////////////////////
	if (strcmp(token, "ls") == 0)
	{
		strcpy(cmd_buff, "NLST");
		while ((token = strtok(NULL, " ")) != NULL)				// check next token
		{
			if (argument != NULL && token[0] != '-')			// more than one argument exeption
			{
				strcpy(cmd_buff, "Error: one argument is required\n");
				return 0;
			} // end of if

			if (token[0] == '-' && strlen(token) > 1)			// option
			{
				for (int i = 1; token[i] != '\0'; i++)
				{
					if (token[i] == 'a') aflag++;			// aflag on
					else if (token[i] == 'l') lflag++;		// lflag on
					else						// invalid option exeption
					{
						strcpy(cmd_buff, "Error: invalid option\n");
						return 0;
					}
				} // end of for
			} // end of if
			else								// argument
			{
				argument = token;
			} // end of else
		} // end of while

		if (aflag > 0 || lflag > 0)						// option exist
		{
			if (aflag > 0 && lflag > 0) strcat(cmd_buff, " -al");		// -al option
			else if (aflag > 0) strcat(cmd_buff, " -a");			// -a option
			else strcat(cmd_buff, " -l");					// -l option
		} // end of if

		if (argument != NULL)							// argument exist
		{
			strcat(cmd_buff, " ");
			strcat(cmd_buff, argument);
		} // end of if
	} // end of if
////////////////////////////////////////////////////// end of ls /////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// dir ////////////////////////////////////////////////////////////
	else if (strcmp(token, "dir") == 0)
	{
		strcpy(cmd_buff, "LIST");

		token = strtok(NULL, " ");						// get next token
		if (token != NULL)
		{
			if (token[0] == '-')						// invalid option exeption
			{
				strcpy(cmd_buff, "Error: invalid option\n");
				return 0;
			} // end of if
			else								// argumnet exists
			{
				argument = token;
			} // end of else
		} // end of if

		token = strtok(NULL, " ");
		if (token != NULL)							// too many arguments
		{
			if (token[0] == '-')
			{
				strcpy(cmd_buff, "Error: invalid option\n");
			}  // end of if
			else
			{
				strcpy(cmd_buff, "Error: one argument is required\n");
			}  // end of else
			return 0;
		} // end of if

		if (argument != NULL)
		{
			strcat(cmd_buff, " ");
			strcat(cmd_buff, argument);					// set argument
		} // end of if
	} // end of else if
////////////////////////////////////////////////////// end of dir/////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// pwd ////////////////////////////////////////////////////////////
	else if (strcmp(token, "pwd") == 0)
	{
		strcpy(cmd_buff, "PWD");

		token = strtok(NULL, " ");
		if (token != NULL)
		{
			if (token[0] == '-')
			{
				strcpy(cmd_buff, "Error: invalid option\n");
			} // end of if
			else
			{
				strcpy(cmd_buff, "Error: argument is not required\n");
			} // end of else

			return 0;
		} // end of if
	} // end of else if
////////////////////////////////////////////////////// end of pwd ////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// cd /////////////////////////////////////////////////////////////
	else if (strcmp(token, "cd") == 0)
	{
		token = strtok(NULL, " ");						// get next token
		if (token != NULL)
		{
			if (token[0] == '-')						// invalid option exeption
			{
				strcpy(cmd_buff, "Error: invalid option\n");
				return 0;
			} // end of if
			else								// argumnet exists
			{
				if (strcmp(token, "..") == 0)				// CDUP
				{
					strcpy(cmd_buff, "CDUP");
				}
				else
				{
					strcpy(cmd_buff, "CWD");
				}
				argument = token;
			} // end of else
		} // end of if
		else									// argument is not exist exeption
		{
			strcpy(cmd_buff, "Error: argument is required\n");
			return 0;
		}

		token = strtok(NULL, " ");
		if (token != NULL)							// too many arguments
		{
			if (token[0] == '-')
			{
				strcpy(cmd_buff, "Error: invalid option\n");
			}  // end of if
			else
			{
				strcpy(cmd_buff, "Error: one argument is required\n");
			}  // end of else
			return 0;
		} // end of if

		if (argument != NULL)
		{
			if (strcmp(argument, "..") != 0)
			{
				strcat(cmd_buff, " ");
				strcat(cmd_buff, argument);					// set argument
			}
		} // end of if
	} // end of else if
////////////////////////////////////////////////////// end of cd /////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// mkdir //////////////////////////////////////////////////////////
	else if (strcmp(token, "mkdir") == 0)
	{
		strcpy(cmd_buff, "MKD");
		
		token = strtok(NULL, " ");						// get next token
		if (token == NULL)							// argument is not exist exeption
		{
			strcpy(cmd_buff, "Error: argument is required\n");
			return 0;
		}

		do {
			if (token[0] == '-')						// invalid option exeption
			{
				strcpy(cmd_buff, "Error: invalid option\n");
				return 0;
			} // end of if
			else
			{
				strcat(cmd_buff, " ");
				strcat(cmd_buff, token);					// set argument
			}
		} while ((token = strtok(NULL, " ")) != NULL);				// get arguments
	} // end of else if
/////////////////////////////////////////////////// end of mkdir /////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// delete ///////////////////////////////////////////////////////////
	else if (strcmp(token, "delete") == 0)
	{
		strcpy(cmd_buff, "DELE");
		
		token = strtok(NULL, " ");						// get next token
		if (token == NULL)							// argument is not exist exeption
		{
			strcpy(cmd_buff, "Error: argument is required\n");
			return 0;
		}

		do {
			if (token[0] == '-')						// invalid option exeption
			{
				strcpy(cmd_buff, "Error: invalid option\n");
				return 0;
			} // end of if
			else
			{
				strcat(cmd_buff, " ");
				strcat(cmd_buff, token);					// set argument
			}
		} while ((token = strtok(NULL, " ")) != NULL);				// get arguments
	} // end of else if
/////////////////////////////////////////////////// end of delete /////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// rmdir //////////////////////////////////////////////////////////
	else if (strcmp(token, "rmdir") == 0)
	{
		strcpy(cmd_buff, "RMD");
		
		token = strtok(NULL, " ");						// get next token
		if (token == NULL)							// argument is not exist exeption
		{
			strcpy(cmd_buff, "Error: one argument is required\n");
			return 0;
		}

		do {
			if (token[0] == '-')						// invalid option exeption
			{
				strcpy(cmd_buff, "Error: invalid option\n");
				return 0;
			} // end of if
			else
			{
				strcat(cmd_buff, " ");
				strcat(cmd_buff, token);					// set argument
			}
		} while ((token = strtok(NULL, " ")) != NULL);				// get arguments
	} // end of else if
/////////////////////////////////////////////////// end of rmdir /////////////////////////////////////////////////////////
////////////////////////////////////////////////////// rename ////////////////////////////////////////////////////////////
	else if (strcmp(token, "rename") == 0)
	{
		strcpy(cmd_buff, "RNFR");
		
		token = strtok(NULL, " ");						// get next token
		if (token != NULL)
		{
			if (token[0] == '-')						// invalid option exeption
			{
				strcpy(cmd_buff, "Error: invalid option\n");
				return 0;
			} // end of if
			else								// 1st argumnet exists
			{
				strcat(cmd_buff, " ");
				strcat(cmd_buff, token);				// set 1st argument
			} // end of else
		} // end of if
		else									// argument is not exist exeption
		{
			strcpy(cmd_buff, "Error: two arguments are required\n");
			return 0;
		}

		strcat(cmd_buff, " RNTO");
		
		token = strtok(NULL, " ");						// get next token
		if (token != NULL)
		{
			if (token[0] == '-')						// invalid option exeption
			{
				strcpy(cmd_buff, "Error: invalid option\n");
				return 0;
			} // end of if
			else								// 1st argumnet exists
			{
				strcat(cmd_buff, " ");
				strcat(cmd_buff, token);				// set 2nd argument
			} // end of else
		} // end of if
		else									// argument is not exist exeption
		{
			strcpy(cmd_buff, "Error: two arguments are required\n");
			return 0;
		}

		token = strtok(NULL, " ");
		if (token != NULL)							// too many arguments
		{
			if (token[0] == '-')
			{
				strcpy(cmd_buff, "Error: invalid option\n");
			}  // end of if
			else
			{
				strcpy(cmd_buff, "Error: two arguments are required\n");
			}  // end of else
			return 0;
		} // end of if
	} // end of else if
/////////////////////////////////////////////////// end of rename ////////////////////////////////////////////////////////
//////////////////////////////////////////////////////// quit ////////////////////////////////////////////////////////////
	else if (strcmp(token, "quit") == 0)			
	{
		strcpy(cmd_buff, "QUIT");

		token = strtok(NULL, " ");
		if (token != NULL)
		{
			if (token[0] == '-')
			{
				strcpy(cmd_buff, "Error: invalid option\n");
			} // end of if
			else
			{
				strcpy(cmd_buff, "Error: argument is not required\n");
			} // end of else

			return 0;
		} // end of if
	} // end of else if
////////////////////////////////////////////////////// end of quit ///////////////////////////////////////////////////////
//////////////////////////////////////////////////////// unknown /////////////////////////////////////////////////////////	
	else
	{
		strcpy(cmd_buff, "Error: command not found\n");
		return 0;
	} // end of else
//////////////////////////////////////////////////// end of unknown //////////////////////////////////////////////////////

	return 0;
///////////////////////////////////// end of check for option and convert command ////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// process_result													//
// ==================================================================================================================== //
// Input: *rcv_buff -> recieved ls command result from server								//
// Output: int 	0 success												//
// 		-1 fail													//
// Purpose: display ls command result recieved from server								//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int process_result(char *rcv_buff)
{
	int n = strlen(rcv_buff);
	if (write(STDOUT_FILENO, rcv_buff, n) != n)					// print result
	{
		return -1;
	} // end of if
	
	write(STDOUT_FILENO, "\n", strlen("\n"));
	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sh_int														//
// ==================================================================================================================== //
// Input: signum -> type of signal											//
// Output: void														//
// Purpose: signal handler for SIGINT											//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void sh_int(int signum)
{
	write(sockfd, "QUIT", strlen("QUIT"));		// send QUIT to server

	close(sockfd);					// close socket
	exit(0);					// terminate program
}


