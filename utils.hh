#pragma once

#define USE_MPI 1

#ifdef USE_MPI
#include <mpi.h>
#endif

#include <string>

using namespace std;

void emsg(string msg);
void emsg(string msg, string msg2);
double ran();
double normal(float mu, float sd);
double gammasamp(double a, double b);

