#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <sys/dir.h>


#define BUFFER_SIZE (500)		//Max buffer size of the data in a frame
#define WINDOW_SIZE  (5)      //Window Size

/*A frame packet with unique id, length and data*/
struct Packet {
	long int seqNo;
	long int size;
	char data[BUFFER_SIZE];
};


/**
----------------------------------------------------------------------------------------------------
ls
---------------------------------------------------------------------------------------------------
* This function scans the current directory and updates the input file with a complete list of 
* files and directories present in the current folder.
*	
*	@\parameters 	Input file which will be updated with the list of current files and 
*			        directories in the present folder
*
*	@\return	On success this function returns 0 and On failure this function returns -1
*
*/

int ls(FILE *fileptr) 
{ 	
	struct dirent **dir; 
	int no = 0;
	if ((no = scandir(".", &dir, NULL, alphasort)) < 0) { //upon return from scandir n will be set to no of files in current directory plus two more for the "." and ".." directory entries. 
		perror("Error in scanning directory"); 
		return -1; 
	}
        
	while (no--) {
		fprintf(fileptr, "%s\n", dir[no]->d_name);	
		free(dir[no]); 
	}
	
	free(dir); 
	return 0; 
}                                             


static void print_error(const char *message, ...)
{
	perror(message);
	exit(EXIT_FAILURE);
}

/*------------------------------------------[ Main loop ]-----------------------------------------*/

