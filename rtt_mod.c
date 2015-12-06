#include	"unprtt_mod.h"

int		rtt_d_flag = 0;		


#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4 * (ptr)->rtt_rttvar))

static uint64_t rtt_minmax(uint64_t rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void rtt_init(struct rtt_info *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec;		

	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 750000;    
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
	//ptr->rtt_rto = 1;
		/* first RTO at (srtt + (4 * rttvar)) = 3 second */
}

uint64_t rtt_ts(struct rtt_info *ptr)
{
	uint64_t		ts;
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec - ptr->rtt_base) * 1000000) + tv.tv_usec;
	return(ts);
}

int rtt_start(struct rtt_info *ptr)
{
	int seconds = ptr->rtt_rto/1000000;
	return(seconds);		/* return int(seconds) */
							/* return value can be used as: alarm(rtt_start(&foo)) */
}


void rtt_stop(struct rtt_info *ptr, uint64_t us)
{

	ptr->rtt_rtt = us;		/* measured RTT in millisecond */
	
	int64_t delta = us;

	//Update our estimators of RTT and mean deviation of RTT.
	
	delta -= (ptr->rtt_srtt >> 3);
	ptr->rtt_srtt += delta;		

	if (delta < 0)
		delta = -delta;				
	
	delta -= (ptr->rtt_rttvar >> 2);

	ptr->rtt_rttvar += delta;	
	
	ptr->rtt_rto = (ptr->rtt_srtt >> 3) + ptr->rtt_rttvar;

	ptr->rtt_rto = rtt_minmax(ptr->rtt_rto);
	
	
}

void rtt_timeout(struct rtt_info *ptr)
{
	ptr->rtt_rto *= 2;		/* next RTO */
	
	ptr->rtt_rto = rtt_minmax(ptr->rtt_rto);
}

void rtt_debug(struct rtt_info *ptr)
{
	if (rtt_d_flag == 0)
		return;

	fprintf(stdout, "rtt = %lldu, srtt = %lldu, rttvar = %lldu, rto = %lldu\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stdout);
}
