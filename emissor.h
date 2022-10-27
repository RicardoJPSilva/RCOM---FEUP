//
// Created by ricardo on 26-10-2022.
//

#ifndef RCOM___FEUP_EMISSOR_H
#define RCOM___FEUP_EMISSOR_H


int openPort(const char *serialPortName);
void closePort(int fd);
int connect(int fd);
int disconnect(int fd);

#endif //RCOM___FEUP_EMISSOR_H


