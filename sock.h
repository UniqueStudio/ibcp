//License: Apache License 2.0
//See LICENSE for details
//Authors (in alphabet order of last name):
// - Qijiang Fan <fqj1994@gmail.com>
// - Bin He <binhe22@gmail.com>
// - Cheng Zhang <chengzhang@hustunique.com>
// - Shiwei Zhou <shwzhou@hustunique.com>
#ifndef SOCK_H
#define SOCK_H

extern int sock_daemon_connect(
	int port);

extern int sock_client_connect(
	const char *server_name,
	int port);

extern int sock_sync_data(
	int sock_fd,
	int is_daemon,
	size_t size,
	const void *out_buf,
	void *in_buf);

extern int sock_sync_ready(
	int sock_fd,
	int is_daemon);

#endif /* SOCK_H */
