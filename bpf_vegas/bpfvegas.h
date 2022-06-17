#ifndef __BPFVEGAS_H__
#define __BPFVEGAS_H__
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
/* Vegas variables */
struct vegas {
	u32	beg_snd_nxt;	/* right edge during last RTT */
	u32	beg_snd_una;	/* left edge  during last RTT */
	u32	beg_snd_cwnd;	/* saves the size of the cwnd */
	u8	doing_vegas_now;/* if true, do vegas for this RTT */
	u16	cntRTT;		/* # of RTTs measured within last RTT */
	u32	minRTT;		/* min of RTTs measured within last RTT (in usec) */
	u32	baseRTT;	/* the min of all Vegas RTT measurements seen (in usec) */
};
int bpf_vegas_init();
void bpf_vegas_stop(void);
void unload_bpf_vegas(void);
void load_bpf_vegas(void);
#endif //__BPFVEGAS_H__