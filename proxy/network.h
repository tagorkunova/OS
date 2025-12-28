#ifndef NETWORK_H
#define NETWORK_H

int create_server_socket_and_listen(int port);

int connect_to_remote_server(char* url, const int max_url_size);

#endif
