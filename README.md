INTRODUCTION
------------
	This project on UDP Socket programming consists of a Reciever and the Sender. The reciever
	interacts with the user, gets the user command and sends it to the Sender. The main purpose
	of the Sender is to collect the data based on the user's input and send back to the Reciever.
	In this project,a reliable connectionless file transmission model is implemented using User 
	Datagram Protocol(UDP).

BUILD and RUN STEPS
-------------------
	Note:- The build system for this project uses GNU MAKE utility.

	PROJECT BUILD STEPS
	===================

	1.) Go to the project folder, cd [FolderName]/
	
	2.) Run make (This command compiles both Sender and Reciever present in a same folder 
	    and generates an respective executable)

	RUN STEPS
	=========

	For SERVER module:
	
		1.) ./server [PORT]

		Note:- Use an unreserved port > 5000

	For CLIENT module:

		1.) ./client [IP ADDRESS] [PORT]

		Note:- Use the address of the server and same port number used in server.

IMPLEMENTATION SUMMARY
----------------------
	The Reciever side implements an user interaction module and publishes the supported commands. 
	The following section covers in detail on how Reciever and Sender interact on each user command.

	The supported user command includes,

	get [filename]		-	This command is used to get the specified file from server to client
	==============
		
		SERVER side Implementation
		
			1.) On recieving the 'get' request from client, the server first checks if 
			    the filename is specified or not. If filename is null, then server does 
			    not send anything back to client
			2.) Then it checks if the specified file is present and have appropriate read 
			    permission. If the file is not present or does not have appropriate 
			    permission, then server does not send anything back to the client
			3.) If the specified file is valid .Then it opens the specified file, gets the file
			    size and calculates the number of frames required to send the file.
			4.) First it sends the total number of frames and then checks if the recieved 
			    acknowledegement matches to the total number of frames.When this is confirmed 
			    it moves to the next stage
			5.) Finally it sends the 5 packets at a time in sequential order since the window
			    size is set to 5 .Then it checks if ack for each packet is recieved.If all 
			    acks recieved it loops tothe next 5 packets else since stop and wait is followed
			    we goto send packet loop and loop until all packets in window are successfully 
			    recieved we wont move foward .Hence using selective repeat mechanism we only send 
			    those packet whose ack is not recieved.If all 5 acks are recieved only then we move
			    foward.
			6.) This procedure ensure Realiablity for sender side

		CLIENT side Implementation

			1.) Server does not transfer any message if the filename is NULL or not present
			    in the directory
			2.) if file name exist reciever recieves count of total no packets from the Sender
			    and checks if valid count of totalframes recieved only then Reciever sends ack for the 
			    count of total no of packets recieved.Then we enter a while loop which loops until packetno
			    is <= totalframes
			3.) Then in the while we use a for loop to loop through forloop which run windowsize time and  
			    packets array(size of window_size) and recieve a packets at a time and check there validity 
			    and stores them in 
			    packets array according to the seqno in the packet.This also handles the reordering at reciever
			    side. 
			4.) Next in a similar manner after storing all packets in packets array.For each packet in packets
			    array an acknowledgement is sent along with its seqno (which is unique for every packet).Then after
			    sending acks the nofacks sent is checked and if acks are less then windows size ,using goto label we 
			    go to the rerecieve label which is above the forloop for recieving packets from sender.
			5.) If noofacks is equal to windows size then loop through packets array an write data to the newly created
			    file.Next we loop the same procedure above unless while loops exits

	delete [filename]	-	This command is used to delete the specified file in the server side
	=================

		SERVER side Implementation

			1.) On recieving the 'delete' request from client, the server first checks if
			    the filename is specified or not. If filename is NULL, then the server 
			    sends an negative number or zero, to indicate the error
			2.) If the filename is valid, then the file is removed and a positive ack is sent

	ls			-	list all the files present in the server side
	==

		In this case, the server scans all the files and directories in the present folder and transfers
		the entire list to the client.

	exit			-	Close the client and server
	====
