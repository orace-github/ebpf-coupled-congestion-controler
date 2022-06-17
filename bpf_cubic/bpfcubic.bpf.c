// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright (c) 2020 Facebook */

/* Adapted by Emery Assogba (assogba.emery@gmail.com) *
 * to fit libbpf+CORE and support global variables    */
 
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "bpfcubic.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

#define BPF_STRUCT_OPS(name, args...) \
SEC("struct_ops/"#name) \
BPF_PROG(name, args)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define min_not_zero(x, y) ({			\
	typeof(x) __x = (x);			\
	typeof(y) __y = (y);			\
	__x == 0 ? __y : ((__y == 0) ? __x : min(__x, __y)); })

static __always_inline __u32 tcp_slow_start(struct tcp_sock *tp, __u32 acked)
{
	__u32 cwnd = min(tp->snd_cwnd + acked, tp->snd_ssthresh);

	acked -= cwnd - tp->snd_cwnd;
	tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);

	return acked;
}

#define tcp_jiffies32 ((__u32)bpf_jiffies64())

#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)


#define BICTCP_BETA_SCALE    1024	/* Scale factor beta calculation
					 * max_cwnd = snd_cwnd * beta
					 */
#define	BICTCP_HZ		10	/* BIC HZ 2^10 = 1024 */

extern unsigned long CONFIG_HZ __kconfig;
#define HZ CONFIG_HZ
#define USEC_PER_MSEC	1000UL
#define USEC_PER_SEC	1000000UL
#define USEC_PER_JIFFY	(USEC_PER_SEC / HZ)

/* Two methods of hybrid slow start */
#define HYSTART_ACK_TRAIN	0x1
#define HYSTART_DELAY		0x2

/* Number of delay samples for detecting the increase of delay */
#define HYSTART_MIN_SAMPLES	8
#define HYSTART_DELAY_MIN	(4000U)	/* 4ms */
#define HYSTART_DELAY_MAX	(16000U)	/* 16 ms */
#define HYSTART_DELAY_THRESH(x)	clamp(x, HYSTART_DELAY_MIN, HYSTART_DELAY_MAX)


static int fast_convergence = 1;
//static const int beta = 717;	/* = 717/1024 (BICTCP_BETA_SCALE) */
int beta;
static int initial_ssthresh;
static const int bic_scale = 41;
static int tcp_friendliness = 1;

static int hystart = 1;
static int hystart_low_window = 16;
static int hystart_ack_delta_us = 2000;
static int hystart_detect = HYSTART_ACK_TRAIN | HYSTART_DELAY;

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} rb SEC(".maps");

static __always_inline __u64 div64_u64(__u64 dividend, __u64 divisor)
{
	return dividend / divisor;
}

#define div64_ul div64_u64

/* This code generates integer overflows. We have to fix it  */
#define BITS_PER_U64 (sizeof(__u64) * 8)
static __always_inline int fls64(__u64 x)
{
	int num = BITS_PER_U64 - 1;
	if (x == 0)
		return 0;

	if (!(x & (~0ull << (BITS_PER_U64-32)))) {
		num -= 32;
		x <<= 32;
	}
	if (!(x & (~0ull << (BITS_PER_U64-16)))) {
		num -= 16;
		x <<= 16;
	}
	if (!(x & (~0ull << (BITS_PER_U64-8)))) {
		num -= 8;
		x <<= 8;
	}
	if (!(x & (~0ull << (BITS_PER_U64-4)))) {
		num -= 4;
		x <<= 4;
	}
	if (!(x & (~0ull << (BITS_PER_U64-2)))) {
		num -= 2;
		x<<= 2;
	}
	//if (!(x & (~0ull << (BITS_PER_U64-1))))
	//	num -= 1;

	return num + 1;
}

static const __u32 cube_rtt_scale = (bic_scale * 10);	/* 1024*c/rtt */
__u32 beta_scale;  /*= 8*(BICTCP_BETA_SCALE+beta) / 3
				/ (BICTCP_BETA_SCALE - beta);*/
/* calculate the "K" for (wmax-cwnd) = c/rtt * K^3
 *  so K = cubic_root( (wmax-cwnd)*rtt/c )
 * the unit of K is bictcp_HZ=2^10, not HZ
 *
 *  c = bic_scale >> 10
 *  rtt = 100ms
 *
 * the following code has been designed and tested for
 * cwnd < 1 million packets
 * RTT < 100 seconds
 * HZ < 1,000,00  (corresponding to 10 nano-second)
 */

