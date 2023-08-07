#include <stdio.h>
#include <stdlib.h>
#include "message.h"

int message_write(char *buf, Message *message)
{
    sprintf(buf, "%d\r\n%d\r\n%ld\r\n%d\r\n", message->type, message->option, message->size, message->broadcast);
    int header = strlen(buf);
    memcpy(buf + header, message->data, message->size);
    return header + message->size;
}

int message_read(Message *message, char *buf, int size)
{
    sscanf(buf, "%d\r\n%d\r\n%ld\r\n%d\r\n", &message->type, &message->option, &message->size, &message->broadcast);
    char *buff = malloc(BUF_SIZE);
    sprintf(buff, "%d\r\n%d\r\n%ld\r\n%d\r\n", message->type, message->option, message->size, message->broadcast);
    memcpy(message->data, buf + strlen(buff), message->size);
}
