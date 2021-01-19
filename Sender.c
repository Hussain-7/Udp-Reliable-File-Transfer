
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


#define BUF_SIZE (500)		//Max buffer size of the data in a frame
#define WIN_SIZE  (5)      //Window Size

/*A frame packet with unique id, length and data*/
struct Packet {
	long int seqno;
	long int length;
	char data[BUF_SIZE];
};
/**
----------------------------------------------------------------------------------------------------
ls
---------------------------------------------------------------------------------------------------
* This function scans the current directory and updates the input file with a complete list of 
* files and directories present in the current folder.
*	
*	@\param f	Input file which will be updated with the list of current files and 
*			directories in the present folder
*
*	@\return	On success this function returns 0 and On failure this function returns -1
*
*/
int ls(FILE *f) 
{ 	
	struct dirent **dirent; int n = 0;
	if ((n = scandir(".", &dirent, NULL, alphasort)) < 0) { 
		perror("Scanerror"); 
		return -1; 
	}
        
	while (n--) {
		fprintf(f, "%s\n", dirent[n]->d_name);	
		free(dirent[n]); 
	}
	
	free(dirent); 
	return 0; 
}                                             


/**
-------------------------------------------------------------------------------------------------
print_error
------------------------------------------------------------------------------------------------
* This function prints the error message to console
*
* 	@\param msg	User message to print
*
* 	@\return	None
*
*/
static void print_error(const char *msg, ...)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/*-------------------------------------------Main loop-----------------------------------------*/

int main(int argc, char **argv)
{
	/*check for appropriate commandline arguments*/
	if ((argc < 2) || (argc > 2)) {				
		printf("Usage --> ./[%s] [Port Number]\n", argv[0]);		//Should have a port number > 5000
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in sv_addr, cl_addr;
	struct stat st;
	struct Packet packets[WIN_SIZE];
    // struct Packet ack_packets[WIN_SIZE];
	// struct timeval t_out = {0, 0};

	char msg_recv[BUF_SIZE];
	char flname_recv[20];         
	char cmd_recv[10];
    

	ssize_t numRead;
	ssize_t length;
	off_t f_size; 	
	long int ack_num = 0;    //Recieve frame acknowledgement
	int ack_send = 0;
	int sfd;
    int ack_number[WIN_SIZE+1];
	FILE *fptr;

	/*Clear the server structure - 'sv_addr' and populate it with port and IP address*/
	memset(&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sin_family = AF_INET;
	sv_addr.sin_port = htons(atoi(argv[1]));
	sv_addr.sin_addr.s_addr = INADDR_ANY;

	if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		print_error("Server: socket");

	if (bind(sfd, (struct sockaddr *) &sv_addr, sizeof(sv_addr)) == -1)
		print_error("Server: bind");

	for(;;) {
		printf("Server: Waiting for client to connect\n");

		memset(msg_recv, 0, sizeof(msg_recv));
		memset(cmd_recv, 0, sizeof(cmd_recv));
		memset(flname_recv, 0, sizeof(flname_recv));

		length = sizeof(cl_addr);

		if((numRead = recvfrom(sfd, msg_recv, BUF_SIZE, 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length)) == -1)
			print_error("Server: recieve");

		//print_msg("Server: Recieved %ld bytes from %s\n", numRead, cl_addr.sin_addr.s_addr);
		printf("Server: The recieved message ---> %s\n", msg_recv);

		sscanf(msg_recv, "%s %s", cmd_recv, flname_recv);

/*----------------------------------------------------------------------"get case"-------------------------------------------------------------------------*/

		if ((strcmp(cmd_recv, "get") == 0) && (flname_recv[0] != '\0')) {

			printf("Server: Get called with file name --> %s\n", flname_recv);

			if (access(flname_recv, F_OK) == 0) {			//Check if file exist
				
				int total_frame = 0;
                int resend_frame = 0;
                // int drop_frame = 0;
				// long int i = 0;
					
				stat(flname_recv, &st);
				f_size = st.st_size;			//Size of the file

				fptr = fopen(flname_recv, "rb");        //open the file to be sent
					
				if ((f_size % BUF_SIZE) != 0)
					total_frame = (f_size / BUF_SIZE) + 1;				//Total number of frames to be sent
				else
					total_frame = (f_size / BUF_SIZE);

				printf("Total number of packets ---> %d\n", total_frame);
					
				length = sizeof(cl_addr);

				sendto(sfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));	//Send number of packets (to be transmitted) to reciever
				recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);

				while (ack_num != total_frame)		//Check for the acknowledgement
				{
					/*keep Retrying until the ack matches*/
					sendto(sfd, &(total_frame), sizeof(total_frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr)); 
					recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);

					resend_frame++;
				}

				/*transmit data frames sequentially followed by an acknowledgement matching*/
                long int packetno=1;
                int window_size=WIN_SIZE;
                while(packetno <= total_frame)
                {
                    for (int j = 1; j <= window_size; j++)
				    {
                        memset(&packets[j], 0, sizeof(packets[j]));
                        ack_number[j-1] = 0;
                        packets[j].seqno = packetno;
                        packets[j].length = fread(packets[j].data, 1, BUF_SIZE, fptr);
                        if(packets[j].length<=0)
                        {
                            window_size=j-1;
                            break;
                        }
                        packetno++;
                    }


                   RESEND: 
                    // Loop for sending packets in the buffer array
		        	for (int j = 1; j <= window_size; j++)
					{
		                // Checking if ack for that packet is received or not
		                if (ack_number[j-1] == 0)
						{
		                    printf("Sending packet --> %ld\n", packets[j].seqno);
		                    sendto(sfd, &packets[j], sizeof(struct Packet), 0,(struct sockaddr *) &cl_addr, sizeof(cl_addr));
		                }
		          	}

                    for (int j = 1; j <= window_size; j++)
					{
		                recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);
                        
                        if(length>0&&ack_num>0 && ack_num <= total_frame)
                        {
                            ack_number[(ack_num-1)%WIN_SIZE]= 1;
                            printf("Ack for packet -> %ld\n", ack_num);
                        }
                        else
                        {
                            printf("<------------Resending a packet-------------->\n");
                            goto RESEND;
                        }
                        ack_num=0;

		          	}

                     // Zeroing array of Acks
	                 memset(ack_number, 0, sizeof(ack_number));

	                 // Zeroing array of packets
	                 memset(packets, 0, sizeof(packets));
                }
                printf("File sent\n");
				// for (i = 1; i <= total_frame; i++)
				// {
				// 	memset(&frame, 0, sizeof(frame));
				// 	ack_num = 0;
				// 	frame.seqno = i;
				// 	frame.length = fread(frame.data, 1, BUF_SIZE, fptr);

				// 	sendto(sfd, &(frame), sizeof(frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));		//send the frame
				// 	recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);	//Recieve the acknowledgement
                    

				// 	while (ack_num != frame.seqno)  //Check for ack
				// 	{
				// 		/*keep retrying until the ack matches*/
				// 		sendto(sfd, &(frame), sizeof(frame), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));
				// 		recvfrom(sfd, &(ack_num), sizeof(ack_num), 0, (struct sockaddr *) &cl_addr, (socklen_t *) &length);
				// 		printf("frame ---> %ld	dropped, %d times\n", frame.seqno, ++drop_frame);
						
				// 		resend_frame++;

				// 		printf("frame ---> %ld	dropped, %d times\n", frame.seqno, drop_frame);

				// 	}

				// 	resend_frame = 0;
				// 	drop_frame = 0;

				// 	printf("frame ----> %ld	Ack ----> %ld \n", i, ack_num);

				// 	if (total_frame == ack_num)
				// 		printf("File sent\n");
				// }


				fclose(fptr);
			}
			else {	
				printf("Invalid Filename\n");
			}
		}