/* 1/c * 2^2*bictcp_HZ * srtt, 2^40 */
static const __u64 cube_factor = (__u64)(1ull << (10+3*BICTCP_HZ))
				/ (bic_scale * 10);

static __always_inline struct tcp_sock *tcp_sk(const struct sock *sk)
{
	return (struct tcp_sock *)sk;
}

static __always_inline __u32 bictcp_clock_us(const struct sock *sk)
{
	return tcp_sk(sk)->tcp_mstamp;
}

#define GSO_MAX_SIZE		65536

static __always_inline struct inet_connection_sock *inet_csk(const struct sock *sk)
{
	return (struct inet_connection_sock *)sk;
}

static __always_inline void *inet_csk_ca(const struct sock *sk)
{
	return (void *)inet_csk(sk)->icsk_ca_priv;
}

static __always_inline bool tcp_in_slow_start(const struct tcp_sock *tp)
{
	return tp->snd_cwnd < tp->snd_ssthresh;
}


static __always_inline bool before(__u32 seq1, __u32 seq2)
{
	return (__s32)(seq1-seq2) < 0;
}
#define after(seq2, seq1) 	before(seq1, seq2)

static inline void bictcp_reset(struct bictcp *ca)
{
	ca->cnt = 0;
	ca->last_max_cwnd = 0;
	ca->last_cwnd = 0;
	ca->last_time = 0;
	ca->bic_origin_point = 0;
	ca->bic_K = 0;
	ca->delay_min = 0;
	ca->epoch_start = 0;
	ca->ack_cnt = 0;
	ca->tcp_cwnd = 0;
	ca->found = 0;
}

static __always_inline bool tcp_is_cwnd_limited(const struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);

	/* If in slow start, ensure cwnd grows to twice what was ACKed. */
	if (tcp_in_slow_start(tp))
		return tp->snd_cwnd < 2 * tp->max_packets_out;

	return !!BPF_CORE_READ_BITFIELD(tp, is_cwnd_limited);
}

static __always_inline void tcp_cong_avoid_ai(struct tcp_sock *tp, __u32 w, __u32 acked)
{
	/* If credits accumulated at a higher w, apply them gently now. */
	if (tp->snd_cwnd_cnt >= w) {
		tp->snd_cwnd_cnt = 0;
		tp->snd_cwnd++;
	}

	tp->snd_cwnd_cnt += acked;
	if (tp->snd_cwnd_cnt >= w) {
		__u32 delta = tp->snd_cwnd_cnt / w;

		tp->snd_cwnd_cnt -= delta * w;
		tp->snd_cwnd += delta;
	}
	tp->snd_cwnd = min(tp->snd_cwnd, tp->snd_cwnd_clamp);
}

static __always_inline void bictcp_hystart_reset(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);

	ca->round_start = ca->last_ack = bictcp_clock_us(sk);
	ca->end_seq = tp->snd_nxt;
	ca->curr_rtt = ~0U;
	ca->sample_cnt = 0;
}

static __always_inline void bictcp_to_event(struct event* e, struct bictcp* ca){
	BPF_CORE_READ_INTO(&e->bictcp.ack_cnt, ca , ack_cnt);
	BPF_CORE_READ_INTO(&e->bictcp.cnt, ca , cnt);
	BPF_CORE_READ_INTO(&e->bictcp.last_cwnd, ca , last_cwnd);
	BPF_CORE_READ_INTO(&e->bictcp.last_time, ca , last_time);
	BPF_CORE_READ_INTO(&e->bictcp.bic_origin_point, ca , bic_origin_point);
	BPF_CORE_READ_INTO(&e->bictcp.bic_K, ca , bic_K);
	BPF_CORE_READ_INTO(&e->bictcp.delay_min, ca , delay_min);
	BPF_CORE_READ_INTO(&e->bictcp.epoch_start, ca , epoch_start);
	BPF_CORE_READ_INTO(&e->bictcp.tcp_cwnd, ca , tcp_cwnd);
	BPF_CORE_READ_INTO(&e->bictcp.sample_cnt, ca , sample_cnt);
	BPF_CORE_READ_INTO(&e->bictcp.round_start, ca , round_start);
	BPF_CORE_READ_INTO(&e->bictcp.end_seq, ca , end_seq);
	BPF_CORE_READ_INTO(&e->bictcp.last_ack, ca , last_ack);
	BPF_CORE_READ_INTO(&e->bictcp.curr_rtt, ca , curr_rtt);
}
/*
 * cbrt(x) MSB values for x MSB values in [0..63].
 * Precomputed then refined by hand - Willy Tarreau
 *
 * For x in [0..63],
 *   v = cbrt(x << 18) - 1
 *   cbrt(x) = (v[x] + 10) >> 6
 */
