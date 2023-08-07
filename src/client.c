#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <threads.h>
#include <libgen.h>
#include <unistd.h>
#include "message.h"
#include "inet_socket.h"

struct file_info_t
{
    int fd;
    char filename[UCHAR_MAX];
    long size;
};

void handle_file_message(Message *message, struct file_info_t *file_info, unsigned long *file_received_bytes)
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

void handle_text_message(Message *message)
{
    printf("%s\n", message->data);
}

int receive_handle_message(void *arg)
{
    int fd = *(int *)arg;

    Message *message = malloc(sizeof(Message));
    char *buf = malloc(BUF_SIZE);
    struct file_info_t *file_info = malloc(sizeof(struct file_info_t));

    unsigned long file_received_bytes = 0;

    while (1)
    {
        int num_recv = recv(fd, buf, BUF_SIZE, 0);
        if (num_recv == -1)
        {
            perror("recv()");
        }
        if (num_recv == 0)
        {
            printf("Server closed.\n");
            return -1;
        }

        // printf("buf = %s\n", buf);

        message_read(message, buf, num_recv);

        // printf("message->data = %s\n", message->data);
        // printf("num_recv = %d\n", num_recv);

        switch (message->type)
        {
        case 0:
        {
            handle_text_message(message);
            break;
        }

        case 1:
        {
            handle_file_message(message, file_info, &file_received_bytes);
            if (file_info->size == file_received_bytes && message->option == 2)
            {
                printf("Download compelete!\n");
            }
            break;
        }
        }
    }

    return 0;
}

void send_file(int fd, char *path)
{
    Message *message = malloc(sizeof(Message));
    char *buf = malloc(BUF_SIZE);

    int file_fd = open(path, O_RDONLY);
    if (file_fd == -1)
    {
        perror("open()");
        return;
    }

    sprintf(message->data, "%s", basename(path));
    message->type = 1;
    message->option = 0;
    message->size = strlen(message->data) + 1;
    unsigned int num_message_write = message_write(buf, message);
    if (send(fd, buf, BUF_SIZE, 0) == -1)
    {
        perror("send()");
        return;
    }
    

    struct stat stat_buf;
    if (stat(path, &stat_buf) == -1)
    {
        perror("stat()");
        return;
    }

    sprintf(message->data, "%ld", stat_buf.st_size);
    message->size = strlen(message->data) + 1;
    message->type = 1;
    message->option = 1;
    num_message_write = message_write(buf, message);
    if (send(fd, buf, BUF_SIZE, 0) == -1)
    {
        perror("send()");
        return;
    }
    

    while (1)
    {
        message->size = read(file_fd, message->data, DATA_SIZE);
        if (message->size == -1)
        {
            perror("read()");
            return;
        }
        else if (message->size == 0)
        {
            break;
        }

        message->type = 1;
        message->option = 2;
        num_message_write = message_write(buf, message);

        int num_sent = send(fd, buf, BUF_SIZE, 0);
        if (num_sent == -1)
        {
            perror("send()");
            return;
        }
        printf("Sent %d bytes\n", num_sent);
        

        if (message->size < DATA_SIZE)
        {
            break;
        }
    }

    close(file_fd);
}

void interact(int fd)
{
    Message *message = calloc(1, sizeof(Message));
    char *buf = malloc(BUF_SIZE);
    char *tmp = malloc(DATA_SIZE);
    int tmp_size = DATA_SIZE;

    while (1)
    {
        printf("Enter your name: ");
        fgets(message->data, DATA_SIZE, stdin);
        message->data[strlen(message->data) - 1] = '\0';
        if (strlen(message->data) > 0)
        {
            break;
        }
    }

    message->size = strlen(message->data) + 1;
    unsigned int num_message_write = message_write(buf, message);
    if (send(fd, buf, num_message_write, 0) == -1)
    {
        perror("send()");
        return;
    }
    

    printf("> ");

    while (1)
    {
        fgets(tmp, DATA_SIZE, stdin);
        tmp[strlen(tmp) - 1] = '\0';
        if (strlen(tmp) == 0)
        {
            continue;
        }

        if (strncmp(tmp, "/upload ", strlen("/upload ")) == 0)
        {
            char *path = malloc(DATA_SIZE);
            strcpy(path, tmp + strlen("/upload "));
            send_file(fd, path);
        }
        else if (strncmp(tmp, "/download ", strlen("/download ")) == 0)
        {
            strcpy(message->data, tmp + strlen("/download "));
            message->size = strlen(message->data) + 1;
            message->type = 2;
            num_message_write = message_write(buf, message);
            if (send(fd, buf, BUF_SIZE, 0) == -1)
            {
                perror("send()");
            }
            
        }
        else
        {
            message->type = 0;
            message->broadcast = 1;
            sprintf(message->data, "%s", tmp);
            message->size = strlen(message->data) + 1;
            // printf("message->data = %s\n", message->data);
            num_message_write = message_write(buf, message);
            // printf("buf = %s\n", buf);
            if (send(fd, buf, BUF_SIZE, 0) == -1)
            {
                perror("send()");
            }
            
        }
        printf("> ");
    }
}

int start_client(char *ip, int port)
{
    int fd = inet_connect(ip, port, SOCK_STREAM);
    if (fd == -1)
    {
        return -1;
    }

    int *arg = malloc(sizeof(int));
    *arg = fd;

    thrd_t thread;
    thrd_create(&thread, receive_handle_message, arg);

    interact(fd);

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Invalid command!\n");
        return 1;
    }

    int port = atoi(argv[2]);
    if (port == 0)
    {
        printf("Invalid port number!\n");
        return 1;
    }

    return start_client(argv[1], port);
}