/*----------------------------------------------------------------------"delete case"-------------------------------------------------------------------------*/

		else if ((strcmp(cmd_recv, "delete") == 0) && (flname_recv[0] != '\0')) {

			if(access(flname_recv, F_OK) == -1) {	//Check if file exist
				ack_send = -1;
				sendto(sfd, &(ack_send), sizeof(ack_send), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));
			}
			else{
				if(access(flname_recv, R_OK) == -1) {  //Check if file has appropriate permission
					ack_send = 0;
					sendto(sfd, &(ack_send), sizeof(ack_send), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr));
				}
				else {
					printf("Filename is %s\n", flname_recv);
					remove(flname_recv);  //delete the file
					ack_send = 1;
					sendto(sfd, &(ack_send), sizeof(ack_send), 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr)); //send the positive acknowledgement
				}
			}
		}

/*----------------------------------------------------------------------"ls case"----------------------------------------------------------------------------*/

		else if (strcmp(cmd_recv, "ls") == 0) {
			
			char file_entry[200];
			memset(file_entry, 0, sizeof(file_entry));

			fptr = fopen("a.log", "wb");	//Create a file with write permission

			if (ls(fptr) == -1)		//get the list of files in present directory
				print_error("ls");

			fclose(fptr);
			
			fptr = fopen("a.log", "rb");	
			int filesize = fread(file_entry, 1, 200, fptr);		

			printf("Filesize = %d	%ld\n", filesize, strlen(file_entry));
			
			if (sendto(sfd, file_entry, filesize, 0, (struct sockaddr *) &cl_addr, sizeof(cl_addr)) == -1)  //Send the file list
				print_error("Server: send");

			remove("a.log");  //delete the temp file
			fclose(fptr);
		}

/*--------------------------------------------------------------------"exit case"----------------------------------------------------------------------------*/

		else if (strcmp(cmd_recv, "exit") == 0) {
			close(sfd);   //close the server on exit call
			exit(EXIT_SUCCESS);
		}

/*--------------------------------------------------------------------"Invalid case"-------------------------------------------------------------------------*/

		else {
			printf("Server: Unkown command. Please try again\n");
		}
	}
	
	close(sfd);
	exit(EXIT_SUCCESS);
}
