
#include <iostream>
#include <string>
#include <vector>
#include "stdlib.h"
#include "math.h"

#include "types.hh"
#include "functions.hh"
#include "consts.hh"
#include "var.hh"

using namespace std;

bool compX(long lhs, long rhs) { return house[lhs].x < house[rhs].x; }
bool compY(long lhs, long rhs) { return house[lhs].y < house[rhs].y; }
bool compNEV(NEV lhs, NEV rhs) { return lhs.t < rhs.t; }

double ran(){                              // Draws a random number between 0 and 1
	if(RAND_MAX == 32767){
		double v = (double(rand())*32766.0+rand())/(32767.0*RAND_MAX); if(v == 0 || v == 1) return 0.1;
		else return v;
	}
	else return double(0.999999999*rand())/RAND_MAX;
}
// Draws a normally distributed number with mean mu and standard deviation sd
double normal(float mu, float sd){ return mu + sd*sqrt(-2*log(ran()))*cos(2*M_PI*ran());}

// Displays any error messages
void emsg(string msg){ cout << msg << "\n"; exit (EXIT_FAILURE);}

double gammasamp(double a, double b)             // Draws a sample from the gamma distribution x^(a-1)*exp(-b*x)
{
  if(a < 0 || b < 0) emsg("Model: EC1");

  if(a < 1){
    double u = ran();
    return gammasamp(1.0 + a, b) * pow (u, 1.0 / a);
  }
  else{
    double x, v, u;
    double d = a - 1.0 / 3.0;
    double c = (1.0 / 3.0) / sqrt (d);
 
    while(1 == 1){
      do{
        x = sqrt(-2*log(ran()))*cos(2*M_PI*ran());
        v = 1.0 + c * x;
      }while (v < 0);

      v = v*v*v;
      u = ran();

      if (u < 1 - 0.0331*x*x*x*x) break;

      if (log(u) < 0.5*x*x + d*(1 - v + log(v))) break;
    }

    return d*v/b;
  }
}
