#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>


#define MP2_NODE_EXEC "mp2_node"

int listener_port;
int node_zero_port;

int m_value;

static int sock;

pthread_t net_recv_thread;

void *net_recv_handler(void *eh)
{
	struct sockaddr_in fromaddr;
	socklen_t len;
	int nbytes;
	char buf[1000];

	while (1)
	{
		len = sizeof(fromaddr);
		nbytes = recvfrom(sock, buf, 1000, 0, (struct sockaddr *)&fromaddr, &len);

		if (nbytes < 0)
		{
			// May want to tell the ring to quit at this point if we have errs like this.
			fprintf(stderr,"Error in recv from socket\n");
			continue;
		}

		// Handle sanitization and printout here.
		printf("recv: %s\n", buf);

	}

	return NULL;

}

void node_zero_init()
{
	struct stat buf;
	int pid, exec_retval;

	char passed_port[20];
	char m_bit_value[5];

	snprintf(passed_port, 19, "%d", listener_port);
	snprintf(m_bit_value, 4, "%d", m_value);

	pid = fork();

	if (pid == 0)
	{
		exec_retval = execl(MP2_NODE_EXEC, MP2_NODE_EXEC, m_bit_value, "0", passed_port, (char *) 0);

	}
	else
		return;

	if (exec_retval != 0)
	{
		fprintf(stderr, "Error in node zero!!! %d\n", exec_retval);
		fprintf(stderr, strerror(errno));
		exit(1);
	}
	else
	{
		exit(0);
	}

}

void listener_net_init()
{
	struct sockaddr_in servaddr;
	socklen_t len;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		fprintf(stderr,"Error init socket for listening\n");
		exit(1);
	}

	memset(&servaddr,0,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	servaddr.sin_port=htons(0);
	if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		fprintf(stderr,"error in binding to port\n");
		exit(1);
	}


	len = sizeof(servaddr);
	if (getsockname(sock, (struct sockaddr *) &servaddr, &len) < 0)
	{
		fprintf(stderr,"Error in getting socket name\n");
		exit(1);
	}

	listener_port = ntohs(servaddr.sin_port);
	fprintf(stderr, "Port number for listener: %d\n", listener_port);

	if (pthread_create(&net_recv_thread, NULL, &net_recv_handler, NULL) != 0) {
		fprintf(stderr,"Error in creating a thread\n");
		exit(1);
	}

}

int main(int argc, char *argv[])
{
	char str[256];
	int len;

	if (argc != 2)
	{
		fprintf(stderr, "Incorrect number of arguments!\n");
		exit(1);
	}

	m_value = atoi(argv[1]);


	if (m_value < 5 || m_value > 10)
	{
		fprintf(stderr, "Incorrect value for m, must be between 5 and 10\n");
		exit(1);
	}

	listener_net_init();

	node_zero_init();

	while (1)
	{

		if(fgets(str, sizeof(str), stdin) == NULL)
		{
			// quit gracefully here... something went bad.  For now we just short of eject the warp core
			exit(1);
		}
		len = strlen(str);

		if (str[len-1] == '\n')
			str[len-1] = 0;

		if (strncmp(str, "ADD_FILE", 8) == 0)
		{
			printf("Add file %s\n", str+8);
		}

		else if (strncmp(str, "ADD_NODE", 8) == 0)
		{
			printf("Add node %s\n", str+9);
		}


		else if (strncmp(str, "FIND_FILE", 9) == 0)
		{
			printf("Find file %s\n", str+9);
		}

		else if (strncmp(str, "DEL_FILE", 8) == 0)
		{
			printf("Delete file %s\n", str+9);
		}

		else if (strncmp(str, "GET_TABLE", 9) == 0)
		{
			printf("Get table %s\n", str+10);
		}

		else if (strncmp(str, "QUIT", 4) == 0)
		{
			printf("Quitting\n");
			break;
		}


		else
		{
			printf("? SYNTAX ERROR\n");
		}

	}

	return 0;
}
