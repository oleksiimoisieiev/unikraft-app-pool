
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
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <uk/alloc.h>
#include <uk/blkdev.h>
#include <dirent.h>
#include <netinet/in.h>

#define LISTEN_PORT 8123
static const char reply_format[] = "HTTP/1.1 200 OK\r\n" \
			    "Content-type: text/html\r\n" \
			    "Connection: close\r\n" \
			    "\r\n" \
			    "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\">" \
			    "<html>" \
			    "<head><title>It works!</title></head>" \
			    "<body><h1>It works! blkdev_data=%s</h1><p>This is only a test.</p></body>" \
			    "</html>\n";

#define BUFLEN 2048
static char recvbuf[BUFLEN];
static char replybuf[BUFLEN];
static char blkdev_data[BUFLEN];

void init_block_device()
{
	int rc;
	struct uk_blkdev *blkdev;
	const struct uk_blkdev_conf conf = { .nb_queues = 1 };
	const struct uk_blkdev_queue_conf q_conf =
		{
		 .a = uk_alloc_get_default(),
		 .callback = 0,
		 .callback_cookie = 0,
		};
	struct uk_blkdev_queue_info q_info;

	memset(blkdev_data, 0, BUFLEN);

	printf("uk_blkdev_count = %u\n", uk_blkdev_count());
	if (uk_blkdev_count() == 0)
		return;

	blkdev = uk_blkdev_get(0);
	if (!blkdev) {
		printf("No blkdev at index 0");
		return;
	}
	rc = uk_blkdev_configure(blkdev, &conf);
	if (rc) {
		printf("%d : rc = %d", __LINE__, rc);
		return;
	}

	rc = uk_blkdev_queue_get_info(blkdev, 0, &q_info);
	if (rc) {
		printf("%d : rc = %d", __LINE__, rc);
		return;
	}

	rc = uk_blkdev_queue_configure(blkdev, 0, 2, &q_conf);
	if (rc) {
		printf("%d : rc = %d", __LINE__, rc);
		return;
	}

	rc = uk_blkdev_start(blkdev);
	if (rc) {
		printf("%d : rc = %d", __LINE__, rc);
		return;
	}

	sprintf(blkdev_data, "Got info nb_max = %d, nb_min = %d\n"
		   "caps: ssize = %ld, mode = %d, size = %ld sectors = %ld\n"
		   "drv name = %s\n"
		   "state (RUNNING if 3) = %d\n",
		   q_info.nb_max, q_info.nb_min,
		   uk_blkdev_ssize(blkdev), uk_blkdev_mode(blkdev), uk_blkdev_size(blkdev),
		   uk_blkdev_sectors(blkdev),
		   uk_blkdev_drv_name_get(blkdev),
		   uk_blkdev_state_get(blkdev)
		   );

	rc = uk_blkdev_stop(blkdev);
	if (rc) {
		printf("%d : rc = %d", __LINE__, rc);
		return;
	}
}

void scan_9p()
{
	void *data;
	int fd;
    int ret = mount("share_dir", "/", "9pfs", 0, data);
	printf("mount ret = %d\n", ret);
	fd = creat("/2", O_CREAT);
    printf("creat fd = %d\n", fd);

	close(fd);
    ret = umount2("/", 0);
	printf("umount2 ret = %d\n", ret);
}

int main(int argc __attribute__((unused)),
	 char *argv[] __attribute__((unused)))
{
	int rc = 0;
	int srv, client;
	ssize_t n;
	struct sockaddr_in srv_addr;

	scan_9p();
	/* return 0; */

	init_block_device();

	srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv < 0) {
		fprintf(stderr, "Failed to create socket: %d\n", errno);
		goto out;
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = INADDR_ANY;
	srv_addr.sin_port = htons(LISTEN_PORT);

	rc = bind(srv, (struct sockaddr *) &srv_addr, sizeof(srv_addr));
	if (rc < 0) {
		fprintf(stderr, "Failed to bind socket: %d\n", errno);
		goto out;
	}

	/* Accept one simultaneous connection */
	rc = listen(srv, 1);
	if (rc < 0) {
		fprintf(stderr, "Failed to listen on socket: %d\n", errno);
		goto out;
	}

	memset(replybuf, 0, BUFLEN);
	sprintf(replybuf, reply_format, blkdev_data);
	printf("Listening on port %d...\n", LISTEN_PORT);
	while (1) {
		client = accept(srv, NULL, 0);
		if (client < 0) {
			fprintf(stderr,
				"Failed to accept incoming connection: %d\n",
				errno);
			goto out;
		}

 		/* Receive some bytes (ignore errors) */
		read(client, recvbuf, BUFLEN);

		/* Send reply */
		n = write(client, replybuf, sizeof(replybuf));
		if (n < 0)
			fprintf(stderr, "Failed to send a reply\n");
		else
			printf("Sent a reply\n");

		/* Close connection */
		close(client);
	}

out:
	return rc;
}