static const __u8 v[] = {
	/* 0x00 */    0,   54,   54,   54,  118,  118,  118,  118,
	/* 0x08 */  123,  129,  134,  138,  143,  147,  151,  156,
	/* 0x10 */  157,  161,  164,  168,  170,  173,  176,  179,
	/* 0x18 */  181,  185,  187,  190,  192,  194,  197,  199,
	/* 0x20 */  200,  202,  204,  206,  209,  211,  213,  215,
	/* 0x28 */  217,  219,  221,  222,  224,  225,  227,  229,
	/* 0x30 */  231,  232,  234,  236,  237,  239,  240,  242,
	/* 0x38 */  244,  245,  246,  248,  250,  251,  252,  254,
};

/* calculate the cubic root of x using a table lookup followed by one
 * Newton-Raphson iteration.
 * Avg err ~= 0.195%
 */
static __always_inline __u32 cubic_root(__u64 a)
{
	__u32 x, b, shift;

	if (a < 64) {
		/* a in [0..63] */
		return ((__u32)v[(__u32)a] + 35) >> 6;
	}

	b = fls64(a);
	b = ((b * 84) >> 8) - 1;
	shift = (a >> (b * 3));

	/* it is needed for verifier's bound check on v */
	if (shift >= 64)
		return 0;

	x = ((__u32)(((__u32)v[shift] + 10) << b)) >> 6;

	/*
	 * Newton-Raphson iteration
	 *                         2
	 * x    = ( 2 * x  +  a / x  ) / 3
	 *  k+1          k         k
	 */
	x = (2 * x + (__u32)div64_u64(a, (__u64)x * (__u64)(x - 1)));
	x = ((x * 341) >> 10);
	return x;
}

/*
 * Compute congestion window to use.
 */
static __always_inline void bictcp_update(struct bictcp *ca, __u32 cwnd,
					  __u32 acked)
{
	__u32 delta, bic_target, max_cnt;
	__u64 offs, t;

	ca->ack_cnt += acked;	/* count the number of ACKed packets */

	if (ca->last_cwnd == cwnd &&
	    (__s32)(tcp_jiffies32 - ca->last_time) <= HZ / 32)
		return;

	/* The CUBIC function can update ca->cnt at most once per jiffy.
	 * On all cwnd reduction events, ca->epoch_start is set to 0,
	 * which will force a recalculation of ca->cnt.
	 */
	if (ca->epoch_start && tcp_jiffies32 == ca->last_time)
		goto tcp_friendliness;

	ca->last_cwnd = cwnd;
	ca->last_time = tcp_jiffies32;

	if (ca->epoch_start == 0) {
		ca->epoch_start = tcp_jiffies32;	/* record beginning */
		ca->ack_cnt = acked;			/* start counting */
		ca->tcp_cwnd = cwnd;			/* syn with cubic */

		if (ca->last_max_cwnd <= cwnd) {
			ca->bic_K = 0;
			ca->bic_origin_point = cwnd;
		} else {
			/* Compute new K based on
			 * (wmax-cwnd) * (srtt>>3 / HZ) / c * 2^(3*bictcp_HZ)
			 */
			ca->bic_K = cubic_root(cube_factor
					       * (ca->last_max_cwnd - cwnd));
			ca->bic_origin_point = ca->last_max_cwnd;
		}
	}

	/* cubic function - calc*/
	/* calculate c * time^3 / rtt,
	 *  while considering overflow in calculation of time^3
	 * (so time^3 is done by using 64 bit)
	 * and without the support of division of 64bit numbers
	 * (so all divisions are done by using 32 bit)
	 *  also NOTE the unit of those veriables
	 *	  time  = (t - K) / 2^bictcp_HZ
	 *	  c = bic_scale >> 10
	 * rtt  = (srtt >> 3) / HZ
	 * !!! The following code does not have overflow problems,
	 * if the cwnd < 1 million packets !!!
	 */

	t = (__s32)(tcp_jiffies32 - ca->epoch_start) * USEC_PER_JIFFY;
	t += ca->delay_min;
	/* change the unit from usec to bictcp_HZ */
	t <<= BICTCP_HZ;
	t /= USEC_PER_SEC;

	if (t < ca->bic_K)		/* t - K */
		offs = ca->bic_K - t;
	else
		offs = t - ca->bic_K;

	/* c/rtt * (t-K)^3 */
	delta = (cube_rtt_scale * offs * offs * offs) >> (10+3*BICTCP_HZ);
	if (t < ca->bic_K)                            /* below origin*/
		bic_target = ca->bic_origin_point - delta;
	else                                          /* above origin*/
		bic_target = ca->bic_origin_point + delta;

	/* cubic function - calc bictcp_cnt*/
	if (bic_target > cwnd) {
		ca->cnt = cwnd / (bic_target - cwnd);
	} else {
		ca->cnt = 100 * cwnd;              /* very small increment*/
	}

	/*
	 * The initial growth of cubic function may be too conservative
	 * when the available bandwidth is still unknown.
	 */
	if (ca->last_max_cwnd == 0 && ca->cnt > 20)
		ca->cnt = 20;	/* increase cwnd 5% per RTT */

tcp_friendliness:
	/* TCP Friendly */
	if (tcp_friendliness) {
		__u32 scale = beta_scale;
		__u32 n;

		/* update tcp cwnd */
		delta = (cwnd * scale) >> 3;
		if (ca->ack_cnt > delta && delta) {
			n = ca->ack_cnt / delta;
			ca->ack_cnt -= n * delta;
			ca->tcp_cwnd += n;
		}

		if (ca->tcp_cwnd > cwnd) {	/* if bic is slower than tcp */
			delta = ca->tcp_cwnd - cwnd;
			max_cnt = cwnd / delta;
			if (ca->cnt > max_cnt)
				ca->cnt = max_cnt;
		}
	}

	/* The maximum rate of cwnd increase CUBIC allows is 1 packet per
	 * 2 packets ACKed, meaning cwnd grows at 1.5x per RTT.
	 */
	ca->cnt = max(ca->cnt, 2U);
}


