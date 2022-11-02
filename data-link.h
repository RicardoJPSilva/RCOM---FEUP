//
// Created by ricardo on 26-10-2022.
//

#ifndef RCOM___FEUP_DATA_LINK_H
#define RCOM___FEUP_DATA_LINK_H

#define numTransmissions 100
#define BUF_SIZE 256
#include "stddef.h"

struct array{
    unsigned char *content;
    size_t size;
};

int openPort(const char *serialPortName);
void closePort(int fd);
int connect(int fd);
int awaitConnection(int fd);
int disconnect(int fd);
int Write(int fd, struct array data);
struct array Read(int fd);

#endif //RCOM___FEUP_DATA_LINK_H


