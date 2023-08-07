#define _DEFAULT_SOURCE

#include <stdio.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <signal.h>
#include <threads.h>
#include "inet_socket.h"
#include "client_list.h"
#include "message.h"

typedef struct file_info_st File_info;

struct file_info_st
{
    int fd;
    char filename[UCHAR_MAX];
    long size;
};

unsigned long num_received_messages = 0;

Client_list *gclient_list;

mtx_t num_received_messages_mutex;

mtx_t client_list_mutex;

static void send_to_all(Client_list *client_list, char *buf, int size, int fd)
{
    int max_clients = client_list->max_clients;
    Client_info *clients = client_list->clients;
    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].fd != -1 && strlen(clients[i].name) != 0 && clients[i].fd != fd)
        {
            if (send(clients[i].fd, buf, size, 0) == -1)
            {
                perror("send()");
            }
        }
    }
}

int send_file(Client_info *client_info, char *filename)
{
    Message *message = malloc(sizeof(Message));
    char *buf = malloc(BUF_SIZE);
    int num_message_write;

    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1)
    {
        sprintf(message->data, "%s not found.", filename);
        message->type = 0;
        message->size = strlen(message->data) + 1;
        num_message_write = message_write(buf, message);
        mtx_lock(&client_list_mutex);
        if (send(client_info->fd, buf, BUF_SIZE, 0) == -1)
        {
            perror("send()");
            return -1;
        }
        mtx_unlock(&client_list_mutex);

        perror("open()");
        return -1;
    }

    sprintf(message->data, "%s", filename);
    message->type = 1;
    message->option = 0;
    message->size = strlen(message->data) + 1;
    num_message_write = message_write(buf, message);
    mtx_lock(&client_list_mutex);
    if (send(client_info->fd, buf, BUF_SIZE, 0) == -1)
    {
        perror("send()");
        return -1;
    }
    mtx_unlock(&client_list_mutex);

    struct stat stat_buf;
    if (stat(filename, &stat_buf) == -1)
    {
        perror("stat()");
        return -1;
    }

    sprintf(message->data, "%ld", stat_buf.st_size);
    message->size = strlen(message->data) + 1;
    message->type = 1;
    message->option = 1;
    num_message_write = message_write(buf, message);
    mtx_lock(&client_list_mutex);
    if (send(client_info->fd, buf, BUF_SIZE, 0) == -1)
    {
        perror("send()");
        return -1;
    }
    mtx_unlock(&client_list_mutex);

    while (1)
    {
        message->size = read(file_fd, message->data, DATA_SIZE);
        if (message->size == -1)
        {
            perror("read()");
            return -1;
        }
        else if (message->size == 0)
        {
            break;
        }

        message->type = 1;
        message->option = 2;
        num_message_write = message_write(buf, message);
        mtx_lock(&client_list_mutex);
        if (send(client_info->fd, buf, BUF_SIZE, 0) == -1)
        {
            perror("send()");
            return -1;
        }
        mtx_unlock(&client_list_mutex);

        if (message->size < DATA_SIZE)
        {
            break;
        }
    }

    close(file_fd);
    return 0;
}

