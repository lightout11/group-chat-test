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

int num_messages;
int num_clients;

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

        
    }

    return 0;
}

void interact(int fd)
{
    Message *message = calloc(1, sizeof(Message));
    char *buf = malloc(BUF_SIZE);
    char *tmp = malloc(BUF_SIZE);
    int tmp_size = BUF_SIZE;

    // while (1)
    // {
    //     printf("Enter your name: ");
    //     fgets(message->data, SHRT_MAX, stdin);
    //     message->data[strlen(message->data) - 1] = '\0';
    //     printf("message->data = %s\n", message->data);
    //     if (strlen(message->data) > 0)
    //     {
    //         break;
    //     }
    // }

    sleep(5);

    sprintf(message->data, "Client");
    message->size = strlen(message->data) + 1;
    unsigned int num_message_write = message_write(buf, message);
    if (send(fd, buf, BUF_SIZE, 0) == -1)
    {
        perror("send()");
        return;
    }

    sleep(2);
    // usleep(100000);

    strcpy(message->data, "Hello");
    message->type = 0;
    message->broadcast = 1;
    message->size = strlen(message->data) + 1;
    num_message_write = message_write(buf, message);
    for (int i = 0; i < num_messages; i++)
    {
        if (send(fd, buf, BUF_SIZE, 0) == -1)
        {
            perror("send()");
        }
        // usleep(100000);
    }

    while (1);

    // while (1)
    // {
    //     fgets(tmp, SHRT_MAX, stdin);
    //     tmp[strlen(tmp) - 1] = '\0';
    //     if (strlen(tmp) == 0)
    //     {
    //         continue;
    //     }
    //     printf("tmp = %s\n", tmp);

    //     if (strncmp(tmp, "/upload ", strlen("/upload ")) == 0)
    //     {
    //         char *path = malloc(SHRT_MAX);
    //         strcpy(path, tmp + strlen("/upload "));
    //         send_file(fd, path);
    //     }
    //     else if (strncmp(tmp, "/download ", strlen("/download ")) == 0)
    //     {
    //         // printf("download\n");
    //         strcpy(message->data, tmp + strlen("/download "));
    //         message->size = strlen(message->data) + 1;
    //         message->type = 2;
    //         num_message_write = message_write(buf, message);
    //         if (send(fd, buf, num_message_write, 0) == -1)
    //         {
    //             perror("send()");
    //         }
    //     }
    //     else
    //     {
    //         printf("send text\n");
    //         message->type = 0;
    //         message->broadcast = 1;
    //         sprintf(message->data, "%s", tmp);
    //         message->size = strlen(message->data) + 1;
    //         num_message_write = message_write(buf, message);
    //         if (send(fd, buf, num_message_write, 0) == -1)
    //         {
    //             perror("send()");
    //         }
    //     }
    // }
}

int start_client(char *ip, int port)
{
    pid_t pid = getppid();
    for (int i = 0; i < num_clients; i++)
    {
        fork();
    }

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
    if (argc != 5)
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

    num_clients = atoi(argv[3]);
    num_messages = atoi(argv[4]);

    return start_client(argv[1], port);
}