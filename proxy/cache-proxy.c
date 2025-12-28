#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <assert.h>

#include "cache.h"
#include "hashmap.c/hashmap.h"
#include "http-parse.h"
#include "list.h"
#include "network.h"

typedef enum Error { SUCCESS, ERROR_REQUEST_UNSUPPORT } Error;

#define BUFFER_SIZE 8192
#define MAX_BUFFER_PARTS 131072 * 2
#define URL_SIZE 1024
#define MAX_URL_SIZE 1024 * 8
#define MAX_CACHE_SIZE 20
#define PORT 80
#define METHOD_SIZE 4

struct hashmap* cache;
LRUQueue* queue_head;
LRUQueue* queue_tail;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cache_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t cache_available = PTHREAD_COND_INITIALIZER;
size_t cache_size;
int terminate;

typedef struct RemoteServer {
    char* url;
    CacheEntry* entry;
} RemoteServer;

RemoteServer* create_remote_data(CacheEntry* entry, char* url) {
    RemoteServer* remote_data = (RemoteServer*)malloc(sizeof(RemoteServer));
    remote_data->url = url;
    remote_data->entry = entry;

    return remote_data;
}

Error check_request(char* method, int minor_version) {
    if (strcmp(method, "GET")) {
        fprintf(stderr, "[ERROR] Only GET method is supported\n");
        return ERROR_REQUEST_UNSUPPORT;
    }

    return SUCCESS;
}

void* cache_gc_monitor(void* arg) {
    while (!terminate) {
        pthread_mutex_lock(&cache_lock);

        while (cache_size < MAX_CACHE_SIZE && !terminate) {
            pthread_cond_wait(&cache_full, &cache_lock);
        }

        if (terminate) {
            pthread_mutex_unlock(&cache_lock);
            break;
        }

        if (cache_size >= MAX_CACHE_SIZE) {
            CacheEntry* removed_entry = cache_entry_remove(&queue_head, &queue_tail);
            hashmap_delete(cache, &(HashValue){.url = removed_entry->url});
            cache_entry_sub(removed_entry);
            cache_size--;
            printf("[INFO] GC: Evicted cache entry, size now: %zu\n", cache_size);
            pthread_cond_broadcast(&cache_available);
        }

        pthread_mutex_unlock(&cache_lock);
    }

    return NULL;
}

void* handle_remote_server(void* arg) {
    RemoteServer* data = (RemoteServer*)arg;
    char* url = data->url;
    CacheEntry* entry = data->entry;

    int server_sockfd = connect_to_remote_server(url, MAX_URL_SIZE);
    if (server_sockfd == -1) {
        fprintf(stderr, "[ERROR] Unable to connect to a remote server\n");
        cache_entry_sub(entry);
        free(data);

        return NULL;
    }

    char host[MAX_URL_SIZE], path[MAX_URL_SIZE];
    if (sscanf(url, "http://%[^/]%s", host, path) < 1) {
        fprintf(stderr, "[ERROR] Invalid URL format\n");
        cache_entry_sub(entry);
        free(data);
        close(server_sockfd);
        return NULL;
    }

    if (strlen(path) == 0) {
        strcpy(path, "/");
    }

    char http10_request[BUFFER_SIZE];
    int request_len = snprintf(http10_request, BUFFER_SIZE,
                               "GET %s HTTP/1.0\r\n"
                               "Host: %s\r\n"
                               "Connection: close\r\n\r\n",
                               path, host);

    size_t written = 0;
    while (written < request_len) {
        written += write(server_sockfd, http10_request + written,
                         request_len - written);
    }

    size_t response_len = 0;

    HTTP_PARSE parse_err = http_parse_read_response(
        server_sockfd, BUFFER_SIZE, MAX_BUFFER_PARTS, &response_len, entry);
    if (parse_err == PARSE_ERROR) {
        cache_entry_sub(entry);
        free(data);

        close(server_sockfd);
        return NULL;
    }

    pthread_rwlock_wrlock(&entry->lock);
    entry->response_len = response_len;
    int arc = __sync_fetch_and_sub(&entry->arc, 1);
    pthread_rwlock_unlock(&entry->lock);

    if (arc == 1) {
        cache_entry_free(entry);
    }
    free(data);
    return NULL;
}