int main(int argc, char **argv)
{
	/*checking if argument are what are required*/
	if ((argc < 2) || (argc > 2)) {				
		printf("Usage --> ./[%s] [Port Number]\n", argv[0]);		//Should have a port number > 5000
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in server_addr, reciever_addr;
	struct stat file_struct; //it is a type of structure used for storing details about file information
	struct Packet packets[WINDOW_SIZE];

	char message_recv[BUFFER_SIZE];
	char filename_recv[20];         
	char command_recv[10];
    

	ssize_t bytes_Read;
	ssize_t client_addr_size;// len of bytes recieved
	off_t file_size; 	//file size
	long int recv_ack_num = 0;    //Recieve packet acknowledgement
	int send_ack_num = 0;
	int sender_fd;
    int acks[WINDOW_SIZE+1];
	FILE *fileptr;

	/*Clearingg the server structure - 'server_addr' and populating it with port and IP address*/

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	server_addr.sin_addr.s_addr = INADDR_ANY;//Setting it to local host not defining specific ip.If using on different networks set public ip here


	if ((sender_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		print_error("Server: Error in socket creation");

	if (bind(sender_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
		print_error("Server: bind");
	int connected=0;
	for(;;) {
		printf("Server: Waiting for client to connect\n");

		memset(message_recv, 0, sizeof(message_recv));
		memset(command_recv, 0, sizeof(command_recv));
		memset(filename_recv, 0, sizeof(filename_recv));

		client_addr_size = sizeof(reciever_addr);

		if((bytes_Read = recvfrom(sender_fd, message_recv, BUFFER_SIZE, 0, (struct sockaddr *) &reciever_addr, (socklen_t *) &client_addr_size)) == -1)
		print_error("Server: Error in recieving command from reciever");

		if(connected==0)
		{
			printf("Connected to Client: %s:%d\n\n",inet_ntoa(reciever_addr.sin_addr),ntohs(reciever_addr.sin_port));
			connected++;
		}
		
		printf("\n\nServer: The recieved message ---> %s\n", message_recv);

		sscanf(message_recv, "%s %s", command_recv, filename_recv);

/*----------------------------------------------------------------------"get case"-------------------------------------------------------------------------*/

		if ((strcmp(command_recv, "get") == 0) && (filename_recv[0] != '\0')) {

			printf("Server: Get called with file name --> %s\n", filename_recv);

			if (access(filename_recv, F_OK) == 0) {			//Check if file exist
				
				int total_packets = 0;
                int resend_packets = 0;
					
				stat(filename_recv, &file_struct);
				file_size = file_struct.st_size;			//Size of the file

				fileptr = fopen(filename_recv, "rb");        //open the file to be sent
					
				if ((file_size % BUFFER_SIZE) != 0)
					total_packets = (file_size / BUFFER_SIZE) + 1;				//Total number of frames to be sent
				else
					total_packets = (file_size / BUFFER_SIZE);

				printf("Total number of packets ---> %d\n", total_packets);
					
				client_addr_size = sizeof(reciever_addr);

				sendto(sender_fd, &(total_packets), sizeof(total_packets), 0, (struct sockaddr *) &reciever_addr, sizeof(reciever_addr));	//Send number of packets (to be transmitted) to reciever
				recvfrom(sender_fd, &(recv_ack_num), sizeof(recv_ack_num), 0, (struct sockaddr *) &reciever_addr, (socklen_t *) &client_addr_size);

				while (recv_ack_num != total_packets)		//Check for the acknowledgement
				{
					/*keep Retrying until the ack matches*/
					sendto(sender_fd, &(total_packets), sizeof(total_packets), 0, (struct sockaddr *) &reciever_addr, sizeof(reciever_addr)); 
					recvfrom(sender_fd, &(recv_ack_num), sizeof(recv_ack_num), 0, (struct sockaddr *) &reciever_addr, (socklen_t *) &client_addr_size);
					resend_packets++;
				}

                long int packetno=1;
                int window_size=WINDOW_SIZE;
				int noofacks=0;
                while(packetno <= total_packets)
                {
					//Saving the data in packets array.
                    for (int j = 1; j <= window_size; j++)
				    {
                        memset(&packets[j], 0, sizeof(packets[j]));
                        acks[j-1] = 0;
                        packets[j].seqNo = packetno;
                        packets[j].size = fread(packets[j].data, 1, BUFFER_SIZE, fileptr);
                        //only for last set of packets
						if(packets[j].size<=0)
                        {
                            window_size=j-1;
                            break;
                        }
                        packetno++;
                    }


                   RESEND: 
                    // Loop for sending packets in group of 5
		        	for (int j = 1; j <= window_size; j++)
					{
		                // Checking if ack for that packet is received or not
		                if (acks[j-1] == 0)
						{
		                    printf("Sending packet --> %ld\n", packets[j].seqNo);
		                    sendto(sender_fd, &packets[j], sizeof(struct Packet), 0,(struct sockaddr *) &reciever_addr, sizeof(reciever_addr));
		                }
		          	}

                    for (int j = 1; j <= window_size; j++)
					{
		                recvfrom(sender_fd, &(recv_ack_num), sizeof(recv_ack_num), 0, (struct sockaddr *) &reciever_addr, (socklen_t *) &client_addr_size);
                        
                        if(sizeof(recv_ack_num)>0&&recv_ack_num>0 && recv_ack_num <= total_packets)
                        {
                            acks[(recv_ack_num-1)%WINDOW_SIZE]= 1;
                            printf("Ack for packet -> %ld\n", recv_ack_num);
							noofacks++;
                        }
                        else
                        {
                            printf("<------------Resending a packet----------->\n");
                            goto RESEND;
                        }
                        recv_ack_num=0;
		          	}
					
					if(noofacks<window_size)
						goto RESEND;
					
					noofacks=0;
                    // Zeroing array of Acks
	                memset(acks, 0, sizeof(acks));

	                // Zeroing array of packets
	                memset(packets, 0, sizeof(packets));
                }
                printf("File sent Successfuly\n");
				fclose(fileptr);
			}
			else {	
				printf("Invalid Filename\n");
			}
		}

/*----------------------------------------------------------------------[ "delete Case" ]-------------------------------------------------------------------------*/

		else if ((strcmp(command_recv, "delete") == 0) && (filename_recv[0] != '\0')) {

			if(access(filename_recv, F_OK) == -1) {	//Checking if file exist
				send_ack_num = -1;
				sendto(sender_fd, &(send_ack_num), sizeof(send_ack_num), 0, (struct sockaddr *) &reciever_addr, sizeof(reciever_addr));
			}
			else{
				if(access(filename_recv, R_OK) == -1) {  //Checking if appropriate permission to access file
					send_ack_num = 0;
					sendto(sender_fd, &(send_ack_num), sizeof(send_ack_num), 0, (struct sockaddr *) &reciever_addr, sizeof(reciever_addr));
				}
				else {
					printf("Filename is %s\n", filename_recv);
					remove(filename_recv);  //deleting the file
					send_ack_num = 1;
					sendto(sender_fd, &(send_ack_num), sizeof(send_ack_num), 0, (struct sockaddr *) &reciever_addr, sizeof(reciever_addr)); //send the positive acknowledgement
				}
			}
		}

/*--------------------------------------------------------------------  [ "ls Case"  ]----------------------------------------------------------------------------*/

		else if (strcmp(command_recv, "ls") == 0) {
			
			char file_entry[200];
			memset(file_entry, 0, sizeof(file_entry));

			fileptr = fopen("a.log", "wb");	//Create a file with write permission

			if (ls(fileptr) == -1)		//get the list of files in present directory
				print_error("ls");

			fclose(fileptr);
			
			fileptr = fopen("a.log", "rb");	
			int filesize = fread(file_entry, 1, 200, fileptr);		

			printf("Filesize = %d	%ld\n", filesize, strlen(file_entry));
			
			if (sendto(sender_fd, file_entry, filesize, 0, (struct sockaddr *) &reciever_addr, sizeof(reciever_addr)) == -1)  //Send the file list
				print_error("Server: send");

			remove("a.log");  //delete the temp file
			fclose(fileptr);
		}

/*--------------------------------------------------------------------"exit case"----------------------------------------------------------------------------*/

		else if (strcmp(command_recv, "exit") == 0) {
			printf("\nClosing Server Socket\n");
			close(sender_fd);   //close the server on exit call
			exit(EXIT_SUCCESS);
		}

/*--------------------------------------------------------------------"Invalid case"-------------------------------------------------------------------------*/

		else {
			printf("Server: Unkown command. Please try again\n");
		}
	}
	printf("\nClosing Socket");
	close(sender_fd);
	exit(EXIT_SUCCESS);
}
