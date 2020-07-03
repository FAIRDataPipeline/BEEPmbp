#pragma once

#include <vector>

using namespace std;

#include "consts.hh"
#include "data.hh"
#include "toml11/toml.hpp"

struct FEV {                               // Stores information about a compartmental transition
  unsigned int trans;                      // References the transition type
	unsigned int ind;                        // The individual on which the transition happens
	double t;                                // The time of the transition
	unsigned int timep;                      // The time period in which the transition occurs 
};

struct EVREF {                             // Used to reference an event
	unsigned int ind;                        // The individual
	unsigned int e;	                         // The event number
};

struct DQINFO {                            // Stores information about a change in the Q matrix
	vector <unsigned int> q;
	vector <double> fac;
};

struct PARAM{                              // Store information about a model parameter
 	string name;                             // Its name
 	double valinit;                          // The simulation value or starting value for inference
	double sim;                              // The simulation value
	double min;                              // The minimum value (assuming a uniform prior) 
	double max;                              // The maximum value (assuming a uniform prior)
	unsigned int used;                       // Determins if the parameter is used in the model.
	unsigned int ntr, nac;                   // Store the number of proposals tried and accepted	
	double jump;
};

struct COMP{                               // Stores information about a compartment in the model
	string name;                             // Its name
	double infectivity;                      // How infectious that compartment is

	unsigned int type;                       // The type of distribution for waiting in compartment (exponential or gamma)
	int param1;                              // First characteristic parameter (e.g. mean)
	int param2;                              // Second characteristic parameter (e.g. standard deviation in the case of gamma)

	vector <unsigned int> trans;             // The transitions leaving that compartment
	unsigned int transtimep;                 // The time period transitions leaving that compartment

	vector <vector <double> > prob, probsum; // The age-dependent probability of going down transition
	vector <vector <double> > probi;         // The age-dependent probability of going down transition for initial state (MBP)
	vector <vector <double> > probp;         // The age-dependent probability of going down transition for proposed state (MBP)
	
	vector <double> probin;                  // The prob ind goes to compartment (used to calculate R0)
	vector <double> infint;                  // The integrated infectivity (used to calculate R0)
};

struct TRANS{                              // Stores information about a compartmental model transition
	unsigned int from;                       // Which compartment the individual is coming from
	unsigned int to;                         // Which compartment the individual is going to
	vector <unsigned int> probparam;         // The parameter for the probability of going down transition
	vector <unsigned int> DQ;                // The change in the Q tensor for going down the transition
};

struct SPLINEP{                            // Stores information about a spline point
	double t;                                // The time of the point
	unsigned int param;                      // The parameter which defines the value
};

class MODEL                                // Stores all the information about the model
{
public:
	MODEL(DATA &data);

	vector <double> beta;                    // The value for beta at the various times
	vector <SPLINEP> betaspline;             // The spline used to define beta
	
	vector <double> phi;                     // The value for phi at the various times
	vector <SPLINEP> phispline;              // The spline used to define phi
	
	vector <double> sus;                     // The susceptibility for different demographic categories

	vector <double> areafac;                 // The modification due to area effects
	
	vector <double> betai, betap;            // Under MBPs the values of beta for the initial and proposed states
	vector <double> phii, phip;              // Under MBPs the values of phi for the initial and proposed states
	vector <double> parami, paramp;          // Under MBPs the parameter values for the initial and proposed states
	vector <double> susi, susp;              // Under MBPs the susceptibility for the initial and proposed states
	vector <double> areafaci, areafacp;      // Under MBPs the area factor for the initial and proposed states
				
	vector <PARAM> param;                    // Information about parameters in the model
	vector <double> paramval;                // The values of the parameters
	vector <TRANS> trans;                    // Stores model transitions
	vector <COMP> comp;	                     // Stores model compartments

	unsigned int infmax;                     // The maximum number of infected individuals
	
	unsigned int ntr, nac;                   // Gets the base acceptance rate
	
	vector< vector <unsigned int> > sus_param;  // The parameters related to fixed effect for susceptibility
	vector <unsigned int> covar_param;       // The parameters related to covariates for areas
	
	vector <TIMEP> timeperiod;              // The timings of changes to Q;
	unsigned int ntimeperiod;
	
	vector <DQINFO> DQ;                      // Keeps track of the change in the Q matrix 
	
	DATA &data;
	
	double getparam(string name);
	double getinfectivity(string name);
	void simmodel(vector <FEV> &evlist, unsigned int i, unsigned int c, double t);
	void mbpmodel(vector <FEV> &evlisti, vector <FEV> &evlistp);
	void definemodel(unsigned int core, double period, unsigned int popsize, const toml::basic_value<::toml::discard_comments, std::unordered_map, std::vector> &tomldata);
	void addQ();

	void priorsamp();
	        
	void checktransdata();
	unsigned int setup(vector <double> &paramval);
	void copyi();
	void copyp();
	vector <double> R0calc();
	
private:
	void addcomp(string name, double infectivity, unsigned int type, string param1, string param2);
	void addparam(string name, double val, double min, double max);
	void addtrans(string from, string to, string prpar);
	void setsus(); 
	void setarea();
	void timevariation();
	unsigned int findparam(string name);
	unsigned int settransprob();
};
