/*
 * pktgen_server.c
 * 	
 * Runs as a daemon and listens to jobs from pktgen_scheduler.py
 *
 */

#include "pktgen_server_util.h"

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
response_handler(int fd, char *request, int request_bytes)
{
	Job *j = job__unpack(NULL, request_bytes, request);

	if (j == NULL) {
		syslog(LOG_ERR, "Failed to unpack job.");
		return -1;
	}

	send_status(STATUS__TYPE__STARTED);

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
