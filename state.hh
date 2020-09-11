#ifndef BEEPMBP__StatisticsE_HH
#define BEEPMBP__StatisticsE_HH

#include <vector>

using namespace std;

#include "model.hh"
#include "obsmodel.hh"
#include "consts.hh"
#include "jump.hh"

struct TransInfo{                          // Stores information about transition when doing compartmental parameter proposals 
	vector <unsigned int> num;               // The number of times down transition 
	unsigned int numvisittot;                // The number of times the compartment is visited
	double dtsum;                            // Sums up the total time spent
	vector <double> dtlist;                  // Keeps a list of waiting time
};

class State
{
	public:
		State(const Details &details, const Data &data, const Model &model, const ObservationModel &obsmodel);
			
		double L; 																			   	 // The observation likelihood (used with MC3 methods)
		double EF; 																				   // The error function (used in abc methods)
		double Pr; 																		       // The prior probability
		
		vector <double> paramval;                            // The parameter values

		vector < vector <Event> > indev;                     // The individual event sequences
		vector <EventRef> infev;                             // Ordered list of references to infection events 
		vector < vector <EventRef> > transev;                // Event references
		
		vector< vector <double> > Qmap;                      // The infectivty map 
		
	 	/* The quantities below are all derived from the parameter values */
		vector <double> susceptibility;                      // The susceptibility for different demographic categories
		vector <double> areafactor;                          // The modification due to area effects
		vector < vector <double> > disc_spline;              // A discretisation of the splines	
		vector <CompTransProb> comptransprob;                // Stores the probabilities of going down transitions
		
		/* The quantities below are temporary */
		double beta, phi;                                    // A store for the values of beta and phi
		vector <double> lambda;                              // Total force of infecion for an area
		double Lev; 																	   	   // The latent process likelihood 
		vector <int> popw;                                   // The population in w (use to calculate Lev)

		void simulate_compartmental_transitions(unsigned int i, unsigned int c, double t);
		void set_process_likelihood();
		void clear();
		void copy_state(const State &from);
		Status set_param(const vector <double> &paramv);
		void set_beta_and_phi(unsigned int sett);
		void set_L_and_Pr();
		double get_infection_time(unsigned int n) const;
		void add_indev(unsigned int i);
		void simulate_from_model(unsigned int i, unsigned int c, double t);
		void set_Qmap_using_dQ(unsigned int sett, const State &state, const vector <double> &dQmap);
		void set_Qmap(unsigned int check);
		void sort_infev();
		void initialise_from_particle(const Particle &part);
		void standard_parameter_prop(Jump &jump);
		void check() const;
						
	private:
		void stand_param_betaphi_prop(Jump &jump);	   // These function make up the "standard" proposal
		void stand_param_area_prop(Jump &jump);
		void stand_param_compparam_prop(Jump &jump);
		double likelihood_prob(vector <TransInfo> &transinfo, vector <CompTransProb> &comptransprob) const;
		double likelihood_dt(vector <TransInfo> &transinfo, vector <double> &paramv) const;
		double dlikelihood_dt(vector <TransInfo> &transinfo, vector <double> &paramvi, vector <double> &paramvf) const;
		void check_Lev_and_Pr();

		
		const vector <Compartment> &comp;
		const vector <Transition> &trans;
		const vector <Param> &param;
		
		const Details &details;
		const Data &data;
		const Model &model;
		const ObservationModel &obsmodel;
};

#endif