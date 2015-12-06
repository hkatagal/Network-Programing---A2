/* This file contains functions that are called to transfer the 
requested file to the client*/ 

#include	"unp.h"
#include	"unprtt_mod.h"
#include	<setjmp.h>
#include 	"serversupport.h"

// Utility function to find minimum of three numbers
int minimum(int a, int b, int c){
	int temp = a;
    if (temp > b) temp = b;
    if (temp > c) temp = c;
    return temp;
}

static struct rtt_info   rttinfo;
static int	rttinit = 0;
static void	sig_alrm(int signo);
static sigjmp_buf	jmpbuf;
int timer_running = 0;

// Starts the retransmission timer
void set_timer(int timeval){
	alarm(timeval);
	timer_running = 1;
}

// Clears the retransmission timer
void clear_timer(){
	alarm(0);
	timer_running = 0;
}

static struct msghdr	msgsend, msgrecv;	


struct iovec	iovsend[2], iovrecv[2];


struct list_packets *phead = NULL;

int DATASIZE = 512-sizeof(struct hdr);



/* Function that reads from the requested file and sends the datagrams to 
			the client in a reliable manner*/
int udp_start_send(int sockfd, char* filename, uint32_t rwnd, const SA * pcliaddr, socklen_t len, int sendwindow){
	FILE *fp;
	char buffer[BLOCKSIZE];
	int n;
	parameters_init(rwnd,sendwindow);
	if ((fp = fopen(filename,"r")) == NULL) {
	  printf("Error: cannot open file %s for FTP.\n",filename);
	  udp_stop(sockfd,pcliaddr,sizeof(pcliaddr));
	  exit(0);
	}
	printf("\nStarting to send file to client using FTP.\n\n");
	while( (n=read(fileno(fp),buffer,BLOCKSIZE)) > 0){
		dg_send_recv_data(sockfd,buffer,pcliaddr,len);
		bzero(buffer,BLOCKSIZE);
	}
	fclose(fp);
}

/*Function that terminates the connection and exits cleanly upon successful 
			data transfer between the server process and the client*/
void udp_stop(int sockfd, const SA * pcliaddr, socklen_t len ){
	char dataoutbuff[MAXLINE], inbuff[MAXLINE];
	fd_set rset;
	int maxfdp1;
	struct timeval timer;
	int ready;
	timer.tv_sec = 3;
	timer.tv_usec = 0;
	int rxt_count = 0;
	int temp_seq = parameter.NextSeqNum;
	parameter.NextSeqNum++;
	printf("\nFile transfer completed. Sending FIN ACK\n");
	
	sendagain:
	bzero(dataoutbuff,strlen(dataoutbuff));
	strcpy(dataoutbuff,"$$");
	msgsend.msg_name = NULL;
	msgsend.msg_namelen = 0;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	sendhdr.seq = temp_seq;		
	sendhdr.ts = rtt_ts(&rttinfo);
	sendhdr.flag = 2;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = dataoutbuff;
	iovsend[1].iov_len = strlen(dataoutbuff);
	
	Sendmsg(sockfd, &msgsend, 0);
	
	FD_ZERO(&rset);	
	for(;;){
		FD_SET(sockfd, &rset);
		maxfdp1 = sockfd + 1;
		if( (ready = select(maxfdp1, &rset, NULL, NULL, &timer)) < 0){
			if(errno == EINTR)
				continue;
			else
				err_sys("Select Error");
		}
		if(FD_ISSET(sockfd, &rset)){
			bzero(inbuff,MAXLINE);
			msgrecv.msg_name = NULL;
			msgrecv.msg_namelen = 0;
			msgrecv.msg_iov = iovrecv;
			msgrecv.msg_iovlen = 2;
			iovrecv[0].iov_base = &recvhdr;
			iovrecv[0].iov_len = sizeof(struct hdr);
			iovrecv[1].iov_base = inbuff;
			iovrecv[1].iov_len = MAXLINE;
			Recvmsg(sockfd, &msgrecv, 0);
			if(recvhdr.flag == 2){
				printf("\nFin ACK Received, Exiting\n");
				exit(0);
			}
			else{
				printf("\nTimeout: Sending FIN ACK again.\n");
				rxt_count++;
				if(rxt_count > 12){
					printf("\nNo response from client, Exiting\n");
					
					exit(0);					
				}					
				else
					goto sendagain;
			}
		}
		rxt_count++;
		if(rxt_count > 12){
			printf("\nNo response from client, Exiting\n");
			exit(0);
		}			
		else
			goto sendagain;
	}	
}

