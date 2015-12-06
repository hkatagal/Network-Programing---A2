/*
* Authors: Harishkumar Katagal(109915793) & Gagan Nagaraju (109889036)
* FileName: clientsupport.c
* 
*/

/* This is used by the client to receive data and send ACK.*/
#include	"unprtt_mod.h"
#include	<setjmp.h>
#include 	<string.h>
#include	<time.h>
#include "pthread.h"
#include "clientsupport.h"
#include <math.h>

static struct msghdr	msgsend, msgrecv;

struct list_packets *phead = NULL;

char fileread[7][50];

pthread_mutex_t lock;


/*
* Receiver side main funtion which handles all the operations.
*/

void client_receive_data(int sockfd){
	
	/*create two threads
	call one on client_producer and another on client_receive. join them and exit;
	*/
	pthread_t producer_thread;
	pthread_t consumer_thread;
	
	setparameters();
	Pthread_create(&producer_thread,NULL,&client_producer,(void *)sockfd);
	Pthread_create(&consumer_thread,NULL,&client_consumer,(void *)sockfd);
	
	Pthread_join(producer_thread,NULL);
	Pthread_join(consumer_thread,NULL);
	//printDataReceived();
	
	return ;	
}

/* 
*Consumer thread function. This function reads data from the receive buffer and puts it onto a *file. 
*/

void client_consumer( void * argc){
	FILE *fw;
	int sleep_time;
	char cli_buff[512];	
	
	if ((fw = fopen("output.txt","w")) == NULL) {
	  printf("Error: cannot open file output.txt for writing data.\n\n");
	  exit(0);
	}
	while(1){
		sleep_time = consumer_sleep_time();
		usleep(sleep_time);
		Pthread_mutex_lock(&lock);
		/*
		* Read the data from the buffer;
		*/
		
		printf("*****************Consumer locked mutex******************\n\n");
		if(phead == NULL){
			printf("Consumer Thread: No data in buffer for consumer to read.\n\n");
		}
		else{
			struct list_packets *temp_head = phead;
			struct list_packets *prev = phead;
			if(temp_head!=NULL && temp_head->seq_no != parameter.next_seq_no_to_read){
				printf("Data in buffer not in order for read.\n");
				printf("Expected %d sequence number. But found %d in buffer.\n\n",parameter.next_seq_no_to_read,temp_head->seq_no);
			}
			while(temp_head!=NULL && temp_head->seq_no == parameter.next_seq_no_to_read){
				struct list_packets *current = temp_head;
				temp_head = temp_head->next;
				bzero(cli_buff,512);
				memcpy(cli_buff,current->data,strlen(current->data));
				
				fputs(cli_buff,fw);
				parameter.next_seq_no_to_read++;
				parameter.packet_count--;
				if(parameter.packet_count<0){
					parameter.packet_count = 0;
				}
				printf("Consumer consumed data from %d seq\n\n",current->seq_no);
				printf("\n*****************Data in %d sequence**************\n\n",current->seq_no);
				printf("\n%s\n\n",cli_buff);
				printf("\n********************End of Data*******************\n\n");
				free(current);
			}
			phead = temp_head;
			
		}		
		if((parameter.finACK == 1 && phead == NULL) || (parameter.finACK == 1 && phead->seq_no != parameter.next_seq_no_to_read)){
			Pthread_mutex_unlock(&lock);
			printf("****************Consumer released mutex*****************\n\n");
			printf("Fin ACK received and all the data is read from buffer.\n\n");
			printf("Exiting from consumer thread.\n\n");
			fclose(fw);
			return;
		}
		
		Pthread_mutex_unlock(&lock);
		printf("****************Consumer released mutex*****************\n\n");
		
	}
	
	fclose(fw);
}

/* 
*Producer thread function. This function reads the incoming packet and puts it in the receive *buffer 
*/

