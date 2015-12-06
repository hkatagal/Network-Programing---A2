#ifndef SERVER_H_
#define SERVER_H_

#include	"unprtt_mod.h"
#include "unp.h"
#include <math.h>

#define	RTT_DEBUG
#define BLOCKSIZE 2048*2048
#define SSTHRES_INITIAL (65535/512)



static struct hdr {
  uint32_t	seq;	/* sequence # */
  uint64_t	ts;		/* timestamp when sent */
  uint32_t	ack;
  uint32_t	rwnd;
  uint32_t rxt_no;
  uint16_t flag;
} sendhdr, recvhdr;

struct list_packets{
	int seq_no;
	char data[512];
	uint32_t rxt_no;
	struct list_packets *next;
};

static struct parameters{
	uint32_t NextSeqNum;
	//uint32_t InitialSeqNum;
	uint32_t SendBase;
	uint32_t EndWindow;
	uint32_t rwnd;
	uint32_t cwnd;
	uint32_t ssthresh;
	int current_state;
	int dup_ack_count;
	uint32_t sendwindow;
}parameter;


int minimum(int a, int b, int c);

static void	sig_alrm(int signo);

void dg_send_recv_data(int sockfd, char* buffer,const SA * pcliaddr, socklen_t len);

int udp_start_send(int sockfd, char* filename, uint32_t rwnd, const SA * pcliaddr, socklen_t len, int sendwindow);
void udp_stop(int sockfd, const SA * pcliaddr, socklen_t len );

void add_to_list(int seq,char *buff, int len, uint32_t rxt);

void print_list();

int list_count();

void delete_list(int end);

void retransmit(int sockfd);

void dg_send_recv_data(int sockfd, char* buffer,const SA * destaddr, socklen_t destlen);

void parameters_init(uint32_t rwnd, uint32_t sendwin);

void set_timer(int timeval);

void clear_timer();




#endif