/*
* Initializing the parameters
*/
void parameters_init(uint32_t rwnd, uint32_t sendwin){
	parameter.rwnd = rwnd;
	parameter.NextSeqNum = 1;
	parameter.SendBase = 1;
	parameter.EndWindow = 1;
	parameter.cwnd = 1;
	parameter.ssthresh = SSTHRES_INITIAL;	
	parameter.current_state = 0; // Slow start
	parameter.dup_ack_count = 0;
	parameter.sendwindow = sendwin;
	printf("\nInitializing Parameters, \nRWND = %d, \nCWND = %d, \nSSTHRESHOLD = %d\n",
					parameter.rwnd,parameter.cwnd,parameter.ssthresh);
}

/* Function used to buffer the datagrams at the sender */
void add_to_list(int seq,char *buff, int len, uint32_t rxt){
	struct list_packets * node = (struct list_packets *) malloc(sizeof(struct list_packets));
	node->seq_no = seq;
	bzero(node->data,strlen(node->data));
	memcpy(node->data, buff, len);
	node->rxt_no = rxt;
	node->next = NULL;
	if(phead == NULL){
		phead = node;
		return;		
	}
	else{
		struct list_packets *temp = phead;
		while(temp->next != NULL)
			temp = temp->next;
		temp->next = node;
	}
}

void print_list(){
	struct list_packets *temp = phead;
	while(temp!=NULL){
		printf("\nseq no = %d\n",temp->seq_no);
		printf("\nrxt no = %d\n",temp->rxt_no);
		printf("\ndata = %s\n\n",temp->data);
		temp = temp->next;
	}
}

int list_count(){
	struct list_packets *temp = phead;
	int count = 0;
	while(temp!=NULL){
		count++;		
		temp = temp->next;
	}
	return count;
}

void delete_list(int end){
	if(phead == NULL){
		printf("\nNo packets in the send buffer to remove\n");
		return;
	}
	else if(phead->seq_no <= end){
		struct list_packets *temp_head = phead;
		struct list_packets *prev = phead;
		while(temp_head!=NULL && temp_head->seq_no <= end){
			prev = temp_head;
			temp_head = temp_head->next;
			free(prev);			
		}
		phead = temp_head;
		printf("\nMoving the sliding window due to the reception of valid ACK\n");	
	}
	else
		printf("\nCannot move the sliding window till a valid ACK is recieved\n");
}

/* Function used to retransmit the datagrams to the client in case of retransmission timeout
			or during fast retransmission */
void retransmit(int sockfd){
	
	char outbuff[MAXLINE];
	struct list_packets* temp  = phead;
	msgsend.msg_name = NULL;
	msgsend.msg_namelen = 0;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	sendhdr.seq = temp->seq_no;		
	sendhdr.ts = rtt_ts(&rttinfo);
	sendhdr.flag = 1;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	bzero(outbuff,MAXLINE);
	memcpy(outbuff,temp->data,strlen(temp->data));
	iovsend[1].iov_base = outbuff;
	iovsend[1].iov_len = strlen(outbuff);
	
	Sendmsg(sockfd, &msgsend, 0);
	printf("\nRetransmitting the packet with sequence number: %d\n",sendhdr.seq);
	set_timer(rtt_start(&rttinfo));
	
}

/* Function that does the reliable data transfer between the server proces and the client
		using ARQ mechanisms, Flow control and Congestion control */
		
