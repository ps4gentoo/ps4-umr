
#include "umr_rumr.h"
#include <sys/socket.h>
#include <netinet/in.h>

/** TCP implementation
 * So far fairly basic.  Only supports IPv4 and
 * only by IP address (no by name).
 */
struct tcp_state {
	int sock, con_sock;
};

// convert a string to sockaddr_in structure
static struct sockaddr_in addr_to_sin4(char *addr)
{
	uint8_t ip4[4];
	uint16_t port;
	struct sockaddr_in sin;
	uint32_t *ip = (uint32_t *)&ip4[0];

	memset(&sin, 0, sizeof sin);
	if (sscanf(addr, "%"SCNu8".%"SCNu8".%"SCNu8".%"SCNu8":%"SCNu16,
				&ip4[3], &ip4[2], &ip4[1], &ip4[0], &port) == 5) {
		// we have an IP:port combo
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = htonl(*ip);
	} else {
		// try domain name
	}
	return sin;
}

// connect a client to a server
static int tcp_connect(struct rumr_comm_funcs *cf, char *server)
{
	struct sockaddr_in sin;
	struct tcp_state *ts;

	ts = cf->data = calloc(1, sizeof *ts);
	ts->sock = -1;

	ts = cf->data;
	sin = addr_to_sin4(server);
	if (sin.sin_family == 0) {
		cf->log_msg("[ERROR]: Could not translate IP address\n");
		return -1;
	}

	// create new socket
	ts->con_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ts->con_sock < 0) {
		cf->log_msg("[ERROR]: Could not create socket\n");
		free(ts);
		cf->data = NULL;
		return -1;
	}

	// connect to host
	if (connect(ts->con_sock, (const struct sockaddr *)&sin, sizeof sin) < 0) {
		cf->log_msg("[ERROR]: Could not connect to server\n");
		close(ts->con_sock);
		free(ts);
		cf->data = NULL;
		return -1;
	}

	cf->log_msg("[VERBOSE]: Connected to %s\n", server);

	return 0;
}

// bind to a host address
static int tcp_bind(struct rumr_comm_funcs *cf, char *host)
{
	struct sockaddr_in sin;
	struct tcp_state *ts;

	cf->data = calloc(1, sizeof *ts);
	ts = cf->data;
	sin = addr_to_sin4(host);
	if (sin.sin_family == 0) {
		cf->log_msg("[ERROR]: Could not translate IP address\n");
		return -1;
	}

	// create new socket
	ts->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (ts->sock < 0) {
		cf->log_msg("[ERROR]: Could not allocate a socket\n");
		free(ts);
		cf->data = NULL;
		return -1;
	}

	// bind to host
	if (bind(ts->sock, (const struct sockaddr *)&sin, sizeof sin) < 0) {
		cf->log_msg("[ERROR]: Could not bind socket\n");
		close(ts->sock);
		free(ts);
		cf->data = NULL;
		return -1;
	}

	// now listen
	if (listen(ts->sock, 1) < 0) {
		cf->log_msg("[ERROR]: Could not listen\n");
		close(ts->sock);
		free(ts);
		cf->data = NULL;
		return -1;
	}

	// assign -1 to con_sock so we know it's closed
	ts->con_sock = -1;

	cf->log_msg("[VERBOSE]: Bound to %s\n", host);

	return 0;
}

// wait (blocking) for a new client connection
static int tcp_accept(struct rumr_comm_funcs *cf)
{
	struct tcp_state *ts = cf->data;
	struct sockaddr_in sin;
	socklen_t sinlen;
	uint8_t *ip4 = (uint8_t *)&sin.sin_addr.s_addr;

	sinlen = sizeof sin;
	ts->con_sock = accept(ts->sock, (struct sockaddr *)&sin, &sinlen);
	if (ts->con_sock < 0) {
		return -1;
	}

	sin.sin_addr.s_addr = ntohl(sin.sin_addr.s_addr);
	cf->log_msg("[VERBOSE]: Accepted connection from %"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8":%"PRIu16"\n",
		    ip4[3], ip4[2], ip4[1], ip4[0], ntohs(sin.sin_port));

	return 0;
}


// this TCP layer wraps each packet with 8 bytes made up of
// RUMR
// payload len (in bytes)
static int tcp_tx(struct rumr_comm_funcs *cf, struct rumr_buffer *buf)
{
	struct tcp_state *ts = cf->data;
	uint32_t len = buf->woffset;
	uint8_t *ptr = buf->data - RUMR_BUFFER_PREHEADER;

	ptr[0] = 'R';
	ptr[1] = 'U';
	ptr[2] = 'M';
	ptr[3] = 'R';
	ptr[4] = (len) & 0xFF;
	ptr[5] = (len>>8) & 0xFF;
	ptr[6] = (len>>16) & 0xFF;
	ptr[7] = (len>>24) & 0xFF;

	// we prefix buffers so we can use one send() to send
	// both our TCP header and the RUMR payload
	// a single send() call greatly speeds up the RX side
	if (send(ts->con_sock, buf->data-8, len+8, 0) != (len+8)) {
		return -1;
	}
	return 0;
}