void handle_file_message(Message *message, File_info *file_info, long *file_received_bytes)
{
    switch (message->option)
    {
    case 0:
    {
        char *path = malloc(message->size);
        strcpy(path, message->data);
        *file_received_bytes = 0;
        file_info->fd = open(path, O_WRONLY | O_TRUNC | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
        if (file_info->fd == -1)
        {
            perror("open()");
        }
        strcpy(file_info->filename, path);
    }
    break;

    case 1:
    {
        file_info->size = atol(message->data);
    }
    break;

    case 2:
    {
        if (write(file_info->fd, message->data, message->size) == -1)
        {
            perror("write()");
            break;
        }
        *file_received_bytes += message->size;
        printf("Received %lu/%lu bytes\n", *file_received_bytes, file_info->size);
        if (*file_received_bytes == file_info->size)
        {
            close(file_info->fd);
        }
    }

    default:
        break;
    }
}

static void handle_text_message(Message *message, Client_info *client_info, char *buf)
{
    switch (message->broadcast)
    {
    case 0:
    {
        int buf_size = message_write(buf, message);
        mtx_lock(&client_list_mutex);
        if (send(client_info->fd, buf, BUF_SIZE, 0) == -1)
        {
            perror("send()");
        }
        mtx_unlock(&client_list_mutex);
    }
    break;

    case 1:
    {
        char *tmp = malloc(BUF_SIZE);
        sprintf(tmp, "%s", message->data);
        sprintf(message->data, "%s: %s", client_info->name, tmp);
        message->size = strlen(message->data) + 1;
        int buf_size = message_write(buf, message);
        mtx_lock(&client_list_mutex);
        send_to_all(gclient_list, buf, BUF_SIZE, -1);
        mtx_unlock(&client_list_mutex);
        free(tmp);
    }
    break;

    default:
        break;
    }
}

static int handle_connection(void *arg)
{
    Client_info *client_info = (Client_info *)arg;
    char *buf = malloc(BUF_SIZE);
    Message *message = malloc(sizeof(Message));
    int num_write_message;

    File_info *file_info = malloc(sizeof(File_info));
    file_info->fd = -1;
    strcpy(file_info->filename, "");
    file_info->size = 0;

    long file_received_bytes = 0;

    // Receive name
    int num_receive = recv(client_info->fd, buf, BUF_SIZE, 0);
    if (num_receive == -1)
    {
        perror("recv()");
        return -1;
    }
    if (num_receive == 0)
    {
        printf("Client %d closed.\n", client_info->fd);
        close(client_info->fd);
        client_info->fd = -1;
        free(buf);
        free(message);
        return -1;
    }

    mtx_lock(&num_received_messages_mutex);
    ++num_received_messages;
    printf("%lu. Message from client %d.\n", num_received_messages, client_info->fd);
    mtx_unlock(&num_received_messages_mutex);

    message_read(message, buf, num_receive);
    mtx_lock(&client_list_mutex);
    strcpy(client_info->name, message->data);
    client_list_update(gclient_list, client_info->fd, client_info->name);
    mtx_unlock(&client_list_mutex);

    sprintf(message->data, "%s has joined the group.", client_info->name);
    message->type = 0;
    message->size = strlen(message->data) + 1;
    message->broadcast = 1;
    num_write_message = message_write(buf, message);
    printf("message->data = %s\n", message->data);
    mtx_lock(&client_list_mutex);
    send_to_all(gclient_list, buf, BUF_SIZE, -1);
    mtx_unlock(&client_list_mutex);

    while (1)
    {
        num_receive = recv(client_info->fd, buf, BUF_SIZE, 0);
        if (num_receive == -1)
        {
            perror("recv");
            break;
        }
        if (num_receive == 0)
        {
            printf("Client %d closed.\n", client_info->fd);
            sprintf(message->data, "%s has left.", client_info->name);
            message->type = 0;
            message->size = strlen(message->data) + 1;
            num_write_message = message_write(buf, message);
            send_to_all(gclient_list, buf, BUF_SIZE, client_info->fd);
            break;
        }

        mtx_lock(&num_received_messages_mutex);
        ++num_received_messages;
        printf("%lu. Message from client %d.\n", num_received_messages, client_info->fd);
        mtx_unlock(&num_received_messages_mutex);

        // printf("buf = %s\n", buf);

        message_read(message, buf, num_receive);
        // printf("message->data = %s\n", message->data);

        switch (message->type)
        {
        case 0:
        {
            handle_text_message(message, client_info, buf);
        }
        break;

        case 1:
        {
            handle_file_message(message, file_info, &file_received_bytes);
            if (file_info->size == file_received_bytes && message->option == 2)
            {
                sprintf(message->data, "%s has uploaded %s", client_info->name, file_info->filename);
                message->type = 0;
                message->size = strlen(message->data) + 1;
                int buf_size = message_write(buf, message);
                mtx_lock(&client_list_mutex);
                send_to_all(gclient_list, buf, BUF_SIZE, -1);
                mtx_unlock(&client_list_mutex);
            }
        }
        break;

        case 2:
        {
            send_file(client_info, message->data);
        }
        break;

        default:
            break;
        }
    }

    mtx_lock(&client_list_mutex);
    client_list_remove(gclient_list, client_info->fd);
    mtx_unlock(&client_list_mutex);
    
    close(client_info->fd);
    free(buf);
    free(message);
    free(file_info);
    free(client_info);

    return 0;
}

int start_server(int port)
{
    int lfd = inet_listen(port, gclient_list->max_clients);
    if (lfd == -1)
    {
        return -1;
    }

    while (1)
    {
        int fd = accept(lfd, NULL, NULL);
        if (fd == -1)
        {
            perror("accept()");
            return fd;
        }

        mtx_lock(&client_list_mutex);
        client_list_add(gclient_list, fd);
        mtx_unlock(&client_list_mutex);
        
        Client_info *client_info = malloc(sizeof(Client_info));
        client_info->fd = fd;

        printf("Client %d connected\n", fd);

        thrd_t thread;
        thrd_create(&thread, handle_connection, client_info);
    }
}

void mutex_init()
{
    mtx_init(&client_list_mutex, mtx_plain);
    mtx_init(&num_received_messages_mutex, mtx_plain);
}

void sigpipe_handle(int signum)
{
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Invalid command!\n");
        return 1;
    }

    int max_clients = atoi(argv[2]);

    if (max_clients == 0)
    {
        printf("Invalid max number of clients!\n");
        return 1;
    }

    int port = atoi(argv[1]);
    if (port == 0)
    {
        printf("Invalid port number!\n");
        return 1;
    }

    struct rlimit *clim = malloc(sizeof(struct rlimit)); // Current limit
    getrlimit(RLIMIT_NOFILE, clim);

    struct rlimit *nlim = malloc(sizeof(struct rlimit)); // New limit
    nlim->rlim_cur = 2048;
    nlim->rlim_max = clim->rlim_max;
    setrlimit(RLIMIT_NOFILE, nlim);

    // signal(SIGPIPE, sigpipe_handle);

    mutex_init();

    gclient_list = client_list_init(max_clients);

    return start_server(port);
}