#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#define BUF_SIZE 1 << 11
#define DATA_SIZE 1 << 10

typedef struct message_st Message;

struct message_st
{
    int type;
    int option;
    long size;
    int broadcast;
    char data[DATA_SIZE];
};

int message_write(char *buf, Message *message);

int message_read(Message *message, char *buf, int size);

#endif