static int tcp_rx(struct rumr_comm_funcs *cf, struct rumr_buffer **buf)
{
	struct tcp_state *ts = cf->data;
	uint8_t hdr[8];
	uint32_t len;

	*buf = NULL;

	// receive the TCP header
	// note that even though we have two recv() calls only
	// the first one blocks waiting for 8 bytes, because
	// we TX in a single send() by time this passes
	// the next recv() has (at least some) contents to read
	if (recv(ts->con_sock, hdr, 8, MSG_WAITALL) != 8) {
		return -1;
	}

	if (memcmp(hdr, "RUMR", 4)) {
		return -1;
	}
	len =	((uint32_t)hdr[4]) | ((uint32_t)hdr[5] << 8) |
		((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);

	*buf = rumr_buffer_init();
	if (!*buf) {
		return -1;
	}
	(*buf)->data = (uint8_t*)calloc(1, len + RUMR_BUFFER_PREHEADER) + RUMR_BUFFER_PREHEADER;
	(*buf)->woffset = len;

	if (recv(ts->con_sock, (*buf)->data, len, MSG_WAITALL) != len) {
		rumr_buffer_free(*buf);
		*buf = NULL;
		return -1;
	}

	return 0;
}

// close down *all* sockets
static int tcp_close(struct rumr_comm_funcs *cf)
{
	struct tcp_state *ts = cf->data;
	cf->log_msg("[VERBOSE]: Shutting down sockets\n");
	if (ts->con_sock >= 0) {
		close(ts->con_sock);
	}
	if (ts->sock >= 0) {
		close(ts->sock);
	}
	free(cf->data);
	cf->data = NULL;
	return 0;
}

// only close the client socket
static int tcp_closeclient(struct rumr_comm_funcs *cf)
{
	struct tcp_state *ts = cf->data;
	cf->log_msg("[VERBOSE]: Closing client connection\n");
	if (ts->con_sock >= 0) {
		close(ts->con_sock);
		ts->con_sock = -1;
	}
	return 0;
}

static int tcp_status(struct rumr_comm_funcs *cf)
{
	(void)cf;
	return 0;
}

const struct rumr_comm_funcs rumr_tcp_funcs = {
	NULL,
	&tcp_connect,
	&tcp_bind,
	&tcp_accept,
	&tcp_tx,
	&tcp_rx,
	&tcp_close,
	&tcp_closeclient,
	&tcp_status,
	NULL
};


#ifdef DEMO

void server(void)
{
	struct rumr_comm_funcs cf;
	struct rumr_buffer *buf;
	uint32_t x, n;

	cf.log_msg = printf;

	printf("server> binding\n");
	if (tcp_bind(&cf, "127.0.0.0:9000"))
		return;
	printf("server> accepting\n");
	if (tcp_accept(&cf))
		return;
	printf("server> accepted\n");

	for (x = 1; x < 100000; x++) {
		printf(">server transmitting buffer %"PRIu32"\n", x);
		buf = rumr_buffer_init();
		for (n = 0; n < x; n++) {
			rumr_buffer_add_uint32(buf, n);
		}
		tcp_tx(&cf, buf);
		rumr_buffer_free(buf);
	}
	usleep(5000000ULL);
}

void client(void)
{
	struct rumr_comm_funcs cf;
	struct rumr_buffer *buf;
	uint32_t x, n, *dat;

	cf.log_msg = printf;

	printf("client> connecting\n");
	if (tcp_connect(&cf, "127.0.0.0:9000"))
		return;
	printf("client> connected\n");

	for (x = 1; x < 100000; x++) {
		tcp_rx(&cf, &buf);
		dat = (uint32_t *)buf->data;
		if (buf->woffset != x*4) {
			printf("client> wrong size of buffer expected %"PRIu32" but got %"PRIu32"\n", x*4, buf->woffset);
			return;
		}
		for (n = 0; n < x; n++) {
			if (dat[n] != n) {
				printf("client> wrong buffer contents expected %"PRIu32" but got %"PRIu32"\n", n, dat[n]);
				return;
			}
		}
		printf("client> buffer of length %"PRIu32" was ok.\n", x);
		rumr_buffer_free(buf);
	}
	tcp_close(&cf);
}

int main(void)
{
	if (fork()) {
		server();
		return EXIT_SUCCESS;
	}
	usleep(1000000UL);
	client();
}

#endif
