/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *
 * Copyright (c) 2019, NEC Laboratories Europe GmbH, NEC Corporation.
 *                     All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <uk/alloc.h>
#include <uk/syscall.h>
#include <poll.h>
#define LISTEN_PORT 8123

#define BUFLEN 2048

static char recvbuf[BUFLEN];
#define IPADDR_ANY          ((uint32_t)0x00000000UL)

#define AF_UNSPEC 0
#define AF_UNIX 1   /* Unix domain sockets      */
#define AF_LOCAL 1  /* POSIX name for AF_UNIX   */
#define AF_INET 2   /* Internet IP Protocol     */
#define AF_INET6 10 /* IP version 6         */

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

#define MSG_DONTWAIT 0x40 /* Nonblocking io		 */

struct in_addr {
	uint32_t s_addr;
};

struct sockaddr_in {
	uint16_t        sin_family;
	uint16_t        sin_port;
	struct in_addr  sin_addr;
#define SIN_ZERO_LEN 20
	char            sin_zero[SIN_ZERO_LEN];
};

struct sockaddr {
	uint16_t    sa_family;
	char        sa_data[26];
};

typedef unsigned int socklen_t;

struct msghdr {
  void *msg_name;
  socklen_t msg_namelen;
  struct iovec *msg_iov;
  int msg_iovlen;
  void *msg_control;
  socklen_t msg_controllen;
  int msg_flags;
};

#define HTONS(n) \
  (((((unsigned short)(n)&0xFF)) << 8) | (((unsigned short)(n)&0xFF00) >> 8))

void load_cpu(void)
{
	uint64_t x=94534634646443534,y=7899345345345,z=2343523523242342342;
	fprintf(stderr, "Starting cpu high load\n");
	while(1) {
		y = x * z * (y / 25674534554534);
	}
}

void load_mem(long size_mb, int count)
{
	long randval = 989978777;
	int i = 0;
	fprintf(stderr, "Starting mem high load\n");

	while (1) {
		if (i++ < count) {
			void *holder = calloc(size_mb * 1024 * 1024, sizeof(char));
			memcpy(holder, &randval, sizeof(long));
		}
		sleep(1);
	}
}

void load_cpu_mem(long size_mb)
{
	long randval = 989978777;
	uint64_t x=94534634646443534,y=7899345345345,z=2343523523242342342;
	void *holder;

	fprintf(stderr, "Starting cpu&mem high load\n");

	holder = calloc(size_mb * 1024 * 1024, sizeof(char));
	memcpy(holder, &randval, sizeof(long));

	while(1) {
		y = x * z * (y / 25674534554534);
	}
}

int main(int argc __attribute__((unused)),
	 char *argv[] __attribute__((unused)))
{
	int i, rc = 0;
	int srv, client;
	int timeout;
	nfds_t nfds = 1, current_size;
	struct pollfd fds[200];
	struct sockaddr_in srv_addr;
	int size_mb = 1, alloc_count = 3, port = LISTEN_PORT;
#if !defined(UK_LIBC_SYSCALLS)
	fprintf(stderr, "UK_LIBC_SYSCALLS should be enabled, exitting\n");
	return 0;
#endif

	for (i = 1; i < argc; i++) {
		if (strncmp(argv[i], "port=", strlen("port=")) == 0) {
			port = atoi(argv[i] + strlen("port="));
			fprintf(stderr, "port = %d\n", port);
		} else
		if (strncmp(argv[i], "size_mb=", strlen("size_mb=")) == 0) {
			size_mb = atoi(argv[i] + strlen("size_mb="));
			fprintf(stderr, "size_mb = %d\n", size_mb);
		} else
		if (strncmp(argv[i], "alloc_count=", strlen("alloc_count=")) == 0) {
			alloc_count = atoi(argv[i] + strlen("alloc_count="));
			fprintf(stderr, "alloc_count = %d\n", alloc_count);
		}
	}

	srv = uk_syscall_r_socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0) {
		fprintf(stderr, "Failed to create socket: %d\n", errno);
		goto out;
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = IPADDR_ANY;
	srv_addr.sin_port = (uint16_t)HTONS(port);

	rc = uk_syscall_r_bind(srv, (struct sockaddr *) &srv_addr, sizeof(srv_addr));
	if (rc < 0) {
		fprintf(stderr, "Failed to bind socket: %d\n", errno);
		goto out;
	}

	/* Accept one simultaneous connection */
	rc = uk_syscall_r_listen(srv, 1);
	if (rc < 0) {
		fprintf(stderr, "Failed to listen on socket: %d\n", errno);
		goto out;
	}

	memset(fds, 0, sizeof(fds));
	fds[0].fd = srv;
	fds[0].events = POLLIN;
	timeout =  10 * 1000;
	printf("Listening on port %d... start polling\n", port);
	while (1) {
		rc = uk_syscall_r_poll(fds, nfds, timeout);
		if (rc < 0) {
			fprintf(stderr,
					"Failed to poll incoming connection: %d\n",
					errno);
			goto out;
		}

		current_size = nfds;
		printf("tick\n");
		for (i = 0; i < current_size; i++) {
			if (fds[i].revents == 0)
				continue;

			if (fds[i].revents != POLLIN)
				break;

			if (fds[i].fd == srv) {
               client = uk_syscall_r_accept(srv, NULL, NULL);
			   if (client < 0) {
				   printf("Err: %d\n", __LINE__);
				   break;
			   }
			   fds[nfds].fd = client;
			   fds[nfds].events = POLLIN;
			   nfds++;
            } else {
              /* Receive some bytes (ignore errors) */
              uk_syscall_r_read(client, recvbuf, BUFLEN);
              printf("Received pvbuf = %s\n", recvbuf);
              uk_syscall_r_write(client, "123", sizeof("123"));
			  uk_syscall_r_close(client);

              fds[i].fd = -1;
              nfds--;
            }
		}
	}
	uk_syscall_r_close(srv);

out:
	return rc;
}
