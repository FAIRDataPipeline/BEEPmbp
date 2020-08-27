// These are function for packing up quantities to be sent by MPI

#include <vector>
#include <iostream>
#include <algorithm>
#include <string>
#include <sstream>  

using namespace std;

#include "consts.hh"
#include "utils.hh"
#include "data.hh"
#include "model.hh"

namespace {
	unsigned int k;

	std::vector<double> buffer;
}

void packinit(size_t size)
{
	k = 0;
	buffer.resize(size);
}

size_t packsize()
{
	return k;
}

vector <double> copybuffer()
{
	return buffer;
}

void setbuffer(const vector <double> &vec)
{
	k = 0;
	buffer = vec;
}

double * packbuffer()
{
	return &buffer[0];
}

template <class T>
void pack_item(T t)
{
	buffer.push_back(t); k++;
}

// Provide overloads for cases where default behaviour needs
// to be modified
void pack_item(const string& vec)
{
	unsigned int jmax = vec.length();
	pack_item(jmax);
	for(unsigned int j = 0; j < jmax; j++){
		pack_item(vec.at(j));
	}
}

void pack(const unsigned int num)
{
	pack_item(num); 
}

void pack(const int num)
{
	pack_item(num); 
}

void pack(const unsigned short num)
{
	pack_item(num); 
}

void pack(const double num)
{
	pack_item(num); 
}

void pack(const string &vec)
{
	pack_item(vec);
}

// Use template to share implementation details between cases
template <class T>
void pack_item(const vector<T>& vec)
{
	pack_item(vec.size());
	for (auto& item : vec) {
		pack_item(item);
	}
}

void pack(const vector <unsigned int> &vec)
{
	pack_item(vec);
}

void pack(const vector <unsigned short> &vec)
{
	pack_item(vec);
}

void pack(const vector <int> &vec)
{
	pack_item(vec);
}

void pack(const vector <double> &vec)
{
	pack_item(vec);
}

void pack(const vector< vector <unsigned int> > &vec)
{
	pack_item(vec);
}

void pack(const vector< vector <double> > &vec)
{
	pack_item(vec);
}

void pack(const vector< vector <float> > &vec)
{
	pack_item(vec);
}

void pack(const vector< vector< vector <double> > > &vec)
{
	pack_item(vec);
}

void pack(const vector <string> &vec)
{
	pack_item(vec);
}

void pack(const vector <FEV> &vec)
{
	unsigned int imax, i;
	imax = vec.size(); buffer.push_back(imax); k++;
	for(i = 0; i < imax; i++){
		buffer.push_back(vec[i].trans); k++;
		buffer.push_back(vec[i].ind); k++;
		buffer.push_back(vec[i].t); k++;
		buffer.push_back(vec[i].timep); k++;
	}
}

void pack(const vector< vector <FEV> > &vec, unsigned int fedivmin, unsigned int fedivmax)
{
	unsigned int imax, i, jmax, j;
	imax = vec.size(); buffer.push_back(imax); k++;
	for(i = fedivmin; i < fedivmax; i++){
		jmax = vec[i].size(); buffer.push_back(jmax); k++;
		for(j = 0; j < jmax; j++){
			buffer.push_back(vec[i][j].trans); k++;
			buffer.push_back(vec[i][j].ind); k++;
			buffer.push_back(vec[i][j].t); k++;
			buffer.push_back(vec[i][j].timep); k++;
		}
	}
}

void pack(const Particle &part)
{
	pack(part.paramval);
	pack(part.ev);
	pack(part.EF);
}

void pack_item(const AREA& area)
{
	pack_item(area.code);
	pack_item(area.region);
	pack_item(area.x);
	pack_item(area.y);
	pack_item(area.agepop);
	pack_item(area.pop);
	pack_item(area.covar);
	pack_item(area.ind);
}

void pack(const vector <AREA> &vec)
{
	pack_item(vec);
}

void pack_item(const REGION& r)
{
	pack_item(r.name);
	pack_item(r.code);
}

void pack(const vector <REGION> &vec)
{
	pack_item(vec);
}

void pack_item(const DEMOCAT& d)
{
	pack_item(d.name);
	pack_item(d.value);
}

