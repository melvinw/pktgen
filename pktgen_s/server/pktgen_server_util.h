/*s
 * pktgen_util.h
 * 	
 * Utility functions for pktgen server.
 *
 */

#ifndef PKTGEN_UTIL_H_
#define PKTGEN_UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/wait.h>
#include <stdint.h>

#include "job.pb-c.h"
#include "status.pb-c.h"

#include "pktgen_worker.h"

#define PORT "1729"
#define SCHEDULER_IP "127.0.0.1"
#define SCHEDULER_PORT "1800"
#define BUFSIZE 8192
#define BACKLOG 25

void sigchild_handler(int sig);
void setup_daemon(void);
int create_and_bind_socket(void);
int connect_socket(char *ip, char *port);
int read_n_bytes(int sock, int n, char *buf);

#endif