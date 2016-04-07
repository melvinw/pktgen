/*
 * pktgen_server.c
 * 	
 * Runs as a daemon and listens to jobs from pktgen_scheduler.py
 *
 */

#include "pktgen_server_util.h"
#include "pktgen.h"

static inline int port_init(uint8_t port, struct pktgen_config *config UNUSED) {
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    int retval;
    uint16_t q;
    char name[7];

    rte_eth_dev_stop(port); 

    snprintf(name, sizeof(name), "RX%02u:%02u", port, (unsigned)0);
    struct rte_mempool *rx_mp = rte_pktmbuf_pool_create(name, GEN_DEFAULT_RX_RING_SIZE,
            0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, 0);
    if (rx_mp == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create RX mbuf pool: %s\n", rte_strerror(rte_errno));
    }

    snprintf(name, sizeof(name), "TX%02u:%02u", port, (unsigned)0);
    struct rte_mempool *tx_mp = rte_pktmbuf_pool_create(name, NUM_PKTS,
            0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, 0);
    if (tx_mp == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create TX mbuf pool: %s\n", rte_strerror(rte_errno));
    }

    if (port >= rte_eth_dev_count()) {
        return -1;
    }

    if (rte_eth_dev_configure(port, 1, 1, &port_conf) != 0) {
        rte_exit(EXIT_FAILURE, "Error with port configuration: %s\n", rte_strerror(rte_errno));
    }

    /* Allocate and set up 1 RX queue per Ethernet port. */
    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port, q, GEN_DEFAULT_RX_RING_SIZE,
                rte_eth_dev_socket_id(port), NULL, rx_mp);
        if (retval != 0) {
            return retval;
        }
    }

    /* Allocate and set up 1 TX queue per Ethernet port. */
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, GEN_DEFAULT_TX_RING_SIZE,
                rte_eth_dev_socket_id(port), NULL);
        if (retval != 0) {
            return retval;
        }
    }

    /* Start the Ethernet port. */
    retval = rte_eth_dev_start(port);
    if (retval < 0) {
        return retval;
    }

    /* Display the port MAC address. */
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
            " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
            (unsigned)port,
            addr.addr_bytes[0], addr.addr_bytes[1],
            addr.addr_bytes[2], addr.addr_bytes[3],
            addr.addr_bytes[4], addr.addr_bytes[5]);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    rte_eth_promiscuous_enable(port);

    return 0;
}

static int lcore_init(void *arg) {
    struct pktgen_config *config = (struct pktgen_config*)arg;
    unsigned port = config->port;
    char name[7];

    printf("Init core %d\n", rte_lcore_id());

    config->seed.a = 1;
    config->seed.b = 2;
    config->seed.c = 3;
    config->seed.d = 4;
    raninit(&config->seed, (u8) get_time_sec());

    snprintf(name, sizeof(name), "RX%02u:%02u", port, (unsigned)0);
    config->rx_pool = rte_mempool_lookup(name);

    if (config->rx_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create RX mbuf pool: %s\n", rte_strerror(rte_errno));
    }

    snprintf(name, sizeof(name), "%s%02u:%02u", "TX", port, (unsigned)0);
    config->tx_pool = rte_mempool_lookup(name);

    if (config->tx_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create TX mbuf pool: %s\n", rte_strerror(rte_errno));
    }

    return 0;
}

static void usage(void) {
    printf("tgen> [-rlo] -m M -t T -w W -n N -s MIN[-MAX]\n"
            "Traffic Gen Options and Flags\n"
            "\t-m M          Transmit at M mpbs\n"
            "\t-t T          Generate traffic for T seconds\n"
            "\t-w W          Warmup for W seconds before generating traffic\n"
            "\t-n N          Generate a uniform distribution of N flows\n"
            "\t-s MIN[-MAX]  Genearte packets with sizes in [MIN,MAX] (or only of size MIN if MAX isn't specified)\n"
            "\t-r            Randomize packet payloads\n"
            "\t-l            Measure latency\n"
            "\t-o            Generate packets online\n");
}

int
request_handler(int fd_client, char *request)
{

	int32_t req_len;
	char len_buf[4];

	// read 4 bytes to get the length of the job
	if (read_n_bytes(fd_client, 4, len_buf) < 0) {
		syslog(LOG_ERR, "Failed to read length of status.");
	}
	
	req_len = ntohl(*((int32_t*) &len_buf));

	// read req_len bytes from the socket
	if (read_n_bytes(fd_client, req_len, request) < 0) {
		syslog(LOG_ERR, "Failed to read status.");
	}

	return req_len;
}

int
send_status(int status)
{
	int sock = connect_socket(SCHEDULER_IP, SCHEDULER_PORT);
	unsigned len;
	int32_t packed_len;
	void *buf;

	if (sock < 0) {
		syslog(LOG_ERR, "Failed to connect to the scheduler to send status.");
		close(sock);
		return -1;
	}

	// set status
	Status s = STATUS__INIT;
	s.has_type = 1;
	s.type = status;

	// get length of serialized data
	len = status__get_packed_size(&s);
	buf = malloc(len+4);
	packed_len = htonl((int32_t) len);

	// set the first 4 bytes to be the length of data
	// then pack the status into the buf
	memcpy(buf, &packed_len, 4);
	status__pack(&s, buf+4);
	
	// send the buf to the socket
	if (send(sock, buf, len+4, 0) < 0) {
		syslog(LOG_ERR, "Failed to send status to the scheduler.");
		close(sock);
		return -1;
	}
	// finish off
	close(sock);
	return 0;
}

