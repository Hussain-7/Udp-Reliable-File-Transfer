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



#define BUFFER_SIZE (500)		//Max buffer size of the data in a frame
#define WINDOW_SIZE  (5)      //Window Size

/*A frame packet with unique id, length and data*/
struct Packet {
	long int seqNo;
	long int size;
	char data[BUFFER_SIZE];
};



static void print_error(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/*----------------------------------------Main loop-----------------------------------------------*/

int main(int argc, char **argv)
{
	if ((argc < 3) || (argc >3)) {
		printf("Client: Usage --> ./[%s] [IP Address] [Port Number]\n", argv[0]);  //IP and Port no are must
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in sender_addr, reciever_addr;
	struct Packet packets[WINDOW_SIZE+1];
	struct Packet packet;

	char command_send[50];
	char filename[20];
	char command[10];
	
	ssize_t bytesRead = 0;
	ssize_t length = 0;
	int reciever_fd;
	int ack_recv = 0;

	FILE *fileptr;

	/*Clear all the data buffer and structure*/
	memset(&sender_addr, 0, sizeof(sender_addr));
	memset(&reciever_addr, 0, sizeof(reciever_addr));

	/*Populate send_addr structure with IP address and Port*/
	sender_addr.sin_family = AF_INET;
	sender_addr.sin_port = htons(atoi(argv[2]));
	sender_addr.sin_addr.s_addr = inet_addr(argv[1]);

	if ((reciever_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		print_error("CLient: Error in socket creation");

	for (;;) {

		memset(command_send, 0, sizeof(command_send));
		memset(command, 0, sizeof(command));
		memset(filename, 0, sizeof(filename));

		printf("\n<---- Menu ----->\n Enter any of the following commands \n 1.) get [file_name] \n 2.) delete [file_name] \n 3.) ls \n 5.) exit \nEnter Command : ");		
		scanf(" %[^\n]%*c", command_send);
		
		sscanf(command_send, "%s %s", command, filename);		//parse the user input into command and filename

		if (sendto(reciever_fd, command_send, sizeof(command_send), 0, (struct sockaddr *) &sender_addr, sizeof(sender_addr)) == -1) {
			print_error("Client: Error in sending command");
		}
		char fname2[100];
    	strcpy(fname2,"Recieved_");
     	strcat(fname2,filename); 
		strcpy(filename,fname2);


/*----------------------------------------------------------------------"get case"-------------------------------------------------------------------------*/

		if ((strcmp(command, "get") == 0) && (filename[0] != '\0' )) {
			long int total_frame = 0;
			long int bytes_rec = 0, packetno = 1;

			recvfrom(reciever_fd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &reciever_addr, (socklen_t *) &length); //Get the total number of frame to recieve

			if (total_frame > 0) {
				sendto(reciever_fd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &sender_addr, sizeof(sender_addr));
				printf("----> %ld\n", total_frame);
				
				fileptr = fopen(filename, "wb");	//open the file in write mode
                memset(&packets, 0, sizeof(packets));
				/*Recieve all the frames and send the acknowledgement sequentially*/
                int window_size=WINDOW_SIZE;
				int succrecv=0;
				int nAcks=0;
				int window_size2=0;
				while(packetno <= total_frame)
				{
                    RERECIEVE:
                    for (int i = 0; i < window_size; i++)
					{
                      
                        memset(&packet, 0, sizeof(packet));
                        if(packetno<=total_frame)
                        {

                            recvfrom(reciever_fd, &(packet), sizeof(packet), 0, (struct sockaddr *) &reciever_addr, (socklen_t *) &length);
                            if (length > 0)
                            {
                                packets[(packet.seqNo - 1) % WINDOW_SIZE] =packet;
								succrecv++;
                                packetno++;
                            }

                        }
                        else
                        {
                            window_size=i;
                            break;
                        }
					}
                      
                    int sentSize=0;
                    for (int i = 0; i < window_size; i++)
					{ 

						if(packets[i].seqNo>0)
						{
						 	sentSize = sendto(reciever_fd, &(packets[i].seqNo), sizeof(packets[i].seqNo), 0, (struct sockaddr *) &sender_addr, sizeof(sender_addr));
				         	if (sentSize > 0)
						 	{
                            nAcks++;
                            printf("Ack for packet %ld sent\n", packets[i].seqNo);
						 	}
						}
					
                         memset(&packet, 0, sizeof(packet));
					}          
					
				   	if (nAcks < window_size)
					{
						window_size2=window_size;
                        window_size=window_size-nAcks;
			        	goto RERECIEVE;
			        }
					else{

						if(window_size2!=0)
					 	window_size=window_size2;
					}
					nAcks=0;
					succrecv=0;
                    
                    for (int i = 0; i < window_size; i++)
					{
                        if(packets[i].size!=0)
                        {
                            fwrite(packets[i].data, 1, packets[i].size, fileptr); /*Write the recieved data to the file*/
                            printf("packet.seqno ---> %ld	packet.length ---> %ld\n", packets[i].seqNo, packets[i].size);
                            bytes_rec += packets[i].size;
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
				fclose(fileptr);
			}
			else {
				printf("File is empty\n");
			}
		}

/*----------------------------------------------------------------------"delete case"-------------------------------------------------------------------------*/

		else if ((strcmp(command, "delete") == 0) && (filename[0] != '\0')) {

			length = sizeof(reciever_addr);
			ack_recv = 0;
                                                                                                                                 
			if((bytesRead = recvfrom(reciever_fd, &(ack_recv), sizeof(ack_recv), 0,  (struct sockaddr *) &reciever_addr, (socklen_t *) &length)) < 0)	//Recv ack from server
				print_error("recieve");
			
			if (ack_recv > 0)
				printf("Client: Deleted the file\n");
			else if (ack_recv < 0)
				printf("Client: Invalid file name entered\n");
			else
				printf("Client: File does not have appropriate permission\n");
		}

/*-----------------------------------------------------------------------"ls case"----------------------------------------------------------------------------*/

		else if (strcmp(command, "ls") == 0) {

			char filename[200];
			memset(filename, 0, sizeof(filename));

			length = sizeof(reciever_addr);

			if ((bytesRead = recvfrom(reciever_fd, filename, sizeof(filename), 0,  (struct sockaddr *) &reciever_addr, (socklen_t *) &length)) < 0)
				print_error("recieve");

			if (filename[0] != '\0') {
				printf("Number of bytes recieved = %ld\n", bytesRead);
				printf("\nThis is the List of files and directories --> \n%s \n", filename);
			}
			else {
				printf("Recieved buffer is empty\n");
				continue;
			}
		}

/*----------------------------------------------------------------------"exit case"-------------------------------------------------------------------------*/

		else if (strcmp(command, "exit") == 0) {
			printf("\nClosing Client Socket\n");
			exit(EXIT_SUCCESS);

		}

/*--------------------------------------------------------------------"Invalid case"-------------------------------------------------------------------------*/

		else {
			printf("--------Invalid Command!----------\n");
		}


	}
		
	close(reciever_fd);

	exit(EXIT_SUCCESS);
}
