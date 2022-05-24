//
// Created by spl211 on 5/24/22.
//

#ifndef LAB6_PIPEHELPER_H
#define LAB6_PIPEHELPER_H
#include "../task2/LineParser.h"

int ** createPipes(int nPipes);
void releasePipes(int **pipes, int nPipes);
int *leftPipe(int **pipes, cmdLine *pCmdLine);
int *rightPipe(int **pipes, cmdLine *pCmdLine);
#endif //LAB6_PIPEHELPER_H

