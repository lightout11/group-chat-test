#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

#include <limits.h>

typedef struct client_info_st Client_info;
typedef struct client_list_st Client_list;

struct client_info_st
{
    int fd;
    char name[UCHAR_MAX];
    Client_info *next;
};

struct client_list_st
{
    Client_info *clients;
    int max_clients;
};

Client_list *client_list_init(int max_clients);

Client_info *client_list_add(Client_list *clients, int fd);

void client_list_remove(Client_list *client_list, int fd);

void client_list_update(Client_list *client_list, int fd, char *name);

#endif