/* "struct_ops/" prefix is not a requirement
 * It will be recognized as BPF_PROG_TYPE_STRUCT_OPS
 * as long as it is used in one of the func ptr
 * under SEC(".struct_ops").
 */
SEC("struct_ops/bictcp_init")
void BPF_PROG(bictcp_init, struct sock *sk)
{
	struct bictcp *ca = inet_csk_ca(sk);

	bictcp_reset(ca);

	if (hystart)
		bictcp_hystart_reset(sk);

	if (!hystart && initial_ssthresh)
		tcp_sk(sk)->snd_ssthresh = initial_ssthresh;
}

/* No prefix in SEC will also work.
 * The remaining tcp-cubic functions have an easier way.
 */
SEC("struct_ops/bictcp_cwnd_event")
void BPF_PROG(bictcp_cwnd_event, struct sock *sk, enum tcp_ca_event event)
{
	if (event == CA_EVENT_TX_START) {
		struct bictcp *ca = inet_csk_ca(sk);
		__u32 now = tcp_jiffies32;
		__s32 delta;

		delta = now - tcp_sk(sk)->lsndtime;

		struct event* e;
		e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
		if(!e)
			return;
		e->type = BICTCP_CWND_EVENT;
		bictcp_to_event(e,ca);
		/* We were application limited (idle) for a while.
		 * Shift epoch_start to keep cwnd growth to cubic curve.
		 */
		if (ca->epoch_start && delta > 0) {
			ca->epoch_start += delta;
			if (after(ca->epoch_start, now))
				ca->epoch_start = now;
		}
		return;
	}
}

/* Or simply use the BPF_STRUCT_OPS to avoid the SEC boiler plate. */
void BPF_STRUCT_OPS(bictcp_cong_avoid, struct sock *sk, __u32 ack, __u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);

	struct event* e;
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	if(!e)
		return;
	if (!tcp_is_cwnd_limited(sk))
		return;

	if (tcp_in_slow_start(tp)) {
		if (hystart && after(ack, ca->end_seq))
			bictcp_hystart_reset(sk);
		acked = tcp_slow_start(tp, acked);
		if (!acked)
			return;
	}
	e->type = BICTCP_CONG_AVOID;
	bictcp_to_event(e,ca);
	bictcp_update(ca, tp->snd_cwnd, acked);
	tcp_cong_avoid_ai(tp, ca->cnt, acked);
	bpf_ringbuf_submit(e,0);
}


__u32 BPF_STRUCT_OPS(bictcp_recalc_ssthresh, struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);

	ca->epoch_start = 0;	/* end of epoch */

	/* Wmax and fast convergence */
	if (tp->snd_cwnd < ca->last_max_cwnd && fast_convergence)
		ca->last_max_cwnd = (tp->snd_cwnd * (BICTCP_BETA_SCALE + beta))
			/ (2 * BICTCP_BETA_SCALE);
	else
		ca->last_max_cwnd = tp->snd_cwnd;

	return max((tp->snd_cwnd * beta) / BICTCP_BETA_SCALE, 2U);
}

