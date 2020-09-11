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
void pack(const vector <Event> &vec);
void pack(const vector< vector <Event> > &vec, unsigned int time_division_per_timesAmin, unsigned int time_division_per_timesAmax);
void pack(const Particle &part);
void pack(const vector <Area> &vec);
void pack(const vector <DataRegion> &vec);
void pack(const vector <DemographicCategory> &vec);
void pack(const vector <vector <EventRef> > &vec);
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
void unpack(vector <Event> &vec);
void unpack(vector< vector <Event> > &vec, unsigned int time_division_per_timesAmin, unsigned int time_division_per_timesAmax);
void unpack(Particle &part);
void unpack(vector <Area> &vec);
void unpack(vector <DataRegion> &vec);
void unpack(vector <DemographicCategory> &vec);
void unpack(vector <vector <EventRef> > &vec);
void unpack(vector < vector <vector <unsigned int> > > &vec);

#endif
