#ifndef	__unp_rtt_h
#define	__unp_rtt_h

#include	"unp.h"

struct rtt_info {
  uint64_t		rtt_rtt;	/* most recent measured RTT, in useconds */
  uint64_t		rtt_srtt;	/* smoothed RTT estimator, in useconds */
  uint64_t		rtt_rttvar;	/* smoothed mean deviation, in useconds */
  uint64_t		rtt_rto;	/* current RTO to use, in useconds */
  uint32_t		rtt_base;	/* # sec since 1/1/1970 at start */
};

#define	RTT_RXTMIN      1000000	// min retransmit timeout value, in useconds 
#define	RTT_RXTMAX     	3000000	// max retransmit timeout value, in useconds 
#define	RTT_MAXNREXMT 	12		// max # times to retransmit 

				/* function prototypes */
void	 rtt_debug(struct rtt_info *);
void	 rtt_init(struct rtt_info *);
void	 rtt_newpack(struct rtt_info *);
int		 rtt_start(struct rtt_info *);
void	 rtt_stop(struct rtt_info *, uint64_t);
void	 rtt_timeout(struct rtt_info *);
uint64_t rtt_ts(struct rtt_info *);

extern int	rtt_d_flag;	/* can be set to nonzero for addl info */

#endif	/* __unp_rtt_h */