void dg_send_recv_data(int sockfd, char* buffer,const SA * destaddr, socklen_t destlen){
	if (rttinit == 0) {
		rtt_init(&rttinfo);		/* Initializing for the first time when called */
		rttinit = 1;
		rtt_d_flag = 1;
	}
	int end_index = 0;
	char outbuff[512];
	char inbuff[512];
	char temp_buff[512];
	size_t inbytes = MAXLINE;
	int sendbase;
	fd_set rset;
	int maxfdp1;
	int select_val;
	struct timeval time;
	
	Signal(SIGALRM, sig_alrm);
	
	int i = 1;
	uint32_t temp_window;
	int buff_size = strlen(buffer);
	int temp = buff_size%(DATASIZE);
	int no_packets = buff_size/(DATASIZE);
	if(temp>0)
		no_packets++;
	
	
	while(no_packets > 0 || parameter.SendBase < parameter.NextSeqNum){
		i = 1;
		int condition = minimum(parameter.rwnd, parameter.cwnd, no_packets);
		condition = (condition < parameter.sendwindow) ? condition : parameter.sendwindow;
		printf("\n*----------------------------------------------------------\n\n");
		printf("\nRWND = %d, CWND = %d, Number of Packets to send = %d\n",
							parameter.rwnd,parameter.cwnd,no_packets);
		printf("\nTaking the minimum of these values (%d) as the size of the sliding window\n\n",condition);
		temp_window = parameter.SendBase + condition - 1;
		if(temp_window > parameter.EndWindow)
			parameter.EndWindow = temp_window;
		sendbase = parameter.SendBase;
		
		if(parameter.NextSeqNum > parameter.EndWindow){
			printf("\nSender sliding window is full and is locked until valid ACK arrives and the window slides\n");
		}
		
		/* Sending the datagrams in the sender sliding window */
		while(parameter.NextSeqNum <= parameter.EndWindow){
			no_packets--;
			int size_of_data = (buff_size<DATASIZE)?buff_size:DATASIZE;
			buff_size -=size_of_data;
			
			msgsend.msg_name = NULL;
			msgsend.msg_namelen = 0;
			msgsend.msg_iov = iovsend;
			msgsend.msg_iovlen = 2;
			sendhdr.seq = parameter.NextSeqNum;
			sendhdr.ts = rtt_ts(&rttinfo);
			parameter.NextSeqNum++;	
			sendhdr.flag = 1;
			sendhdr.rxt_no = 0;
			iovsend[0].iov_base = &sendhdr;
			iovsend[0].iov_len = sizeof(struct hdr);
			bzero(outbuff,512);
			memcpy(outbuff,buffer+end_index,size_of_data);
			end_index += size_of_data;
			iovsend[1].iov_base = outbuff;
			iovsend[1].iov_len = strlen(outbuff);

			Sendmsg(sockfd, &msgsend, 0);
			if(!timer_running)					// if timer currently not running, start timer
				set_timer(rtt_start(&rttinfo));				
				
			printf("\n\nSending the datagram with seq no = %d, with size of data = %d\n\n",
							sendhdr.seq,iovsend[1].iov_len);
			
			add_to_list(sendhdr.seq, outbuff, strlen(outbuff), sendhdr.rxt_no);
			printf("\nBuffering the sent datagram until a valid ACK is received\n\n");

			bzero(temp_buff,512);
			memcpy(temp_buff,outbuff,strlen(outbuff));
					
		}
		
		/* Check for time out */		
		if (sigsetjmp(jmpbuf, 1) != 0) {
			printf("\n**********************TIMEOUT*********************\n\n");
			rtt_timeout(&rttinfo);					// Double the RTO value
			phead->rxt_no++;
			if(phead->rxt_no > RTT_MAXNREXMT){
				err_msg("\nNo response from client, giving up\n\n");
				printf("\nNo response from client, giving up\n\n");
				rttinit = 0;						
				errno = ETIMEDOUT;
				udp_stop(sockfd,destaddr,sizeof(destlen));
				exit(0);
			}
			else{			
				printf("\nTimeout, retransmitting\n\n");			
				/* Retransmit and start the timer */
				retransmit(sockfd);
			}
			if(parameter.current_state == 0){								// Slow Start
				printf("\nTimeOut: Current State is Slow Start\n\n");
				parameter.ssthresh = parameter.cwnd/2;
				if(parameter.ssthresh == 0){
					parameter.ssthresh = 1;
				}
				parameter.cwnd = 1;
				parameter.dup_ack_count = 0;
				parameter.current_state = 0;
				printf("\nTimeOut: Hence there is no change in the state\n\n");
			}
			else if(parameter.current_state == 1){							// Congestion Avoidanace
					printf("\nTimeOut: Current State is Congestion Avoidance\n\n");
					parameter.ssthresh = parameter.cwnd/2;
					if(parameter.ssthresh == 0){
						parameter.ssthresh = 1;
					}
					parameter.cwnd = 1;
					parameter.dup_ack_count = 0;
					parameter.current_state = 0;
					printf("\nTimeOut: Hence the state changes to Slow start\n\n");
			}
			else if(parameter.current_state == 2){							// Fast Recovery
				printf("\nTimeOut: Current state is Fast Recovery\n\n");
				parameter.ssthresh = parameter.cwnd/2;
				if(parameter.ssthresh == 0){
					parameter.ssthresh = 1;
				}
				parameter.cwnd = 1;
				parameter.dup_ack_count = 0;
				parameter.current_state = 0;
				printf("\nTimeOut: Hence the state changes to Slow start\n\n");
			}
			printf("\n********************************************************************\n\n");
			continue;				
		}

		/*Receive the ACK sent from the client, check if the RWND is zero and do the probing,
		else if its a valid ACK update the sliding window parameters so that it moves appropriately */
	
		msgrecv.msg_name = NULL;
		msgrecv.msg_namelen = 0;
		msgrecv.msg_iov = iovrecv;
		msgrecv.msg_iovlen = 2;
		iovrecv[0].iov_base = &recvhdr;
		iovrecv[0].iov_len = sizeof(struct hdr);
		iovrecv[1].iov_base = inbuff;
		iovrecv[1].iov_len = inbytes;
		Recvmsg(sockfd, &msgrecv, 0);
		printf("\nACK with sequence: %d received\n\n",recvhdr.ack);
		rtt_debug(&rttinfo);			// Prints the rtt, srtt, rttvar and rto values
		
		
		/* Check if rwnd is zero (persistent timer) */
		if(recvhdr.rwnd == 0){
			printf("\n*****************************RWND: 0 RECEIVED****************************\n");
			clear_timer();
			time.tv_sec = i;
			time.tv_usec = 0;
			FD_ZERO(&rset);
			for(;;){
				printf("\nACK with RWND = 0 received, hence the server process goes to sleep\n\n");
				FD_SET(sockfd, &rset);
				maxfdp1 = sockfd + 1;
				if( (select_val = select(maxfdp1, &rset, NULL, NULL, &time)) < 0){
					if(errno == EINTR)
						continue;
					else
						err_quit("Select Error");
				}
				else if(select_val == 0){			// no ack received
					i = i*2;
					if(i >= 60)
						i = 1;
					msgsend.msg_name = NULL;
					msgsend.msg_namelen = 0;
					msgsend.msg_iov = iovsend;
					msgsend.msg_iovlen = 2;
					sendhdr.seq = 0;
					sendhdr.ts = rtt_ts(&rttinfo);
					sendhdr.flag = 0;
					iovsend[0].iov_base = &sendhdr;
					iovsend[0].iov_len = sizeof(struct hdr);
					bzero(outbuff,512);
					memcpy(outbuff,"",1);
					iovsend[1].iov_base = outbuff;
					iovsend[1].iov_len = strlen(outbuff);
					Sendmsg(sockfd, &msgsend, 0);
					printf("\nSending a probe signal after waking up\n\n");
					continue;
				}
				else{
					msgrecv.msg_name = NULL;
					msgrecv.msg_namelen = 0;
					msgrecv.msg_iov = iovrecv;
					msgrecv.msg_iovlen = 2;
					iovrecv[0].iov_base = &recvhdr;
					iovrecv[0].iov_len = sizeof(struct hdr);
					iovrecv[1].iov_base = inbuff;
					iovrecv[1].iov_len = inbytes;
					Recvmsg(sockfd, &msgrecv, 0);
					
					if(recvhdr.rwnd != 0){
						printf("\nFinished probing, got an ACK with rwnd size = %d\n", recvhdr.rwnd);
						printf("\n************************PROBE FINISHED**********************\n\n");
						break;		
						
					}
								
				}
			}
			set_timer(rtt_start(&rttinfo));	
		}
		
		if(recvhdr.ack > parameter.SendBase){
			
			printf("\nGot a valid ACK, hence updating the sliding window parameters\n\n");
			rtt_stop(&rttinfo, rtt_ts(&rttinfo) - recvhdr.ts);
			parameter.SendBase = recvhdr.ack;
			parameter.rwnd = recvhdr.rwnd;
			delete_list(parameter.SendBase-1);
			if(list_count() > 0){			// not yet ACK datagrams, so start timer
				printf("\nThere are packets in the send buffer that are not yet acknowledged, hence starting the timer\n\n");
				set_timer(rtt_start(&rttinfo));				
			}
			else{
				printf("\nAll the packets in the send buffer have been acknowledged, stopping timer\n\n");	
				clear_timer();
			}
		}
		
		/* Congestion Control Implementation
			state 0: Slow Start
			state 1: Congestion Avoidance
			state 2: Fast Recovery
		*/
		printf("\n***************************CONGESTION CONTROL**********************\n\n");
		switch(parameter.current_state){
			case 0:										// Slow Start
				printf("\nCurrent State is Slow start, CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
				if(recvhdr.ack > sendbase){				// new ack
					printf("\nACK is valid.\n\n");
					parameter.cwnd++;
					parameter.dup_ack_count = 0;
					parameter.current_state = 0;
					//printf("Next State is also Slow start, CWND = %d, SSTHRESHOLD = %d\n",parameter.cwnd,parameter.ssthresh);
				}
				if(recvhdr.ack == sendbase){			// dup ack
					printf("\nDUP ACK received\n\n");
					parameter.dup_ack_count++;
					parameter.current_state = 0;
				//	printf("Next state is Slow start duplicate ack\n");
				}
				if(parameter.cwnd >= parameter.ssthresh){
					parameter.current_state = 1; 
					printf("\nNext state is Congestion Avoidance,CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
				}
				if(parameter.dup_ack_count == 3){
					printf("\n3 DUP ACKs received\n\n");
					parameter.ssthresh = parameter.cwnd/2;
					if(parameter.ssthresh == 0){
						parameter.ssthresh = 1;
					}
					parameter.cwnd = parameter.ssthresh + 3;
					retransmit(sockfd);	
					parameter.current_state = 2;
					printf("\nNext state is Fast Recovery,CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
				}
				break;
				
			case 1:										// Congestion Avoidance
				printf("\nCurrent State is Congestion Avoidance,CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
				if(recvhdr.ack > sendbase){				// new ack
					printf("\nNew ACK received\n");
					parameter.cwnd = parameter.cwnd + (1/parameter.cwnd);
					parameter.dup_ack_count = 0;
					parameter.current_state = 1;
					//printf("Next state is Congestion Avoidance\n");
				}
				if(recvhdr.ack == sendbase){			// dup ack
					printf("\nDUP ACK received\n\n");
					parameter.dup_ack_count++;
					parameter.current_state = 1;
					//printf("Next state is Congestion Avoidance\n");
				}
				if(parameter.dup_ack_count == 3){
					printf("\n3 DUP ACKs received\n\n");
					parameter.ssthresh = parameter.cwnd/2;
					if(parameter.ssthresh == 0){
						parameter.ssthresh = 1;
					}
					parameter.cwnd = parameter.ssthresh + 3;
					retransmit(sockfd);	
					parameter.current_state = 2;
					printf("\nNext state is Fast Recovery,CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
					
				}
				break;
				
			case 2:										// Fast Recovery
				printf("\nCurrent State is Fast Recovery,CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
				if(recvhdr.ack > sendbase){				// new ack
					printf("\nNew ACK received\n\n");
					parameter.cwnd = parameter.ssthresh;
					parameter.dup_ack_count = 0;
					parameter.current_state = 1;
					printf("\nNext state is Congestion Avoidance,CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
				}
				else if(recvhdr.ack == sendbase){		// dup ack
					printf("\nDUP ACK received\n\n");
					parameter.cwnd++;
					parameter.current_state = 2;
					printf("\nNext state is also Fast Recovery,CWND = %d, SSTHRESHOLD = %d\n\n",parameter.cwnd,parameter.ssthresh);
				}
				break;				
		}
		printf("\n*****************************************************************\n\n");
	}
	
}

/* Signal Handler for SIGALRM */
static void sig_alrm(int signo)
{
	siglongjmp(jmpbuf, 1);
}
