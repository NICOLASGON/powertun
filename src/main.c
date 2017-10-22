/*******************************************************************************
 *
 *  This file is part of powertun.
 *
 * powertun is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * powertun is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with powertun.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stropts.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define BUFSIZE			2000
#define CLIENT_MODE		0
#define SERVER_MODE		1
#define TCP_MODE		0
#define UDP_MODE		1
#define TUN_MODE		0
#define TAP_MODE		1
#define DEFAULT_PORT	6666
#define LISTEN_MAX		5

#define OPT_TUN			100
#define OPT_TAP			101
#define OPT_TCP			102
#define OPT_UDP			103

static int verbose = 0;
static char *program_name = NULL;
static int mode     = SERVER_MODE;
static int nwk_mode = TCP_MODE;
static int if_mode  = TUN_MODE;

struct sockaddr_in si_other;
socklen_t slen = sizeof(si_other);

static int tun_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;

	if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
		return tun_alloc(dev);

	memset(&ifr, 0, sizeof(ifr));

	if( if_mode == TUN_MODE )
		ifr.ifr_flags = IFF_TUN;
	else
		ifr.ifr_flags = IFF_TAP;

	if( *dev )
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 )
	{
		close(fd);
		return err;
	}
	strcpy(dev, ifr.ifr_name);
	return fd;
}        

static int cread(int fd, char *buf, int n, int socket_mode)
{
	int nread;

	if( socket_mode == TCP_MODE )
	{
		if((nread=read(fd, buf, n)) < 0)
		{
			perror("TCP Reading data");
			exit(1);
		}
	}
	else
	{
		if((nread=recvfrom(fd, buf, n, 0, (struct sockaddr *) &si_other, &slen)) < 0)
		{
			perror("UDP Reading data");
			exit(1);
		}
	}

	return nread;
}

static int cwrite(int fd, char *buf, int n, int socket_mode)
{
	int nwrite;

	if( nwk_mode == TCP_MODE )
	{
		if((nwrite=write(fd, buf, n)) < 0)
		{
			perror("TCP Writing data");
			exit(1);
		}
	}
	else
	{
		if((nwrite=sendto(fd, buf, n, 0, (struct sockaddr*) &si_other, slen)) < 0)
		{
			perror("UDP Writing data");
			exit(1);
		}
	}

	return nwrite;
}

static int read_n(int fd, char *buf, int n, int socket_mode)
{
	int nread, left = n;

	while(left > 0)
	{
		if ((nread = cread(fd, buf, left, socket_mode)) == 0)
		{
			return 0 ;      
		}
		else
		{
			left -= nread;
			buf += nread;
		}
	}
	return n;  
}

static void usage(FILE* stream, int exit_code)
{
	fprintf(stream, "Usage: %s\n", program_name);
	fprintf(stream, "\t-h: prints this help text\n");
	fprintf(stream, "\t-i: indicated tun/tap interface\n");
	fprintf(stream, "\t-c <ip>: client mode, connect to server <ip>\n");
	fprintf(stream, "\t-s: server mode\n");
	fprintf(stream, "\t-v: verbose mode\n");
	exit(exit_code);
}

static void error(char *error_msg, ...)
{
	va_list argp;
	va_start(argp, error_msg);
	vfprintf(stderr, error_msg, argp);
	va_end(argp);
	exit(1);
}

int main(int argc, char* argv[])
{
	int next_option;
	int tunfd, sockfd, netfd, maxfd;
	char tun_name[IFNAMSIZ] = "";
	struct sockaddr_in local, server;
	char server_ip[16] = "";
	unsigned short int port = DEFAULT_PORT;
	socklen_t serverlen;
	uint16_t nread, nwrite, plength;
	int optval = 1;
	char buffer[BUFSIZE];

	const char* const short_options = "hi:sc:p:v";
	const struct option long_options[] = {
		{ "help",      0, NULL, 'h' },
		{ "interface", 1, NULL, 'i' },
		{ "client",    1, NULL, 'c' },
		{ "server",    0, NULL, 's' },
		{ "port",      1, NULL, 'p' },
		{ "verbose",   0, NULL, 'v' },
		{ "tun",       0, NULL, OPT_TUN },
		{ "tap",       0, NULL, OPT_TAP },
		{ "udp",       0, NULL, OPT_UDP },
		{ "tcp",       0, NULL, OPT_TCP },
		{ NULL,        0, NULL , 0  }
	};

	program_name = argv[0];

	do {
		next_option = getopt_long(argc, argv, short_options, long_options, NULL);
		switch(next_option)
		{
			case 'h':
				usage(stdout, 0);
				break;
			case 'i':
				strncpy(tun_name,optarg, IFNAMSIZ-1);
				break;
			case 's':
				mode = SERVER_MODE;
				break;
			case 'c':
				mode = CLIENT_MODE;
				strncpy(server_ip,optarg,15);
				break;
			case 'v':
				verbose = 1;
				break;
			case OPT_TCP:
				nwk_mode = TCP_MODE;
				break;
			case OPT_UDP:
				nwk_mode = UDP_MODE;
				break;
			case OPT_TUN:
				if_mode = TUN_MODE;
				break;
			case OPT_TAP:
				if_mode = TAP_MODE;
				break;
			case '?':
				usage(stderr, 1);
				break;
			case -1:
				break;
			default:
				usage(stderr, 1);
				break;
		}
	} while(next_option != -1);

	if(*tun_name == '\0')
		error("Must specify interface name!\n");

	/* Allocation of TUN/TAP interface */
	if( (tunfd = tun_alloc(tun_name)) < 0 )
	{
		error("Failed to connect to %s interface.\n", tun_name);
	}

	if( nwk_mode == TCP_MODE )
	{
		if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			perror("socket()");
			exit(1);
		}
	}
	else
	{
		if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			perror("socket()");
			exit(1);
		}
	}

	if(mode == CLIENT_MODE)
	{
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_addr.s_addr = inet_addr(server_ip);
		server.sin_port = htons(port);

		if( nwk_mode == TCP_MODE )
		{
			if (connect(sockfd, (struct sockaddr*) &server, sizeof(server)) < 0)
			{
				perror("connect()");
				exit(1);
			}
		}

		netfd = sockfd;
	}
	else
	{
		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0)
		{
			perror("setsockopt()");
			exit(1);
		}

		memset(&local, 0, sizeof(local));
		local.sin_family = AF_INET;
		local.sin_addr.s_addr = htonl(INADDR_ANY);
		local.sin_port = htons(port);
		if (bind(sockfd, (struct sockaddr*) &local, sizeof(local)) < 0)
		{
			perror("bind()");
			exit(1);
		}

		if( nwk_mode == TCP_MODE )
		{
			if (listen(sockfd, LISTEN_MAX) < 0)
			{
				perror("listen()");
				exit(1);
			}

			serverlen = sizeof(server);
			memset(&server, 0, serverlen);
			if ((netfd = accept(sockfd, (struct sockaddr*)&server, &serverlen)) < 0)
			{
				perror("accept()");
				exit(1);
			}
		}
	}

	maxfd = (tunfd > netfd) ? tunfd : netfd;

	while(1)
	{
		int ret;
		fd_set rd_set;

		FD_ZERO(&rd_set);
		FD_SET(tunfd, &rd_set);
		FD_SET(netfd, &rd_set);

		ret = select(maxfd + 1, &rd_set, NULL, NULL, NULL);

		if (ret < 0 && errno == EINTR)
			continue;

		if (ret < 0)
		{
			perror("select()");
			exit(1);
		}

		/* Received packet from TUN interface */
		if(FD_ISSET(tunfd, &rd_set))
		{  
			nread = cread(tunfd, buffer, BUFSIZE, TCP_MODE);

			plength = htons(nread);
			nwrite = cwrite(netfd, (char *)&plength, sizeof(plength), nwk_mode);
			nwrite = cwrite(netfd, buffer, nread, nwk_mode);
		}

		/* Received packet from the tunnel */
		if(FD_ISSET(netfd, &rd_set))
		{
			nread = read_n(netfd, (char *)&plength, sizeof(plength), nwk_mode);
			if(nread == 0)
				break;

			nread  = read_n(netfd, buffer, ntohs(plength), nwk_mode);
			nwrite = cwrite(tunfd, buffer, nread, TCP_MODE);
		}
	}

	return EXIT_SUCCESS;
}
