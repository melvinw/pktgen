#include "pktgen_server_util.h"

void 
setup_daemon()
{

	pid_t pid;

	pid = fork();
	
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}
	if (setsid() < 0) {
		exit(EXIT_FAILURE);
	}
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	umask(0);	

	if ((chdir("/")) < 0) {
		exit(EXIT_FAILURE);
    }

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
	struct addrinfo hints, *res, *p;

	memset(&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((status = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
		return -1;
	}

	for (p = res; p != NULL; p = p->ai_next) {
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

int 
connect_socket(char *ip, char *port)
{
	int sock, status;
	struct addrinfo hints, *res, *p;

	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(ip, port, &hints, &res)) != 0) {
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
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

void
sigchild_handler(int sig) 
{
	int tmp_errno = errno;
	while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
	errno = tmp_errno;
}