void client_producer( void * argc){
	ssize_t			n;
	struct iovec	iovrecv[2];
	char			inbuff[512];
	int sockfd = (int)argc;
	while(1){
		/*
		* Read the incoming packet.
		*/
		bzero(inbuff,512);
		msgrecv.msg_name = NULL;
		msgrecv.msg_namelen = 0;
		msgrecv.msg_iov = iovrecv;
		msgrecv.msg_iovlen = 2;
		iovrecv[0].iov_base = &recvhdr;
		iovrecv[0].iov_len = sizeof(struct hdr);
		iovrecv[1].iov_base = inbuff;
		iovrecv[1].iov_len = 512;
		n = recvmsg(sockfd, &msgrecv, 0);
		/*If socket is closed at the server side.*/
		if(n<=0 && errno == ECONNREFUSED){
			printf("Producer Thread: Server connection terminated.\n");
			printf("Closing producer thread.\n\n");
			parameter.finACK = 1;
			return;
		}
		/* FIN ACK flag is set in the received packet. */
		if(recvhdr.flag == 2){
			printf("Producer Thread: FIN received from server.\n");
			printf("Closing producer thread.\n\n");
			parameter.finACK = 1;
			sendACK(sockfd,parameter.next_expected_packet,2,recvhdr.ts);
			return;
		}
		
		/*Persistent probe packet.*/
		if(recvhdr.flag == 0){
			printf("Producer Thread: Persistent probe packet received.\n");
			printf("Producer Thread: Sending probe ack with receiver buffer size of : %d\n\n",parameter.rwnd - parameter.packet_count);
			sendACK(sockfd,parameter.next_expected_packet,0,recvhdr.ts);
		}
		
		
		if(check_drop_packet()==1){
			printf("Prodcuer Thread: Probability is less than drop probability\n");
			printf("Producer Thread: Dropping the packet with sequence: %d\n\n",recvhdr.seq);
			sendACK(sockfd,parameter.next_expected_packet,0,recvhdr.ts);
			continue;
		}
		
		
		/*Data packet*/
		if(recvhdr.flag==1){
			if(parameter.packet_count == parameter.rwnd){
				printf("Producer Thread: Receive buffer full. Dropping packet with %d sequence.\n\n", recvhdr.seq);
				sendACK(sockfd,parameter.next_expected_packet,1,recvhdr.ts);
			}
			else{
				Pthread_mutex_lock(&lock);
				printf("****************Producer locked mutex*******************\n");
				if(recvhdr.seq >= parameter.next_expected_packet){
					add_to_list(recvhdr.seq,inbuff,strlen(inbuff));
				}
				printf("****************Producer released mutex*****************\n\n");
				Pthread_mutex_unlock(&lock);
				if(parameter.next_expected_packet == recvhdr.seq){
					parameter.next_expected_packet = getNextSeqNumber(parameter.next_expected_packet);
				}
				sendACK(sockfd,parameter.next_expected_packet,1,recvhdr.ts);
			}
		}
	}
}



/* 
*This function sends the ack.
*/

void sendACK(int sockfd, int seq, int flag, uint64_t timestamp){
	char outbuffer[512];
	bzero(outbuffer,512);
	struct iovec	iovsend[2];
	if(flag == 0){
		strcpy(outbuffer,"PROBE");
	}
	else if(flag==1){
		strcpy(outbuffer,"ACK");
		printf("\nSending ACK for %d sequence.\n\n",seq);
	}
	else{
		strcpy(outbuffer,"FIN");
		printf("\n Sending FIN ACK.\n\n");
	}
	
	sendhdr.ack  = seq;
	sendhdr.flag = flag;
	sendhdr.ts = timestamp;
	sendhdr.rwnd = parameter.rwnd - parameter.packet_count;
	msgsend.msg_name = NULL;
	msgsend.msg_namelen = 0;
	msgsend.msg_iov = iovsend;
	msgsend.msg_iovlen = 2;
	iovsend[0].iov_base = &sendhdr;
	iovsend[0].iov_len = sizeof(struct hdr);
	iovsend[1].iov_base = outbuffer;
	iovsend[1].iov_len = strlen(outbuffer);
	Sendmsg(sockfd, &msgsend, 0);
	
}

/*
* Helper Functions
*/

/* 
*This function returns the the next sequence number to be acknowledged.
*/
int getNextSeqNumber(int seq){
	struct list_packets *temp_head = phead;
	if(phead == NULL){
		return (seq+1);
	}
	while(temp_head!=NULL && temp_head->seq_no < seq){
		temp_head = temp_head->next;
	}
	if(temp_head == NULL){
		return (seq+1);
	}
	while(temp_head != NULL && temp_head->seq_no == seq){
		seq = seq+1;
		temp_head = temp_head->next;
	}
	return seq;
}


