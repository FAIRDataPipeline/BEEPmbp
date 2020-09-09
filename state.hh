#ifndef BEEPMBP__STATE_HH
#define BEEPMBP__STATE_HH

#include <vector>

using namespace std;

#include "model.hh"

class State
{
	public:
		State(MODEL &model);
		
	double L; 																				// The observation likelihood 
	double Lev; 																	   	// The latent process likelihood 
	double EF; 																				// The error function (used in abc methods)
	double Pr; 																		    // The prior probability
	
	vector < vector <FEV> > indev;                    // The individual event sequences
	vector <EVREF> x;                                 // Ordered list of references to infection events 
	vector < vector <EVREF> > trev;                   // Event references
	
	vector< vector <double> > Qmap;                   // The infectivty map 
	vector <double> lam;                              // Total force of infecion for an area

	double beta, phi;                                 // A temporary store for the values of beta and phi

	vector <double> sus;                              // The susceptibility for different demographic categories
	
	vector <double> areafac;                          // The modification due to area effects
	
	vector < vector <double> > disc_spline;           // A discretisation of the splines	
	
	//double likelihood();
	
	//void init();
	private:
	MODEL &model;
};

#endif