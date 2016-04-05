#ifndef PKTGEN_WORKER_H
#define PKTGEN_WORKER_H

#include "pktgen.h"

static uint16_t gen_pkt_size(struct pktgen_config *config);
static void stats(double *start_time, struct rate_stats *r_stats);
static void latency_calc(double *samples, uint32_t sample_count, struct pktgen_config *config);
static void generate_packet(struct rte_mbuf *buf, struct pktgen_config *config, uint64_t key);
static void generate_traffic(struct rte_mbuf **tx_bufs, struct pktgen_config *config, uint64_t key);
static void worker_loop(struct pktgen_config *config);
static int launch_worker(void *config);

#endif