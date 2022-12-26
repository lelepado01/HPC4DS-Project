#pragma once

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>

int getNumberOfLines(FILE *fp); 

extern void takeTime(int pid); 
extern void printTime(int pid, char* label);
extern double getTime(); 
extern void setTime(int pid, double time);
extern void saveTime(int pid, char* filename, char* label); 

extern void setReferenceProcess(int pid);