void* handle_client(void* arg) {
    Error err;

    int client_sockfd = (long)arg;

    int pthread_err;
    pthread_attr_t attr;
    pthread_err = pthread_attr_init(&attr);
    if (pthread_err) {
        close(client_sockfd);
        return NULL;
    }

    pthread_err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_err) {
        close(client_sockfd);
        return NULL;
    }

    size_t request_len = 0;
    char client_request[BUFFER_SIZE];
    char* method;
    char* url;
    int minor_version;

    HTTP_PARSE parse_err;
    parse_err = http_parse_read_request(
        client_sockfd, client_request, BUFFER_SIZE, &method, &url, URL_SIZE,
        MAX_URL_SIZE, &minor_version, &request_len);

    if (parse_err == PARSE_ERROR) {
        fprintf(stderr, "[ERROR] Error occured during http parse\n");
        close(client_sockfd);
        return NULL;
    }

    err = check_request(method, minor_version);
    if (err == ERROR_REQUEST_UNSUPPORT) {
        fprintf(stderr, "[ERROR] Request is unsupported\n");
        close(client_sockfd);
        return NULL;
    }

    printf("[INFO] HTTP GET %s\n", url);

    pthread_mutex_lock(&cache_lock);
    const HashValue* value = hashmap_get(cache, &(HashValue){.url = url});
    CacheEntry* entry;

    if (value == NULL) {
        entry = cache_entry_create(url, BUFFER_SIZE);

        value = &(HashValue){.url = url, .entry = entry};

        while (cache_size >= MAX_CACHE_SIZE) {
            pthread_cond_signal(&cache_full);
            pthread_cond_wait(&cache_available, &cache_lock);
        }

        hashmap_set(cache, value);
        cache_size++;

        cache_entry_add(&queue_head, &queue_tail, entry);

        __sync_fetch_and_add(&entry->arc, 3);

        RemoteServer* remote_data = create_remote_data(entry, url);

        pthread_t tid;
        pthread_err = pthread_create(&tid, &attr, handle_remote_server,
                                     (void*)remote_data);
        if (pthread_err) {
            close(client_sockfd);
            cache_entry_sub(entry);
            cache_entry_sub(entry);
            pthread_mutex_unlock(&cache_lock);
            return NULL;
        }
    } else {
        entry = value->entry;
        __sync_fetch_and_add(&entry->arc, 1);
        cache_entry_upd(&queue_head, &queue_tail, entry);
        printf("[INFO] Reading entry from cache. URL: %s\n", entry->url);
    }

    if (entry->error) {
        cache_entry_sub(entry);
        close(client_sockfd);

        pthread_mutex_unlock(&cache_lock);
        return NULL;
    }

    pthread_mutex_unlock(&cache_lock);

    size_t written = 0;
    int parts_read = 0;
    int amount_to_read = 0;

    pthread_mutex_lock(&entry->wait_lock);
    while (entry->parts_done == 0 && !entry->done) {
        pthread_cond_wait(&entry->new_part, &entry->wait_lock);
    }
    pthread_mutex_unlock(&entry->wait_lock);

    char* buffer[BUFFER_SIZE];
    pthread_rwlock_rdlock(&entry->lock);
    List* node = entry->data;
    amount_to_read = node->buf_len;
    memcpy(buffer, node->buffer, node->buf_len);
    pthread_rwlock_unlock(&entry->lock);

    while (node != NULL) {
        pthread_rwlock_rdlock(&entry->lock);
        if (entry->error) {
            pthread_rwlock_unlock(&entry->lock);
            break;
        }
        pthread_rwlock_unlock(&entry->lock);

        err = write(client_sockfd, buffer + written, amount_to_read - written);
        if (err == -1) {
            fprintf(stderr,
                    "[ERROR] Error during writing response to client: %s\n",
                    strerror(errno));
            cache_entry_sub(entry);
            close(client_sockfd);
            return NULL;
        }
        written += err;

        if (written >= amount_to_read) {
            parts_read++;
            written = 0;

            if (entry->done) {
                pthread_rwlock_rdlock(&entry->lock);
                node = node->next;
                if (!node) {
                    break;
                }
                amount_to_read = node->buf_len;
                memcpy(buffer, node->buffer, amount_to_read);
                pthread_rwlock_unlock(&entry->lock);
            } else {
                pthread_mutex_lock(&entry->wait_lock);
                while (parts_read == entry->parts_done && !entry->done) {
                    pthread_cond_wait(&entry->new_part, &entry->wait_lock);
                }
                pthread_mutex_unlock(&entry->wait_lock);

                pthread_rwlock_rdlock(&entry->lock);
                node = node->next;
                if (!node) {
                    break;
                }
                amount_to_read = node->buf_len;
                memcpy(buffer, node->buffer, amount_to_read);
                pthread_rwlock_unlock(&entry->lock);
            }
        }
    }

    cache_entry_sub(entry);
    close(client_sockfd);
    return NULL;
}

void SIGINT_handler(int signo) {
    if (signo == SIGINT) {
        if (!terminate) {
            terminate = 1;
        } else {
            syscall(SYS_exit_group, 0);
        }
    }
}

int main() {
    int err;
    terminate = 0;

    sig_t sig_error = signal(SIGINT, SIGINT_handler);
    if (sig_error == SIG_ERR) {
        fprintf(stderr, "[ERROR] Unable to set SIGINT handler: %s\n",
                strerror(errno));
    }

    int server_sockfd = create_server_socket_and_listen(PORT);
    if (server_sockfd == -1) {
        printf("[ERROR] Error starting proxy server\n");
        return -1;
    }

    cache = cache_create();
    cache_size = 0;
    queue_head = NULL;

    sig_error = signal(SIGPIPE, SIG_IGN);
    if (sig_error == SIG_ERR) {
        fprintf(stderr, "[ERROR] Unable to ingore SIGPIPE %s\n",
                strerror(errno));
    }

    pthread_attr_t attr;
    err = pthread_attr_init(&attr);
    if (err) {
        return 0;
    }

    err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (err) {
        return 0;
    }

    pthread_t gc_thread;
    err = pthread_create(&gc_thread, &attr, cache_gc_monitor, NULL);
    if (err) {
        fprintf(stderr, "[ERROR] Failed to create GC monitor thread\n");
        return -1;
    }
    printf("[INFO] GC monitor thread started\n");

    while (!terminate) {
        struct sockaddr_in client_addr;
        socklen_t client_len;

        int client_sockfd =
            accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);

        if (client_sockfd < 0) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "[ERROR] Unable to accept new client");
            continue;
        }

        printf("[INFO] New client\n");

        pthread_t client_thread;

        err = pthread_create(&client_thread, &attr, handle_client,
                             (void*)(long)client_sockfd);
        if (err) {
            fprintf(stderr, "[ERROR] pthread_create error\n");
            close(client_sockfd);
            continue;
        }
    }

    pthread_mutex_lock(&cache_lock);
    pthread_cond_broadcast(&cache_full);
    pthread_mutex_unlock(&cache_lock);

    pthread_exit(NULL);
}
