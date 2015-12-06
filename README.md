# Network-Programing---A2
Authors: Harishkumar Katagal(109915793) & Gagan Nagaraju (109889036)
CSE 533 - Assignment 2
********************************************************************************************
USER DOCUMENTATION:

Following are the files that contain the source codes for the assignment 2.
Client Side
	1. udpclient.c
	2. clientsupport.c
	3. clientsupport.h
	4. client.in

Server Side
	1. udpserver.c
	2. serversupport.c
	3. serversupport.h
	4. server.in
	5. rtt_mod.c
	6. unprtt_mod.h

To compile and generate executables use the "Makefile" and issue a "make". This will generate the *.o and the executable files. The two main important files that we want are "server" and "client". Use these two files to run the server and client respectively. To run the server use "./server" command, this will starts the server and creates listening sockets on port number specified in the server.in file. To run the client use "./client", we must specify the IP address of the server as arguments in the client.in file. Some sample examples are shown below.

Ex1:   	./server
		./client
	   
Note: When you issue make, you might get some warnings like "warning: assignment from incompatible pointer type", these can be safely ignored.

Output:
All the messages from the server and client are displayed in their corresponding terminals. At the client side, the data that is received is also dumped to a file named output.txt. You can use this file for checking if the received file is correct or not.

**************************************************************************************************
		
SYSTEM DOCUMENTATION:

1.	Datagram header format used:

We used the following data structure to store the socket file descriptor, IP address, network mask and subnet address. The unicast address obtained from the get_ifi_info() are stored in an array of structures of type sock_info. This same structure is used at both the server and the client side.

struct sock_info{
	int sockfd;
	struct sockaddr_in ipaddr;
	struct sockaddr_in netaddr;
	struct sockaddr_in subnetaddr;
};

In order, to not bind the broadcast address and wildcard address, we deleted the code that was binding the wildcard and broadcast address(ifi_brdaddr) in udpserv03.c. Hence only the unicast address(ifi_addr) is being binded to the socket.



2.Timeout mechanism at the sender (server)

*	To ensure reliable data transmission between the server and the client, a timeout mechanism was implemented at the sender side. The timeout mechanism described by Stevens was used with a couple of changes to suit our requirements and these are listed below.

*	The basic driving mechanism at the server was modified to follow a send-send-send progression instead of send-receive as mentioned in Stevens. The RTO, RTT and other parameters are calculated using integer arithmetic instead of floating point. All the rtt_xxx functions like rtt_stop, rtt_ts, rtt_minmax etc were modified appropriately to account for integer arithmetic.
 
*	The following modifications were made in the unprtt.h file:
	RTT_RXTMIN 		1 sec
	RTT_RXTMAX 		3 sec
	RTT_MAXNREXMT	12
	
*	To get a better precision, the calculations with respect to RTT, SRTT, RTTVAR and RTO were all done on a microsecond scale rather than seconds. The rtt_info structure was altered appropriately to store the values in microseconds.


3.ARQ sliding window mechanism and Flow Control

*	Sliding windows were implemented at the sender and receiver sides to ensure retransmission of lost datagrams and in order delivery of the received datagrams at the receiver. TCP based sliding windows were implemented and the sender and receiver window sizes were read from the server.in and client.in files repectively.

*	Sender Sliding Window: Three parameters namely: SendBase, NextSeqNum and EndWindow were used to implement the sliding windows. SendBase points to the oldest unacknowledged datagram, NextSeqNum points to the next datagram that can be sent and EndWindow points to the end of the sliding window. The sliding window size was determined dynamically based on the minimum of RWND, CWND, sender buffer size and number of datagrams to be sent. When valid ACK were received at the sender, the sliding window parameters were updated and the sliding window was moved appropriately.

