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

// struct dirent {
//     ino_t          d_ino;       /* inode number */
//     off_t          d_off;       /* offset to the next dirent */
//     unsigned short d_reclen;    /* length of this record */
//     unsigned char  d_type;      /* type of file; not supported
//                                    by all file system types */
//     char           d_name[256]; /* filename */ we  only use this
// };



int ls(FILE *f) 
{ 	
	struct dirent **dirent; int n = 0;
	if ((n = scandir(".", &dirent, NULL, alphasort)) < 0) { //upon return from scandir n will be set to no of files in current directory plus two more for the "." and ".." directory entries. 
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

	/*	typedef struct sockaddr_in {
		short          sin_family; The address family for the transport address. This member should always be set to AF_INET. which represents ipv4
		USHORT         sin_port;  A transport protocol port number.
		IN_ADDR        sin_addr; An IN_ADDR structure that contains an IPv4 address.Or the ip address
		CHAR           sin_zero[8]; this is reserved for system we donot use this
		};
	*/
	struct sockaddr_in sv_addr, cl_addr;
	struct stat st; //it is a type of structure used for storing details about file information
	struct Packet packets[WIN_SIZE];

	char msg_recv[BUF_SIZE];
	char flname_recv[20];         
	char cmd_recv[10];
    

	ssize_t numRead;
	ssize_t length;// len of bytes recieved
	off_t f_size; 	//file size
	long int ack_num = 0;    //Recieve packet acknowledgement
	int ack_send = 0;
	int sfd;
    int ack_number[WIN_SIZE+1];
	FILE *fptr;

	/*Clear the server structure - 'sv_addr' and populate it with port and IP address*/
	memset(&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sin_family = AF_INET;
	sv_addr.sin_port = htons(atoi(argv[1]));
	sv_addr.sin_addr.s_addr = INADDR_ANY;//Setting it to local host not defining specific ip.If using on different networks set public ip here

	/*
	socket() has 3 parameters
	   1)The domain argument specifies a communication domain; this selects  the
       protocol  family  which will be used for communication.  These families
       are defined in <sys/socket.h>.we set it to AF_INET - IPv4 Internet protocols
	   2) Next is the type, which specifies the communication semantics.
	   we set it to SOCK_DGRAM which represents udp protocol
	   3)Third is the  protocol which specifies  a  particular  protocol  to  be used with the
       socket.  Normally only a single protocol exists to support a particular
       socket  type within a given protocol family, in which case protocol can
       be specified as 0.
	*/
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

		printf("Server: The recieved message ---> %s\n", msg_recv);

		sscanf(msg_recv, "%s %s", cmd_recv, flname_recv);

/*----------------------------------------------------------------------"get case"-------------------------------------------------------------------------*/

		if ((strcmp(cmd_recv, "get") == 0) && (flname_recv[0] != '\0')) {

			printf("Server: Get called with file name --> %s\n", flname_recv);

			if (access(flname_recv, F_OK) == 0) {			//Check if file exist
				
				int total_frame = 0;
                int resend_frame = 0;
					
				/*
				stat() function is used to list properties of a file identified by path or filename. 
				It reads all file properties and dumps to st structure*/
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
				
                long int packetno=1;
                int window_size=WIN_SIZE;
				int noofacks=0;
                while(packetno <= total_frame)
                {
					//Saving the data in packets array.
                    for (int j = 1; j <= window_size; j++)
				    {
                        memset(&packets[j], 0, sizeof(packets[j]));
                        ack_number[j-1] = 0;
                        packets[j].seqno = packetno;
                        packets[j].length = fread(packets[j].data, 1, BUF_SIZE, fptr);
                        //only for last set of packets
						if(packets[j].length<=0)
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
							noofacks++;
                        }
                        else
                        {
                            printf("<------------Resending a packet----------->\n");
                            goto RESEND;
                        }
                        ack_num=0;

		          	}
					
					if(noofacks<window_size)
						goto RESEND;
					
					noofacks=0;
                    // Zeroing array of Acks
	                memset(ack_number, 0, sizeof(ack_number));

	                // Zeroing array of packets
	                memset(packets, 0, sizeof(packets));
                }
                printf("File sent Successfuly\n");
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
	printf("\nClosing Socket");
	close(sfd);
	exit(EXIT_SUCCESS);
}
