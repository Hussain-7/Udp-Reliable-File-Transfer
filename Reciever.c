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
#include <stdio.h>
#include <sys/time.h>


#define BUF_SIZE 500	//Max buffer size of the data in a packet
#define WIN_SIZE 5	//Window size

/*A packet with unique id, length and data*/
struct Packet {
	long int seqno;
	long int length;
	char data[BUF_SIZE];
};

/**
---------------------------------------------------------------------------------------------------
print_error
--------------------------------------------------------------------------------------------------
* This function prints the error message to console
*
* 	@\param msg	User message to print
*
* 	@\return	None
*
*/ 
static void print_error(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/*----------------------------------------Main loop-----------------------------------------------*/

int main(int argc, char **argv)
{
	if ((argc < 3) || (argc >3)) {
		printf("Client: Usage --> ./[%s] [IP Address] [Port Number]\n", argv[0]);  //Should have the IP of the server
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in send_addr, from_addr;
	// struct stat st;
	struct Packet packets[WIN_SIZE+1];
	struct Packet packet;

	char cmd_send[50];
	char flname[20];
	char cmd[10];
	char ack_send[4] = "ACK";
	
	ssize_t numRead = 0;
	ssize_t length = 0;
	// off_t f_size = 0;
	// long int ack_num = 0;
	int cfd;
	int ack_recv = 0;

	FILE *fptr;

	/*Clear all the data buffer and structure*/
	memset(ack_send, 0, sizeof(ack_send));
	memset(&send_addr, 0, sizeof(send_addr));
	memset(&from_addr, 0, sizeof(from_addr));

	/*Populate send_addr structure with IP address and Port*/
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = htons(atoi(argv[2]));
	send_addr.sin_addr.s_addr = inet_addr(argv[1]);

	if ((cfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		print_error("CLient: socket");

	for (;;) {

		memset(cmd_send, 0, sizeof(cmd_send));
		memset(cmd, 0, sizeof(cmd));
		memset(flname, 0, sizeof(flname));

		printf("\n<---- Menu ----->\n Enter any of the following commands \n 1.) get [file_name] \n 2.) delete [file_name] \n 3.) ls \n 5.) exit \nEnter Command : ");		
		scanf(" %[^\n]%*c", cmd_send);

		//printf("----> %s\n", cmd_send);
		
		sscanf(cmd_send, "%s %s", cmd, flname);		//parse the user input into command and filename

		if (sendto(cfd, cmd_send, sizeof(cmd_send), 0, (struct sockaddr *) &send_addr, sizeof(send_addr)) == -1) {
			print_error("Client: send");
		}
		char fname2[100];
    	strcpy(fname2,"Recieved_");
     	strcat(fname2,flname); 
		strcpy(flname,fname2);


/*----------------------------------------------------------------------"get case"-------------------------------------------------------------------------*/

		if ((strcmp(cmd, "get") == 0) && (flname[0] != '\0' )) {
			long int total_frame = 0;
			long int bytes_rec = 0, packetno = 1;

			recvfrom(cfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &from_addr, (socklen_t *) &length); //Get the total number of frame to recieve

			if (total_frame > 0) {
				sendto(cfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
				printf("----> %ld\n", total_frame);
				
				fptr = fopen(flname, "wb");	//open the file in write mode
                memset(&packets, 0, sizeof(packets));
				/*Recieve all the frames and send the acknowledgement sequentially*/
                int window_size=WIN_SIZE;
				while(packetno <= total_frame)
				{
                    REACK:
                    for (int i = 0; i < window_size; i++)
					{
                      
                        memset(&packet, 0, sizeof(packet));
                        if(packetno<=total_frame)
                        {

                            recvfrom(cfd, &(packet), sizeof(packet), 0, (struct sockaddr *) &from_addr, (socklen_t *) &length);
                            if (length > 0)
                            {
                                packets[(packet.seqno - 1) % WIN_SIZE] =packet;
                                packetno++;
                            }
                        }
                        else
                        {
                            window_size=i;
                            break;
                        }
					}
                    int nAcks=0;  
                    int sentSize=0;
                    for (int i = 0; i < window_size; i++)
					{ 

						sentSize = sendto(cfd, &(packets[i].seqno), sizeof(packets[i].seqno), 0, (struct sockaddr *) &send_addr, sizeof(send_addr));
				         if (sentSize > 0)
						 {
                            nAcks++;
                            printf("Ack for packet %ld sent\n", packets[i].seqno);
						 }
                         memset(&packet, 0, sizeof(packet));
					}          

				    if (nAcks < window_size)
					{
                        printf("In reac");
                        packetno-=window_size;
			        	goto REACK;
			        }
                    
                    for (int i = 0; i < window_size; i++)
					{
                        if(packets[i].length!=0)
                        {
                            fwrite(packets[i].data, 1, packets[i].length, fptr); /*Write the recieved data to the file*/
                            printf("packet.seqno ---> %ld	packet.length ---> %ld\n", packets[i].seqno, packets[i].length);
                            bytes_rec += packets[i].length;
                        }
					}          

				}
                packetno=packetno-1;
                if (packetno == total_frame) {
					printf("Complete File recieved\n");
				}
                else
                {
                    printf("\npacketno : %ld",packetno);
                    printf("Incomplete File recieved\n");
                }
				printf("Total bytes recieved ---> %ld\n", bytes_rec);
				fclose(fptr);
			}
			else {
				printf("File is empty\n");
			}
		}

/*----------------------------------------------------------------------"delete case"-------------------------------------------------------------------------*/

		else if ((strcmp(cmd, "delete") == 0) && (flname[0] != '\0')) {

			length = sizeof(from_addr);
			ack_recv = 0;
                                                                                                                                 
			if((numRead = recvfrom(cfd, &(ack_recv), sizeof(ack_recv), 0,  (struct sockaddr *) &from_addr, (socklen_t *) &length)) < 0)	//Recv ack from server
				print_error("recieve");
			
			if (ack_recv > 0)
				printf("Client: Deleted the file\n");
			else if (ack_recv < 0)
				printf("Client: Invalid file name\n");
			else
				printf("Client: File does not have appropriate permission\n");
		}

/*-----------------------------------------------------------------------"ls case"----------------------------------------------------------------------------*/

		else if (strcmp(cmd, "ls") == 0) {

			char filename[200];
			memset(filename, 0, sizeof(filename));

			length = sizeof(from_addr);

			if ((numRead = recvfrom(cfd, filename, sizeof(filename), 0,  (struct sockaddr *) &from_addr, (socklen_t *) &length)) < 0)
				print_error("recieve");

			if (filename[0] != '\0') {
				printf("Number of bytes recieved = %ld\n", numRead);
				printf("\nThis is the List of files and directories --> \n%s \n", filename);
			}
			else {
				printf("Recieved buffer is empty\n");
				continue;
			}
		}

/*----------------------------------------------------------------------"exit case"-------------------------------------------------------------------------*/

		else if (strcmp(cmd, "exit") == 0) {

			exit(EXIT_SUCCESS);

		}

/*--------------------------------------------------------------------"Invalid case"-------------------------------------------------------------------------*/

		else {
			printf("--------Invalid Command!----------\n");
		}


	}
		
	close(cfd);

	exit(EXIT_SUCCESS);
}