void pack(const vector <DEMOCAT> &vec)
{
	pack_item(vec);
}

void pack_item(const EVREF& ev)
{
	pack_item(ev.ind);
	pack_item(ev.e);
}

void pack(const vector <vector <EVREF> > &vec)
{
	pack_item(vec);
}

void pack(const vector < vector <vector <unsigned int> > > &vec)
{
	pack_item(vec);
}

template<class T>
void unpack_item(T &num)
{
	num = buffer[k]; k++;
}
void unpack_item(string &vec)
{
	unsigned int jmax, j;
	
	unpack_item(jmax);
	stringstream ss; for(j = 0; j < jmax; j++){ ss << (char) buffer[k]; k++;}
	vec = ss.str();
}

void unpack(unsigned int &num)
{
	unpack_item(num);
}

void unpack(unsigned short &num)
{
	unpack_item(num);
}

void unpack(double &num)
{
	unpack_item(num);
}

void unpack(string &vec)
{
	unpack_item(vec);
}

template <class T>
void unpack_item(vector<T>& vec)
{
	unsigned int size;
	unpack_item(size);
	vec.resize(size);
	for (auto& item : vec) {
		unpack_item(item);
	}
}

void unpack(vector <unsigned int> &vec)
{
	unpack_item(vec);
}

void unpack(vector <unsigned short> &vec)
{
	unpack_item(vec);
}

void unpack(vector <int> &vec)
{
	unpack_item(vec);
}

void unpack(vector <double> &vec)
{
	unpack_item(vec);
}

void unpack(vector< vector <unsigned int> > &vec)
{
	unpack_item(vec);
}

void unpack(vector< vector <double> > &vec)
{
	unpack_item(vec);
}

void unpack(vector< vector <float> > &vec)
{
	unpack_item(vec);
}

void unpack(vector< vector< vector <double> > > &vec)
{
	unpack_item(vec);
}

void unpack(vector <string> &vec)
{
	unpack_item(vec);
}

void unpack(vector <FEV> &vec)
{
	unsigned int imax, i;
	imax = buffer[k]; k++; vec.resize(imax);
	for(i = 0; i < imax; i++){
		vec[i].trans = buffer[k]; k++;
		vec[i].ind = buffer[k]; k++;
		vec[i].t = buffer[k]; k++;
		vec[i].timep = buffer[k]; k++;
	}
}

void unpack(vector< vector <FEV> > &vec, unsigned int fedivmin, unsigned int fedivmax)
{
	unsigned int imax, i, jmax, j;
	imax = buffer[k]; k++; vec.resize(imax);
	for(i = fedivmin; i < fedivmax; i++){
		jmax = buffer[k]; k++; vec[i].resize(jmax);
		for(j = 0; j < jmax; j++){ 
			vec[i][j].trans = buffer[k]; k++;
			vec[i][j].ind = buffer[k]; k++;
			vec[i][j].t = buffer[k]; k++;
			vec[i][j].timep = buffer[k]; k++;
		}
	}
}

void unpack(Particle &part)
{
	unpack(part.paramval);
	unpack(part.ev);
	unpack(part.EF);
}

void unpack_item(AREA& area)
{
	unpack_item(area.code);
	unpack_item(area.region);
	unpack_item(area.x);
	unpack_item(area.y);
	unpack_item(area.agepop);
	unpack_item(area.pop);
	unpack_item(area.covar);
	unpack_item(area.ind);
}

void unpack(vector <AREA> &vec)
{
	unpack_item(vec);
}

void unpack_item(REGION& r)
{
	unpack_item(r.name);
	unpack_item(r.code);
}

void unpack(vector <REGION> &vec)
{
	unpack_item(vec);
}

void unpack_item(DEMOCAT& d)
{
	unpack_item(d.name);
	unpack_item(d.value);
}

void unpack(vector <DEMOCAT> &vec)
{
	unpack_item(vec);
}

void unpack_item(EVREF& ev)
{
	unpack_item(ev.ind);
	unpack_item(ev.e);
}

void unpack(vector <vector <EVREF> > &vec)
{
	unpack_item(vec);
}

void unpack(vector < vector <vector <unsigned int> > > &vec)
{
	unpack_item(vec);
}
