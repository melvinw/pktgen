/*
 * pktgen_server.c
 * 	
 * Runs as a daemon and listens to jobs from pktgen_scheduler.py
 *
 */

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

#define PORT "1729"
#define SCHEDULER_IP "127.0.0.1"
#define SCHEDULER_PORT "1800"
#define BUFSIZE 8192
#define BACKLOG 25

static void 
setup_daemon()
{

	pid_t pid;

	// fork off parent process
	pid = fork();
	if (pid < 0) {
		// parent failed to fork child and exit
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		// successfully forked child and exit parent
		exit(EXIT_SUCCESS);
	}

	// give child a unique SID; child is a "session leader"
	if (setsid() < 0) {
		// failed to assign SID and exit
		exit(EXIT_FAILURE);
	}

	// fork off parent process again
	pid = fork();
	if (pid < 0) {
		// parent failed to fork child and exit
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		// successfully forked child and exit parent
		exit(EXIT_SUCCESS);
	}

	// so we can write/read files
	umask(0);	

	// change working directory to somewhere guaranteed to exit
	if ((chdir("/")) < 0) {
		// failed to change directory and exit
		exit(EXIT_FAILURE);
    }

    // close off STD's
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	openlog("pktgen_server", LOG_PID, LOG_DAEMON);
}

int 
create_and_bind_socket() 
{
	int yes = 1;
	int status, fd_server;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
		return -1;
	}

	for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
		if ((fd_server = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
			continue;
		}
		setsockopt(fd_server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(fd_server, p->ai_addr, p->ai_addrlen) < 0) {
            close(fd_server);
            continue;
        }
		break;
	}

	freeaddrinfo(res);

    return fd_server;

}

int connect_socket(char *ip, char *port)
{
	int sock, status;
	struct addrinfo hints, *res, *p;

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(ip, port, &hints, &res)) != 0) {
        return -1;
    }

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock);
            continue;
        }
		break;
    }

    if (p == NULL) {
        return -2;
    }

    freeaddrinfo(res); 

    return sock;

}

int
read_n_bytes(int sock, int n, char *buf) 
{	
	int bytes_read = 0;
	while (bytes_read < n) {
		int res = recv(sock, buf + bytes_read, n - bytes_read, 0);
		if (bytes_read < 1) {
			break;
		}
		bytes_read += res;
	}
	return bytes_read;
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
	
	// convert to host
	req_len = ntohl(*((int32_t*) &len_buf));

	// read req_len bytes from the socket
	if (read_n_bytes(fd_client, req_len, request) < 0) {
		syslog(LOG_ERR, "Failed to read status.");
	}

	// return len of Job
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
response_handler(int fd, char *request, int request_bytes)
{
	Job *j = job__unpack(NULL, request_bytes, request);

	if (j == NULL) {
		syslog(LOG_ERR, "Failed to unpack job.");
		return -1;
	}

	// just print the job data for now
	syslog(LOG_ERR, "m: %d t: %d w: %d n: %d s_min: %d "
		            "s_max: %d r: %d l: %d.",
		            j->m,
		            j->t,
		            j->w,
		            j->n,
		            j->s_min,
		            j->s_max,
		            j->r,
		            j->l);

	// sleep for 10 seconds like its actually doing something
	sleep(10);

	// unpack job
	job__free_unpacked(j, NULL);
	
	// send success status regardless for now
	return send_status(STATUS__TYPE__SUCCESS);
}

void
sigchild_handler(int sig) 
{
	int tmp_errno = errno;
	while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
	errno = tmp_errno;
}

int
main(void) 
{
	// setup daemon
	setup_daemon();

	syslog(LOG_NOTICE, "Pktgen Server started.");
	
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
            		syslog(LOG_INFO, "Sucessfully completed job.");
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
