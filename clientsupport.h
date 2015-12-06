/*
* Authors: Harishkumar Katagal(109915793) & Gagan Nagaraju (109889036)
* FileName: clientsupport.h
* 
*/

#ifndef CLIENT_H_
#define CLIENT_H_

#include	"unprtt_mod.h"
#include "unp.h"
#include <math.h>

/* Header structure of the packet */
static struct hdr {
  uint32_t	seq;	/* sequence # of packet*/
  uint64_t	ts;		/* timestamp when sent */
  uint32_t	ack;    /* acknowledge # */
  uint32_t	rwnd;	/* receiver window buffer size*/
  uint32_t rxt_no;	/* retransmission count of packet*/
  uint16_t flag;	/* to identify type of packet. 0-Persistent Probe, 1-Data packet 2-FIN*/
}sendhdr, recvhdr;

/* Receiver buffer list */
struct list_packets{
	int seq_no;		/* sequence # of packets */
	char data[512];	/* data sent in the packet */
	struct list_packets *next; /* next packet pointer */
};

/* Parameters to be stored */
static struct parameters{
	uint32_t rwnd;	/* maximum receive window size */
	uint32_t seed;	/* seed for the random generator */
	float prob;		/* probability of datagram loss */
	uint32_t mean;	/* mean for an exponential distribution */
	uint32_t packet_count;	/* packet count of receive buffer */
	uint32_t next_seq_no_to_read; /* next sequence number to read */ 
	uint32_t next_expected_packet; /* next expected packet */
	uint32_t last_seq_no; /* last sequence number that was acknowledged */
	uint32_t finACK;	/*  fin ack flag */
}parameter;


/* To initialise all the parameters at the beginning. */
void setparameters();

/* Funtion that handles receiving data and buffering it. */
void client_receive_data(int sockfd);

/* Function that prints the data that was received. */
void printDataReceived();

/* Consumer thread function. This function reads data from the receive buffer and puts it onto a file. */
void client_consumer();


/* Producer thread function. This function reads the incoming packet and puts it in the receive buffer */
void client_producer( void * argc);

/* This function returns the the next sequence number to be acknowledged */
int getNextSeqNumber(int seq);

/* This function sends the ack */
void sendACK(int sockfd, int seq, int flag, uint64_t timestamp);

/* Function to add data to the receive buffer */
void add_to_list(int seq,char *buff, int len);

/* Function to print the contents of receive buffer. */ 
void print_list();

/* Function to check if the packet should be dropped or not. */
int check_drop_packet();

/* Function to get the sleep time required for the consumer thread. */ 
int consumer_sleep_time();


#endif
