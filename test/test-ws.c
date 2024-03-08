/*
 * Copyright (c) 2002-2007 Niels Provos <provos@citi.umich.edu>
 * Copyright (c) 2007-2012 Niels Provos and Nick Mathewson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "util-internal.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#endif

#include "event2/event-config.h"

#include <sys/types.h>
#include <sys/stat.h>
#ifdef EVENT__HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/queue.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#endif
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "event2/event.h"
#include "event2/http.h"
#include "event2/buffer.h"
#include "event2/bufferevent.h"
#include "event2/bufferevent_ssl.h"
#include "event2/util.h"
#include "event2/listener.h"
#include "event2/ws.h"
#include "log-internal.h"
#include "http-internal.h"

#define CLIENT_MSG "hello from client"
#define SERVER_MSG "hello from server"

static char client_buf[32];
static char server_buf[32];
static struct event_base *base;
static struct evhttp *http_server;
static struct bufferevent *bev;
static struct evws_connection *client, *server;

static void
on_server_msg(struct evws_connection *conn, int type, const unsigned char *buf,
	size_t size, void *arg)
{
	strncpy(server_buf, (const char *)buf, size);
	event_base_loopbreak(base);
}

static void
on_client_msg(struct evws_connection *conn, int type, const unsigned char *buf,
	size_t size, void *arg)
{
	strncpy(client_buf, (const char *)buf, size);
	evws_send_text(conn, CLIENT_MSG);
}

static void
on_ws(struct evhttp_request *req, void *arg)
{
	server = evws_new_session(req, on_server_msg, NULL, 0);
	evws_send_text(server, SERVER_MSG);
}

int
main(int argc, char **argv)
{
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(2, 2);

	(void)WSAStartup(wVersionRequested, &wsaData);
#endif
	base = event_base_new();

	// server setup
	http_server = evhttp_new(base);
	evhttp_bind_socket_with_handle(http_server, "0.0.0.0", 12345);
	evhttp_set_cb(http_server, "/ws", on_ws, NULL);
	// client setup
	bev = bufferevent_socket_new(base, -1, BEV_OPT_DEFER_CALLBACKS);
	bufferevent_socket_connect_hostname(bev,NULL,AF_INET,"127.0.0.1",12345);
	client = evws_connect(
		bev, "ws://127.0.0.1:12345/ws", on_client_msg, NULL, 0);
	event_base_dispatch(base);
	if (strcmp(client_buf, SERVER_MSG) != 0) {
		return 1;
	}

	if (strcmp(server_buf, CLIENT_MSG) != 0) {
		return 1;
	}
	return 0;
}