* Receiver Sliding Window: The sliding window at the receiver was also implemented in a similar manner, but here the datagrams received were buffered and any out of order packets were stored instead of discarding them so that they can all be processed when the missing datagram arrives. The datagrams stay in the receive buffer until the consumer thread consumes the data while the producer thread keeps buffering the received datagrams restricted by the receiver buffer size that is specified. From the receiver side, once all the datagrams are read in inorder from the buffer, it sends a commulative ACK to the sender and the sender removes the corresponding data from its buffer.

* The receiver advertises the current RWND size to the sender when it sends the ACK. When the receive window becomes full, the receiver sends a duplicate ACK which acts as a window update when the receive buffer becomes free again. There are chances that this ACK might get lost and since this might cause a deadlock, a persist timer is implemented at the sender which causes the process to sleep for certain duration in the range of 1 to 60 seconds (each time doubling the time starting if the receive window still doesn’t open up after waking up from the sleep, starting with 1 sec). When the sender process wakes up, it sends probe signal and goes to sleep again. This continues until an ACK is received with RWND greater than zero. 


4. Congestion Control

To implement the congestion control, four state parameters were used namely: current_state, cwnd, ssthreshold, dup_ack_count

*	Slow Start: The sender initially starts in Slow Start phase with cwnd = 1 MSS( 1 datagram in our case), ssthreshold = 65535/512 and dup_ack_count  = 0. The window grows exponentially with cwnd being incremented by 1 MSS for every new ACK received. This continues till a first loss event occurs. If there is a timeout, then the same state is retained and the oldest outstanding datagram which has not yet been acknowledged is retransmitted. If the sender receives 3 duplicate ACKS before timeout, then the missing datagram is retransmitted, ssthreshold is reduced to half of the cwnd, cwnd is set to ssthreshold + 3 and the sender goes to fast recovery state. If the cwnd value becomes greater than ssthreshold then the sender goes to Congestion Avoidance state.
Note that we have covered a special case here, when the first packet with data is lost then ssthreshold becomes zero. We reset ssthreshold to 1 whenever it becomes zero.

*	Congestion Avoidance: In this state the cwnd grows linearly as opposed to the exponential growth in the Slow Start state. The cwnd value is increased by 1MSS for every RTT. If the sender receives three duplicate ACKs before the timeout then it goes to Fast Recovery state and retransmits the oldest unacknowledged datagram. If there is a timeout, then the sender retransmits the oldest datagram and goes back to Slow start phase.

*	Fast Recovery: In this state the cwnd is incremented by 1MSS for every duplicate ACK received by the sender and new datagrams are transmitted as allowed. If a new ACK arrives then the sender goes to Congestion Avoidance State after setting its cwnd value equal to ssthreshold. In case of timeout, the oldest outstanding datagram is retransmitted and the sender goes back to Slow Start phase with ssthreshold equal to half the cwnd and  cwnd is reset to 1 MSS.

5.End of File Message and Clean Closing of the  process using TIME_WAIT state

*	After the entire content of the requested file is transferred from the server to the client, the server sends a special datagram containing just two bytes of data and a flag set to indicate to the client that the data transfer is complete. When the ACK is received at the sender for this EOF datagram, the server child process exits and SIGCHLD signal is delivered to the parent process and the control goes to a SIGCHLD handler that prints out a message on stdout about the process ID of the child that was terminated, thus ensuring that no zombie children are left out.

*	In case the receiver does not send an ACK for the EOF datagram sent by the server, timeout occurs and the datagram is retransmitted. This continues either till a valid ACK arrives or the datagram has been retransmitted maximum number of times, after which the server child process terminates and it is cleaned up by the parent as mentioned above.

*	There are chances that the last ACK sent by the client may be lost. To ensure clean closing of the connection between the sender and the receiver, a TIME_WAIT state is implemented at the receiver where in it waits for a certain duration after sending an ACK for the EOF datagram and doesn’t terminate immediately in case it has to retransmit the final ACK. After this duration, the receiver process exits.

6. Testing

We tested our client and server implementation with different sizes of file, different probability of datagram loss and different sizes of sending and receiving buffers. The results were as expected and the design worked for all the cases.
