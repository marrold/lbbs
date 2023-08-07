/*
 * LBBS -- The Lightweight Bulletin Board System
 *
 * Copyright (C) 2023, Naveen Albert
 *
 * Naveen Albert <bbs@phreaknet.org>
 *
 */

/*! \file
 *
 * \brief Socket functions
 *
 * \author Naveen Albert <bbs@phreaknet.org>
 */

/*!
 * \brief Create a UNIX domain socket
 * \param sock Pointer to socket
 * \param sockfile Socket file path
 * \param perm Permissions for socket
 * \param uid User ID. -1 to not change.
 * \param gid Group ID. -1 to not change.
 * \retval 0 on success, -1 on failure
 */
int bbs_make_unix_socket(int *sock, const char *sockfile, const char *perm, uid_t uid, gid_t gid);

/*!
 * \brief Create a TCP socket
 * \param sock Pointer to socket
 * \param port Port number on which to create the socket
 * \retval 0 on success, -1 on failure
 */
int bbs_make_tcp_socket(int *sock, int port);

/*! \brief Put a socket in nonblocking mode */
int bbs_unblock_fd(int fd);

/*! \brief Put a socket in blocking mode */
int bbs_block_fd(int fd);

/*!
 * \brief Check whether a given hostname has an A record for a particular IP address
 * \param hostname Hostname to check
 * \param ip IP address for which to check
 * \retval 1 if there is a match, 0 if there are no matches
 */
int bbs_hostname_has_ip(const char *hostname, const char *ip);

/*!
 * \brief Resolve a hostname to an IP address
 * \param hostname Hostname or IP address
 * \param[out] buf IP address
 * \param[out] len Size of buf.
 * \retval -1 on failure, 0 on success
 */
int bbs_resolve_hostname(const char *hostname, char *buf, size_t len);

/*!
 * \brief Open a TCP socket to another server
 * \param hostname DNS hostname of server
 * \param port Destination port number
 * \retval -1 on failure, socket file descriptor otherwise
 * \note This does not perform TLS negotiation, use ssl_client_new immediately or later in the session for encryption.
 */
int bbs_tcp_connect(const char *hostname, int port);

/*!
 * \brief Wrapper around accept(), with poll timeout
 * \param socket Socket fd
 * \param ms poll time in ms
 * \param ip Optional IP restriction. NULL to allow any IP address.
 * \retval -1 on failure, socket file descriptor otherwise
 */
int bbs_timed_accept(int socket, int ms, const char *ip);

/*!
 * \brief Cleanly shutdown and close a socket
 * \param socket Pointer to socket fd
 */
void bbs_socket_close(int *socket);

/*!
 * \brief Cleanly shutdown and close a socket and an associated listening thread
 * \param socket Pointer to socket fd
 * \param thread
 */
void bbs_socket_thread_shutdown(int *socket, pthread_t thread);

/*!
 * \brief Check whether a socket has been closed by the remote peer, without reading from it
 * \param fd
 * \retval 1 if closed, 0 if no activity
 */
int bbs_socket_pending_shutdown(int fd);

/*!
 * \brief Listen on a TCP socket
 * \param port TCP port number
 * \param name Name of network service
 * \param handler Handler to execute to handle nodes spawned by this listener
 * \retval 0 on success, -1 on failure
 */
#define bbs_start_tcp_listener(port, name, handler) __bbs_start_tcp_listener(port, name, handler, BBS_MODULE_SELF)

int __bbs_start_tcp_listener(int port, const char *name, void *(*handler)(void *varg), void *module);

/*! \brief Same as bbs_start_tcp_listener but, like bbs_tcp_listener3, for multiple TCP listeners at once */
#define bbs_start_tcp_listener3(port, port2, port3, name, name2, name3, handler) __bbs_start_tcp_listener3(port, port2, port3, name, name2, name3, handler, BBS_MODULE_SELF)

int __bbs_start_tcp_listener3(int port, int port2, int port3, const char *name, const char *name2, const char *name3, void *(*handler)(void *varg), void *module);

/*!
 * \brief Stop a TCP listener registered previously using bbs_start_tcp_listener
 * \param port TCP port number
 * \retval 0 on success, -1 on failure
 * \note This does not close the socket
 */
