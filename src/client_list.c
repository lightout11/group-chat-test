#include "client_list.h"
#include <threads.h>
#include <stdlib.h>
#include <string.h>

Client_list *client_list_init(int max_clients)
{
    Client_list *client_list = calloc(1, sizeof(Client_list));
    client_list->max_clients = max_clients;
    client_list->clients = calloc(max_clients, sizeof(Client_info));
    for (int i = 0; i < max_clients; i++)
    {
        client_list->clients[i].fd = -1;
    }
    return client_list;
}

Client_info *client_list_add(Client_list *client_list, int fd)
{
    int max_clients = client_list->max_clients;
    Client_info *clients = client_list->clients;
    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].fd == -1)
        {
            clients[i].fd = fd;
            return clients + i;
        }
    }
    return NULL;
}

void client_list_remove(Client_list *client_list, int fd)
{
    int max_clients = client_list->max_clients;
    Client_info *clients = client_list->clients;
    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].fd == fd)
        {
            clients[i].fd = -1;
            break;
        }
    }
}

void client_list_update(Client_list *client_list, int fd, char *name)
{
    int max_clients = client_list->max_clients;
    Client_info *clients = client_list->clients;
    for (int i = 0; i < max_clients; i++)
    {
        if (clients[i].fd == fd)
        {
            strcpy(clients[i].name, name);
            break;
        }
    }
}