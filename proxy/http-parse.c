#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cache.h"
#include "http-parse.h"
#include "list.h"
#include "picohttpparser/picohttpparser.h"

HTTP_PARSE http_parse_read_request(int client_sockfd, char* client_request,
                                   int request_size, char** ret_method,
                                   char** ret_url, const int url_size,
                                   const int max_url_size, int* minor_version,
                                   size_t* buflen) {
    const char* method;
    const char* url;
    struct phr_header headers[100];
    size_t prevbuflen = 0, num_headers, method_len, url_len;
    int pret;

    ssize_t rret;

    while (1) {
        while ((rret = read(client_sockfd, client_request + *buflen,
                            request_size - *buflen)) == -1 &&
               errno == EINTR)
            ;

        if (rret <= 0) {
            fprintf(stderr, "[ERROR] Error reading from client\n");
            return PARSE_ERROR;
        }

        prevbuflen = *buflen;
        *buflen += rret;

        num_headers = sizeof(headers) / sizeof(headers[0]);
        pret = phr_parse_request(client_request, *buflen, &method, &method_len,
                                 &url, &url_len, minor_version, headers,
                                 &num_headers, prevbuflen);

        if (pret > 0) {
            break;
        }

        if (pret != -2) {
            fprintf(stderr, "[ERROR] Error parsing client request\n");
            return PARSE_ERROR;
        }

        if (*buflen == request_size) {
            fprintf(stderr, "[ERROR] Client request is too long\n");
            return PARSE_ERROR;
        }
    }

    if (url_len > max_url_size) {
        fprintf(stderr, "[ERROR] URL is too long\n");
        return PARSE_ERROR;
    }

    if (url_len > url_size) {
        *ret_url = realloc(*ret_url, sizeof(char) * url_len);
    }

    *ret_url = strndup(url, url_len);
    *ret_method = strndup(method, method_len);

    return PARSE_SUCCESS;
}

HTTP_PARSE http_parse_read_response(int server_sockfd,
                                    const size_t max_chunk_size,
                                    const int max_buffer_parts, size_t* buflen,
                                    CacheEntry* entry) {
    const char* msg;
    struct phr_header headers[100];
    size_t prevbuflen = 0, msg_len, num_headers;
    int minor_version, status, pret;
    ssize_t rret;

    size_t chunk_size = max_chunk_size;
    size_t chunk_len = *buflen;
    List* node = entry->data;

    char buffer[chunk_size];

    while (1) {
        while ((rret = read(server_sockfd, buffer + chunk_len,
                            chunk_size - chunk_len)) == -1 &&
               errno == EINTR)
            ;

        if (rret <= 0) {
            return PARSE_ERROR;
        }

        prevbuflen = *buflen;
        *buflen += rret;
        chunk_len += rret;

        if (chunk_len == chunk_size) {
            pthread_rwlock_wrlock(&entry->lock);

            if (entry->parts_done == max_buffer_parts) {
                entry->error = true;

                pthread_rwlock_unlock(&entry->lock);

                break;
            }

            memcpy(node->buffer, buffer, chunk_len);
            node->buf_len = chunk_len;
            entry->parts_done += 1;

            list_add_node(node, chunk_size);
            node = node->next;
            pthread_cond_broadcast(&entry->new_part);

            pthread_rwlock_unlock(&entry->lock);
            chunk_len = 0;
        }

        num_headers = sizeof(headers) / sizeof(headers[0]);

        pret =
            phr_parse_response(buffer, *buflen, &minor_version, &status, &msg,
                               &msg_len, headers, &num_headers, prevbuflen);

        if (pret > 0) {
            pthread_rwlock_wrlock(&entry->lock);

            if (status != 200) {
                entry->error = true;
                pthread_rwlock_unlock(&entry->lock);
                fprintf(stderr, "[ERROR] Non-200 status code: %d\n", status);
                return PARSE_ERROR;
            }

            memcpy(node->buffer, buffer, chunk_len);
            node->buf_len = chunk_len;

            pthread_rwlock_unlock(&entry->lock);
            break;
        } else if (pret == -1) {
            fprintf(stderr, "[ERROR] Error parsing server response header\n");
            return PARSE_ERROR;
        }

        if (pret != -2) {
            fprintf(stderr, "[ERROR] Error parsing server response header\n");
            return PARSE_ERROR;
        }
    }

    int content_length = -1;
    for (int i = 0; i < num_headers; i++) {
        if (strncmp(headers[i].name, "Content-Length", headers[i].name_len)) {
            continue;
        }

        content_length = atoi(headers[i].value);
        break;
    }

    if (content_length + pret > chunk_size * max_buffer_parts) {
        entry->error = true;
    }

    rret = 0;
    int offset = (content_length + pret) % chunk_size;
    while (content_length == -1 || *buflen < content_length + pret) {
        pthread_rwlock_rdlock(&entry->lock);
        if (entry->error) {
            pthread_rwlock_unlock(&entry->lock);
            break;
        }
        pthread_rwlock_unlock(&entry->lock);

        rret = read(server_sockfd, buffer + chunk_len, chunk_size - chunk_len);

        if (rret == -1) {
            fprintf(stderr, "[ERROR] Read response error\n");
            return PARSE_ERROR;
        }

        if (rret == 0) {
            pthread_rwlock_wrlock(&entry->lock);
            if (entry->parts_done == max_buffer_parts) {
                entry->error = true;

            } else {

                memcpy(node->buffer, buffer, chunk_len);
                __sync_fetch_and_add(&entry->parts_done, 1);
                pthread_cond_broadcast(&entry->new_part);
            }

            if (content_length != *buflen - pret && content_length != -1) {
                entry->error = true;
            }

            pthread_rwlock_unlock(&entry->lock);

            break;
        }

        *buflen += rret;
        chunk_len += rret;

        if (chunk_len >= chunk_size) {
            pthread_rwlock_wrlock(&entry->lock);

            if (entry->parts_done == max_buffer_parts) {
                entry->error = true;

                pthread_rwlock_unlock(&entry->lock);

                break;
            }

            memcpy(node->buffer, buffer, chunk_len);
            __sync_fetch_and_add(&entry->parts_done, 1);
            pthread_cond_broadcast(&entry->new_part);
            node->buf_len = chunk_len;

            list_add_node(node, chunk_size);
            node = node->next;

            pthread_rwlock_unlock(&entry->lock);

            chunk_len = 0;
            if (content_length + pret - *buflen <= chunk_size) {
                chunk_size = offset;
            }
        }
    }

    pthread_rwlock_wrlock(&entry->lock);
    entry->done = true;
    pthread_rwlock_unlock(&entry->lock);
    pthread_cond_broadcast(&entry->new_part);

    return PARSE_SUCCESS;
}