int bbs_stop_tcp_listener(int port);

/*!
 * \brief Run a terminal services TCP network login service listener thread
 * \param socket Socket fd
 * \param name Name of network login service, e.g. Telnet, RLogin, etc.
 * \param handshake Handshake callback function. It should return 0 to proceed and -1 to abort.
 * \param module Module reference
 */
void bbs_tcp_comm_listener(int socket, const char *name, int (*handshake)(struct bbs_node *node), void *module);

/*!
 * \brief Run a generic TCP network login service listener thread
 * \param socket Socket fd
 * \param name Name of network login service, e.g. Telnet, RLogin, etc.
 * \param handler Service handler function
 * \param module Module reference
 */
void bbs_tcp_listener(int socket, const char *name, void *(*handler)(void *varg), void *module);

/*!
 * \brief Run a generic TCP network login service listener thread for up to 2 sockets
 * \param socket Socket fd (typically the insecure socket). -1 if not needed.
 * \param socket2 Optional 2nd fd (typically the secure socket). -1 if not needed.
 * \param name Name of network login service corresponding to socket
 * \param name2 Name of network login service corresponding to socket2
 * \param handler Common service handler function (for both sockets)
 * \param module Module reference
 */
void bbs_tcp_listener2(int socket, int socket2, const char *name, const char *name2, void *(*handler)(void *varg), void *module);

/*!
 * \brief Run a generic TCP network login service listener thread for up to 3 sockets
 * \param socket Socket fd (typically the insecure socket). -1 if not needed.
 * \param socket2 Optional 2nd fd (typically the secure socket). -1 if not needed.
 * \param socket3 Optional 3rd fd. -1 if not needed.
 * \param name Name of network login service corresponding to socket
 * \param name2 Name of network login service corresponding to socket2
 * \param name3 Name of network login service corresponding to socket3
 * \param handler Common service handler function (for all sockets)
 * \param module Module reference
 */
void bbs_tcp_listener3(int socket, int socket2, int socket3, const char *name, const char *name2, const char *name3, void *(*handler)(void *varg), void *module);

/*!
 * \brief Get local IP address
 * \param buf
 * \param len
 * \retval 0 on success, -1 on failure
 */
int bbs_get_local_ip(char *buf, size_t len);

/*!
 * \brief Get the hostname of an IP address
 * \param ip IP address
 * \param[out] buf
 * \param len Size of buf
 * \retval 0 on success, -1 on failure
 * \note If no hostname is determinable, the IP address may be returned and this will count as success.
 */
int bbs_get_hostname(const char *ip, char *buf, size_t len);

/*!
 * \brief Get remote IP address
 * \param sinaddr
 * \param buf
 * \param len
 * \retval 0 on success, -1 on failure
 */
int bbs_get_remote_ip(struct sockaddr_in *sinaddr, char *buf, size_t len);

/*!
 * \brief Get remote IP address, from a file descriptor
 * \param fd
 * \param[out] buf
 * \param len
 * \retval 0 on success, -1 on failure
*/
int bbs_get_fd_ip(int fd, char *buf, size_t len);

/*!
 * \brief Save remote IP address
 * \param sinaddr
 * \param node
 * \retval 0 on success, -1 on failure
 */
int bbs_save_remote_ip(struct sockaddr_in *sinaddr, struct bbs_node *node);

/*! \brief Check whether a hostname is an IPv4 address */
int bbs_hostname_is_ipv4(const char *hostname);

/*!
 * \brief Check if an IP address is within a specified CIDR range
 * \param ip IP address to check, e.g. 192.168.1.1
 * \param cidr CIDR range, e.g. 192.168.1.1/24
 * \retval 1 if in range, 0 if error or not in range
 */
int bbs_cidr_match_ipv4(const char *ip, const char *cidr);

/*!
 * \brief Check if an IP address matches an IP address, CIDR range, or hostname
 * \param ip IP address to check, e.g. 192.168.1.1
 * \param s IPv4 address, IPv4 CIDR range, or hostname (not recommended, since it will only match one of the returned IPs, if multiple)
 * \retval 1 if IP address matches, 0 if not
 */
int bbs_ip_match_ipv4(const char *ip, const char *s);

/*! \brief Get the name of a poll revent */
const char *poll_revent_name(int revents);
