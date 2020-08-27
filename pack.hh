#ifndef BEEPMBP__PACK_HH
#define BEEPMBP__PACK_HH

using namespace std;

#include "model.hh"

void packinit(size_t size);
size_t packsize();
double * packbuffer();
vector <double> copybuffer();
void setbuffer(const vector <double> &vec);

void pack(const unsigned int num);
void pack(const unsigned short num);
void pack(const double num);
void pack(const string &vec);
void pack(const vector <unsigned int> &vec);
void pack(const vector <unsigned short> &vec);
void pack(const vector <int> &vec);
void pack(const vector <double> &vec);
void pack(const vector< vector <unsigned int> > &vec);
void pack(const vector< vector <double> > &vec);
void pack(const vector< vector <float> > &vec);
void pack(const vector< vector< vector <double> > > &vec);
void pack(const vector <string> &vec);
void pack(const vector <FEV> &vec);
void pack(const vector< vector <FEV> > &vec, unsigned int fedivmin, unsigned int fedivmax);
void pack(const Particle &part);
void pack(const vector <AREA> &vec);
void pack(const vector <REGION> &vec);
void pack(const vector <DEMOCAT> &vec);
void pack(const vector <vector <EVREF> > &vec);
void pack(const vector < vector <vector <unsigned int> > > &vec);

void unpack(unsigned int &num);
void unpack(unsigned short &num);
void unpack(double &num);
void unpack(string &vec);
void unpack(vector <unsigned int> &vec);
void unpack(vector <unsigned short> &vec);
void unpack(vector <int> &vec);
void unpack(vector <double> &vec);
void unpack(vector< vector <unsigned int> > &vec);
void unpack(vector< vector <double> > &vec);
void unpack(vector< vector <float> > &vec);
void unpack(vector< vector< vector <double> > > &vec);
void unpack(vector <string> &vec);
void unpack(vector <FEV> &vec);
void unpack(vector< vector <FEV> > &vec, unsigned int fedivmin, unsigned int fedivmax);
void unpack(Particle &part);
void unpack(vector <AREA> &vec);
void unpack(vector <REGION> &vec);
void unpack(vector <DEMOCAT> &vec);
void unpack(vector <vector <EVREF> > &vec);
void unpack(vector < vector <vector <unsigned int> > > &vec);

#endif
