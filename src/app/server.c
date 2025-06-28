/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include "parson.h"

extern JSON_Value *umr_process_json_request(JSON_Object *request, void **raw_data, unsigned *raw_data_size);
extern void init_asics(void);
extern struct umr_asic *asics[16];

void run_server_loop(const char *url, struct umr_asic * asic)
{
	int sock = nn_socket(AF_SP, NN_REP);
	if (sock < 0) {
		exit(1);
	}

	int rv = nn_bind(sock, url);
	if (rv < 0) {
		exit(1);
	}

	int size = 100000000;
	if (nn_setsockopt(sock, NN_SOL_SOCKET, NN_RCVMAXSIZE, &size, sizeof(size)) < 0) {
		exit(0);
	}

	if (asic) {
		asics[0] = asic;
	} else {
		init_asics();
	}

	/* Everything is ready. Wait for commands */

	printf("Waiting for commands.\n");
	for (;;) {
		char* buf;
		int len = nn_recv(sock, &buf, NN_MSG, 0);
		if (len < 0)
			exit(0);
		else if (len == 0)
			continue;

		buf[len - 1] = '\0';
		JSON_Value *request = json_parse_string(buf);

		if (request == NULL) {
			printf("ERROR\n");
		} else {
			void *raw_data = NULL;
			unsigned raw_data_size = 0;
			JSON_Value *answer = umr_process_json_request(
				json_object(request), &raw_data, &raw_data_size);

			char* s = json_serialize_to_string(answer);
			size_t len = strlen(s) + 1;

			/* We can only send a single reply because of the nn protocol used,
			 * so pack everything.
			 */
			uint8_t *msg = nn_allocmsg(sizeof(uint32_t) + len + raw_data_size, 0);

			memcpy(msg, &raw_data_size, sizeof(uint32_t));
			memcpy(&msg[sizeof(uint32_t)], s, len);
			memcpy(&msg[sizeof(uint32_t) + len], raw_data, raw_data_size);

			if (nn_send(sock, &msg, NN_MSG, 0) < 0)
				exit(0);

			json_free_serialized_string(s);
			json_value_free(answer);
			free(raw_data);
		}
		nn_freemsg(buf);
	}
}