void BPF_STRUCT_OPS(bictcp_state, struct sock *sk, __u8 new_state)
{
	if (new_state == TCP_CA_Loss) {
		bictcp_reset(inet_csk_ca(sk));
		bictcp_hystart_reset(sk);
	}
}


/* Account for TSO/GRO delays.
 * Otherwise short RTT flows could get too small ssthresh, since during
 * slow start we begin with small TSO packets and ca->delay_min would
 * not account for long aggregation delay when TSO packets get bigger.
 * Ideally even with a very small RTT we would like to have at least one
 * TSO packet being sent and received by GRO, and another one in qdisc layer.
 * We apply another 100% factor because @rate is doubled at this point.
 * We cap the cushion to 1ms.
 */
static __always_inline __u32 hystart_ack_delay(struct sock *sk)
{
	unsigned long rate;

	rate = sk->sk_pacing_rate;
	if (!rate)
		return 0;
	return min((__u64)USEC_PER_MSEC,
		   div64_ul((__u64)GSO_MAX_SIZE * 4 * USEC_PER_SEC, rate));
}


static __always_inline void hystart_update(struct sock *sk, __u32 delay)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);
	__u32 threshold;

	if (hystart_detect & HYSTART_ACK_TRAIN) {
		__u32 now = bictcp_clock_us(sk);

		/* first detection parameter - ack-train detection */
		if ((__s32)(now - ca->last_ack) <= hystart_ack_delta_us) {
			ca->last_ack = now;

			threshold = ca->delay_min + hystart_ack_delay(sk);

			/* Hystart ack train triggers if we get ack past
			 * ca->delay_min/2.
			 * Pacing might have delayed packets up to RTT/2
			 * during slow start.
			 */
			if (sk->sk_pacing_status == SK_PACING_NONE)
				threshold >>= 1;

			if ((__s32)(now - ca->round_start) > threshold) {
				ca->found = 1;
				tp->snd_ssthresh = tp->snd_cwnd;
			}
		}
	}

	if (hystart_detect & HYSTART_DELAY) {
		/* obtain the minimum delay of more than sampling packets */
		if (ca->sample_cnt < HYSTART_MIN_SAMPLES) {
			if (ca->curr_rtt > delay)
				ca->curr_rtt = delay;

			ca->sample_cnt++;
		} else {
			if (ca->curr_rtt > ca->delay_min +
			    HYSTART_DELAY_THRESH(ca->delay_min >> 3)) {
				ca->found = 1;
				tp->snd_ssthresh = tp->snd_cwnd;
			}
		}
	}
}



SEC("struct_ops/tcp_reno_undo_cwnd")
__u32 BPF_PROG(tcp_reno_undo_cwnd, struct sock*sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);

	return max(tp->snd_cwnd, tp->prior_cwnd);
}

SEC("struct_ops/bictcp_acked")
void BPF_PROG(bictcp_acked, struct sock *sk,
		    const struct ack_sample *sample)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);
	__u32 delay;

	struct event* e = bpf_ringbuf_reserve(&rb, sizeof(struct event), 0);
	if(!e)
		return;
	
	e->type = BICTCP_ACKED;
	bictcp_to_event(e,ca);
	/* Some calls are for duplicates without timetamps */
	if (sample->rtt_us < 0)
		return;

	/* Discard delay samples right after fast recovery */
	if (ca->epoch_start && (__s32)(tcp_jiffies32 - ca->epoch_start) < HZ)
		return;

	delay = sample->rtt_us;
	if (delay == 0)
		delay = 1;

	/* first time call or link delay decreases */
	if (ca->delay_min == 0 || ca->delay_min > delay)
		ca->delay_min = delay;

	/* hystart triggers when cwnd is larger than some threshold */
	if (!ca->found && tcp_in_slow_start(tp) && hystart &&
	    tp->snd_cwnd >= hystart_low_window)
		hystart_update(sk, delay);
}

SEC(".struct_ops")
struct tcp_congestion_ops cubic = {
	.init		= (void *)bictcp_init,
	.ssthresh	= (void *)bictcp_recalc_ssthresh,
	.cong_avoid	= (void *)bictcp_cong_avoid,
	.set_state	= (void *)bictcp_state,
	.undo_cwnd	= (void *)tcp_reno_undo_cwnd,
	.cwnd_event	= (void *)bictcp_cwnd_event,
	.pkts_acked     = (void *)bictcp_acked,
	.name		= "bpf_cubic",
};