/*
* To initialise all the parameters at the beginning.
* Reads from client.in file and updates the structure with the parameters.
*/
void setparameters(){
	FILE *fp;
	int i;
	int length;
	char line[50];
	if ((fp = fopen("client.in","r")) == NULL) {
	  printf("Error: cannot open file server.in for reading port number.\n");
	  exit(0);
	}
	for(i=0;i<7;i++){
		if(fgets(line,50,(FILE*)fp)!=NULL){
			length = strlen(line);
			strncpy(fileread[i],line,length);
			if(fileread[i][length-1]=='\n'){
				fileread[i][length-1] = '\0';
			}
		}
	}
	/* Receiving Sliding Window */
	parameter.rwnd = atoi(fileread[3]);
	if(parameter.rwnd <= 0){
		printf("\nInvalid receive window specified in \"client.in\" file.");
		exit(0);
	}
	/* Seed */
	parameter.seed = atoi(fileread[4]);
	if(parameter.seed == 0){
		printf("\nInvalid seed specified in \"client.in\" file.");
		exit(0);
	}
	/*Probability of datagram loss. */
	parameter.prob = atof(fileread[5]);
	if(parameter.prob < 0){
		printf("\nInvalid probability specified in \"client.in\" file.");
		exit(0);
	}
	/* Mean for exponetial distribution. */
	parameter.mean = atoi(fileread[6]);
	if(parameter.mean == 0){
		printf("\nInvalid mean specified in \"client.in\" file.");
		exit(0);
	}
	parameter.packet_count = 0;
	parameter.next_seq_no_to_read = 1;
	srand(parameter.seed);
	parameter.finACK = 0;
	parameter.last_seq_no = 0;
	parameter.next_expected_packet = 1;
}

/*
* Function to add data to the receive buffer.
* Adds a datagram data to the linked list.
*/

void add_to_list(int seq,char *buff, int len){
	
	struct list_packets * node;
	if(phead!=NULL && phead->seq_no == seq){
		printf("Producer Thread: Data for %d sequence already buffered. Hence ignoring packet.\n",seq);
		return;
	}
	node = (struct list_packets *) malloc(sizeof(struct list_packets));
	node->seq_no = seq;
	bzero(node->data,512);
	memcpy(node->data, buff, len);
	node->next = NULL;
	
	if(phead == NULL || phead->seq_no > seq){
		node->next = phead;
		phead = node;
		printf("Producer Thread: Data for %d sequence buffered. current: %d\n",seq,parameter.next_expected_packet);
		if(parameter.last_seq_no < seq)
			parameter.last_seq_no = seq;
	}
	else{
		
		struct list_packets *temp = phead;
		while(temp->next !=NULL && temp->next->seq_no < seq){
			temp = temp->next;
		}
	
		if(temp->next!=NULL && temp->next->seq_no == seq){
			printf("Producer Thread: Data for %d sequence already buffered. Hence ignoring packet.\n",seq);
			return;
		}
		node->next = temp->next;
		temp->next = node;
		printf("Producer Thread: Data for %d sequence buffered. current: %d\n",seq,parameter.next_expected_packet);
		if(parameter.last_seq_no < seq)
			parameter.last_seq_no = seq;
	}
	parameter.packet_count++;
}

/*
* Function to print the contents of receive buffer.
* Used to print the contents of the list. Is not called anywhere. Just for debugging.
*/
void print_list(){
	struct list_packets *temp = phead;
	while(temp!=NULL){
		printf("seq no = %d\n",temp->seq_no);
		printf("data = %s\n\n",temp->data);
		temp = temp->next;
	}
}
/*
* Returns the number of contents in the list.
*/
int list_count(){
	struct list_packets *temp = phead;
	int count = 0;
	while(temp!=NULL){
		count++;		
		temp = temp->next;
	}
	return count;
}


/*
* To check if an incoming packet must be dropped.
*  
*/

int check_drop_packet(){
	float rate;
	int retvalue;
	rate = (float) (rand()%1000)/1000;
	retvalue = rate < parameter.prob ? 1 : 0;
	return retvalue;	
}

/*
* Consumer thread sleep time calculation in milliseconds.
*/

int consumer_sleep_time(){
	float rate;
	int retvalue;
	int mean = parameter.mean;
	int logval;
	do{
		rate = (float) rand()/(float)RAND_MAX;
	}while(rate == 0);
	logval = log(rate);
	mean = mean *1000 * -1;
	retvalue = (int) (logval * mean);
	return retvalue;
}

/* 
* Function that prints the data that was received. 
*/
void printDataReceived(){
	char readdata[MAXLINE];
	FILE *fr;
	if ((fr = fopen("output.txt","r")) == NULL) {
	  printf("Error: cannot open file output.txt for writing to console.\n\n");
	  exit(0);
	}
	while (fscanf(fr, "%s", readdata)!=EOF){
        printf("%s",readdata);
	}
    fclose(fr);
}