int
response_handler(int fd, char *request, int request_bytes, struct pktgen_config *cmd)
{
	Job *j = job__unpack(NULL, request_bytes, request);

	if (j == NULL) {
		syslog(LOG_ERR, "Failed to unpack job.");
		return -1;
	}

    cmd->flags &= !FLAG_WAIT;
    cmd->port_mac = zero_mac;

    cmd->tx_rate = j->tx_rate;
    cmd->warmup = j->warmup;
    cmd->duration = j->duration;
    cmd->num_flows = j->num_flows;
    cmd->size_min = j->size_min;
    cmd->size_max = j->size_max;
    cmd->port_min = j->port_min;
    cmd->port_max = j->port_max;
    cmd->life_min = j->life_min;
    cmd->life_max = j->life_max;

    ether_addr_from_str(j->dst_mac, &cmd->dst_mac);
    ether_addr_from_str(j->port, &cmd->port_mac);

    if (j->randomize)
        cmd->flags |= FLAG_RANDOMIZE_PAYLOAD;

    if (j->latency)
        cmd->flags |= FLAG_MEASURE_LATENCY;

    if (j->online)
        cmd->flags |= FLAG_GENERATE_ONLINE;

    if (j->stop)
        cmd->flags |= FLAG_WAIT;

    if (j->print)
        cmd->flags |= (FLAG_PRINT | FLAG_WAIT);

    if (j->tcp)
        cmd->proto = 6;
    else
        cmd->proto = 17; 

    if (cmd->life_min >= 0)
        cmd->flags |= FLAG_LIMIT_FLOW_LIFE;

	send_status(STATUS__TYPE__STARTED);

	// just print the job data for now
	// syslog(LOG_ERR, "m: %d t: %d w: %d n: %d s_min: %d "
	// 	            "s_max: %d r: %d l: %d.",
	// 	            j->m,
	// 	            j->t,
	// 	            j->w,
	// 	            j->n,
	// 	            j->s_min,
	// 	            j->s_max,
	// 	            j->r,
	// 	            j->l);

	// sleep for 10 seconds like its actually doing something
	//sleep(10);

	// unpack job
	job__free_unpacked(j, NULL);
	
	// send success status regardless for now
	return send_status(STATUS__TYPE__SUCCESS);
}

int
main(int argc, char *argv[]) 
{
	// setup daemon
	setup_daemon();

    uint8_t nb_ports, port, nb_cores, core; 
    struct rte_mempool *mp UNUSED;
    struct pktgen_config cmd;
    char *icmd;

    /* Initialize EAL */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        syslog(LOG_ERR, "Error with EAL initialization");
        exit(EXIT_FAILURE);
    }

    nb_ports = rte_eth_dev_count();
    nb_cores = rte_lcore_count();
    uint8_t port_map[nb_cores];

    core = 0;
    for (port = 0; port < nb_ports; port++) {
        if (port_init(port, NULL) != 0) {
            syslog(LOG_ERR, "Cannot init port %" PRIu8, port);
            closelog();
            exit(EXIT_FAILURE);
        }
        port_map[core++] = port;
    }

    syslog(LOG_NOTICE, "Pktgen Server started.");

    // get the workers running
    int i;
    core = 0;
    port = 0;
    struct pktgen_config config[nb_cores];
    RTE_LCORE_FOREACH_SLAVE(i) {
        if (port == nb_ports) {
            break;
        }
        rte_eal_remote_launch(lcore_init, (void*)&config[i], i);
        rte_eal_wait_lcore(i);
        rte_eal_remote_launch(launch_worker, (void*)&config[i], i);
        core++;
        port++;
    }
	
	int request_bytes;
	char request[BUFSIZE];
	int fd_server, fd_client;
	struct sockaddr_storage addr_client;
	struct sigaction sigact;

	sigact.sa_handler = &sigchild_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART | SA_NOCLDSTOP;    

	if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
		syslog(LOG_ERR, "Failed to setup sigchild handler.");
		closelog();
		exit(EXIT_FAILURE);
	}	

	if ((fd_server = create_and_bind_socket()) < 0) {
		syslog(LOG_ERR, "Failed to create/bind to socket.");
		closelog();
		exit(EXIT_FAILURE);
	}

    if (listen(fd_server, BACKLOG) == -1) {
        syslog(LOG_ERR, "Failed to listen to socket.");
        closelog();
        exit(EXIT_FAILURE);
    }

    while(1) {
    	socklen_t sin_size = sizeof addr_client;
    	if ((fd_client = accept(fd_server, (struct sockaddr *)&addr_client, &sin_size)) < 0) {
    		syslog(LOG_ERR, "Failed to accept connection from scheduler.");
    		continue;
    	}
        if (!fork()) {
        	close(fd_server);
			if ((request_bytes = request_handler(fd_client, request)) > 0) {
            	if (response_handler(fd_client, request, request_bytes) != -1) {
            		syslog(LOG_INFO, "Sucessfully started job.");
            	} else {
            		syslog(LOG_ERR, "Failed to respond to request from scheduler.");
            	}
			} else {
				syslog(LOG_ERR, "Failed to process request from scheduler.");
			}
			close(fd_client);
			exit(EXIT_SUCCESS);
        }
		close(fd_client);
	}
}
