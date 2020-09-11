// This file contains all the functions for running a MCMC er the MBP algorithm

#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>  
#include "stdlib.h"
#include "math.h"
#include "assert.h"

using namespace std;

#include "timers.hh"
#include "model.hh"
#include "utils.hh"
#include "chain.hh"
#include "output.hh"
#include "pack.hh"
#include "obsmodel.hh"

/// Initialises a single mcmc chain
Chain::Chain(const Details &details, const DATA &data, const MODEL &model, const POPTREE &poptree, const Obsmodel &obsmodel, const Output &output, unsigned int _ch) : initial(details,data,model,obsmodel), propose(details,data,model,obsmodel), comp(model.comp), lev(poptree.lev), trans(model.trans), details(details), data(data), model(model), poptree(poptree), obsmodel(obsmodel), output(output)
{
	ch = _ch;

	initialise_variables();                      // Initialises the variables in the class
	
	sample_state();                              // Samples the prior and simulates to generate an initial state 
	
	jump.init(initial.paramval);                 // Initialises the kernel used for jumping in parameter space
	
	if(details.mode != inf) return;
	
	initial.set_LPr();                            // Sets the initial observation likelihood
}


/// Performs a "standard" set of proposals
void Chain::standard_prop(double EFcut) 
{
	initial.standard_parameter_prop(jump);       // Makes changes to parameters with fixed event sequence
	stand_event_prop(EFcut);                     // Makes changes to event sequences with fixed parameters
}

		
/// Randomly samples a state by sampling from the prior and simulating an event sequence
void Chain::sample_state()
{
	unsigned int loop, loopmax = 100;
	for(loop = 0; loop < loopmax; loop++){       // Keeps simulating until successful
		if(simulate(model.priorsamp()) == success) break;
	}
	
	if(loop == loopmax){
		stringstream ss;
		ss << "After '"+to_string(loopmax)+"' random simulations, it was not possible to find an initial ";
		ss << "state with the number of infected individuals below the threshold 'infmax' specified in the input TOML file.";
		emsg(ss.str());
	}
}


/// Performs a MBP proposal on parameter 'th'
void Chain::mbp_proposal(unsigned int th)  
{
	timers.timembpprop -= clock();

	vector <double> paramv = jump.mbp_prop(initial.paramval,th);

	double al = 0; 
	if(mbp(paramv) == success){              // Performs the MBP and is successful calculates acceptance probability
		propose.set_LPr();
		
		al = exp(propose.Pr-initial.Pr + invT*(propose.L-initial.L));		
		if(checkon == 1) cout << al << " " << invT << " " << propose.L << " " << initial.L << " al" << endl;
	}

	if(ran() < al){
		initial.copy(propose);                 // If successful copies the proposed state into the initial
		jump.mbp_accept(th);
	}
	else{
		jump.mbp_reject(th);
	}

	if(checkon == 1) initial.check();
	
	timers.timembpprop += clock();
}


/// Performs a model-based proposal (MBP)
Status Chain::mbp(const vector<double> &paramv)
{
	timers.timembpinit -= clock();
	
	if(model.inbounds(paramv) == false) return fail;      // Checks parameters are within the prior bounds
		
	if(propose.set_param(paramv) == fail) return fail;    // Sets quantities derived from parameters (if not possible fails)
	
	mbp_init();                                           // Prepares for the proposal

	timers.timembpinit += clock();
	
	timers.timembp -= clock();
		
	auto n = 0u;                                          // Indexes infections in the initial state 
	double t = 0;                                         // Current time
	
	for(auto sett = 0u; sett < details.nsettime; sett++){ // Goes across discrete timesteps
		if(details.mode == sim) output.populations(sett,N);
		
		initial.set_betaphi(sett);                          // Sets beta and phi variables used later
		propose.set_betaphi(sett); 
		
		propose.set_Qmap_using_dQ(sett,initial,dQmap);      // Calculates proposed Qmap from initial Qmap plus dQ
	
		construct_infection_sampler(initial.Qmap[sett],propose.Qmap[sett]); // Sampler used to generate new infections 
	
		double tmax = details.settime[sett+1];
		do{
			auto tini = initial.get_infection_time(n);
			auto tinf = t + exp_sample(new_infection_rate[0][0]);
			
			if(tini >= tmax && tinf >= tmax){ t = tmax; break;}
			
			if(tinf < tini){                                  // A new infection appears in the propsed state
				t = tinf;
				add_infection_in_area(area_of_next_infection(),t);	
			}
			else{                                             // An event on initial sequence is considered
				t = tini;
				copy_event_or_not(n);
				n++;
			}
	
			if(propose.x.size() >= model.infmax){             // If the number of infections exceeds the prior limit then fails
				reset_susceptible_lists(); return fail;
			}
		}while(1 == 1);
		
		update_dQmap(initial.trev[sett],propose.trev[sett]);// Based on infections updates dQmap
		if(checkon == 1) check(t,sett);
	}

	timers.timembp += clock();
		
	reset_susceptible_lists();
	
	return success;
}

/// Decides to copy infection event i from the initial to proposed states or not
void Chain::copy_event_or_not(unsigned int n)
{
	auto i = initial.x[n].ind;

	if(susceptible_status[i] == both_sus){                // The copy can only be done if individual is suscepticle
		auto w = data.ind[i].area*data.ndemocatpos + data.ind[i].dp;

		auto al = propose.lam[w]/initial.lam[w];
		if(ran() < al){                                     // Copies the infection event to propose
			change_susceptible_status(i,not_sus,1);
			
			if(do_mbp_event == true) mbp_compartmental_transitions(i);
			else propose.indev[i] = initial.indev[i];
			
			propose.add_indev(i);
		}
		else change_susceptible_status(i,ponly_sus,1);      // Does not copy the infection event propose
	}
}


/// Based on compartmental trasitions in the initial state for i, this uses MBPs to generates transitions for the proposal
void Chain::mbp_compartmental_transitions(unsigned int i)
{
	const auto &evlisti = initial.indev[i];
	auto &evlistp = propose.indev[i];
	const auto &parami = initial.paramval, &paramp = propose.paramval;
	const auto &comptransi = initial.comptrans, &comptransp = propose.comptrans;
	
	evlistp.clear();
	
	FEV ev = evlisti[0];
	auto tra = ev.trans;
	auto ti = ev.t; 
	auto timep = ev.timep;
	evlistp.push_back(ev);
	
	auto a = data.democatpos[data.ind[i].dp][0];
		
	auto c = trans[tra].to;
	
	auto tp = ti;
	unsigned int emax = evlisti.size();
	for(auto e = 1u; e < emax; e++){
		tra = evlisti[e].trans;
		if(trans[tra].istimep == 0){		
			auto dt = evlisti[e].t - ti;
			ti = evlisti[e].t;
			
			unsigned int kmax = comp[c].trans.size();
			if(kmax == 0) break;
			
			if(kmax > 1){
				auto k = 0u; while(k < kmax && tra != comp[c].trans[k]) k++;
				if(k == kmax) emsgEC("model",4);
				
				if(comptransp[c].prob[a][k] < comptransi[c].prob[a][k]){  // Looks at switching to another branch
					if(ran() < 1 - comptransp[c].prob[a][k]/comptransi[c].prob[a][k]){
						auto sum = 0.0; 
						vector <double> sumst;
						for(auto k = 0u; k < kmax; k++){
							auto dif = comptransp[c].prob[a][k] - comptransi[c].prob[a][k];
							if(dif > 0) sum += dif;
							sumst.push_back(sum);
						}
						
						auto z = ran()*sum; auto k = 0u; while(k < kmax && z > sumst[k]) k++;
						if(k == kmax) emsgEC("Model",5);
						tra = comp[c].trans[k];
					}
				}	
			}
			
			double dtnew;
			switch(trans[tra].type){
			case exp_dist:
				{
					auto p = trans[tra].param_mean;
					dtnew = dt*paramp[p]/parami[p];
				}
				break;
			
			case gamma_dist:
				emsgEC("model",6);
				break;
				
			case lognorm_dist:
				{
					auto p = trans[tra].param_mean, p2 = trans[tra].param_cv;
					
					auto mean_nsi = parami[p], cv_nsi = parami[p2];
					auto mean_nsp = paramp[p], cv_nsp = paramp[p2];
					
					if(mean_nsi == mean_nsp && cv_nsi == cv_nsp) dtnew = dt;
					else{
						auto sdi = sqrt(log(1+cv_nsi*cv_nsi)), meani = log(mean_nsi) - sdi*sdi/2;
						auto sdp = sqrt(log(1+cv_nsp*cv_nsp)), meanp = log(mean_nsp) - sdp*sdp/2;
						dtnew = exp(meanp + (log(dt) - meani)*sdp/sdi); 
					}
				}
				break;
				
			default:
				emsgEC("Model",7);
				break;
			}
	
			if(dtnew < tiny) dtnew = tiny;
			tp += dtnew;
	
			while(timep < model.ntimeperiod-1 && model.timeperiod[timep].tend < tp){    // Adds in changes in time period
				ev.trans = comp[c].transtimep; ev.t = model.timeperiod[timep].tend;
				evlistp.push_back(ev);
				timep++;
				ev.timep = timep; 
			}
			
			ev.trans = tra; ev.t = tp;
			evlistp.push_back(ev);
	
			c = trans[tra].to; 
			if(evlisti[e].trans != tra) break;
		}
	}
	
	if(comp[c].trans.size() != 0)	propose.simulate_compartmental_transitions(i,c,tp);
}


/// Constructs a fast Gillespie sampler for finding the next infection to be added	
void Chain::construct_infection_sampler(const vector <double> &Qmi, const vector <double> &Qmp)
{
	timers.infection_sampler -= clock();
		
	int l = poptree.level-1;
	for(auto c = 0u; c < data.narea; c++){
		auto wmin = c*data.ndemocatpos; 
		auto wmax = wmin + data.ndemocatpos;
	
		auto faci = initial.beta*initial.areafac[c];
		auto facp = propose.beta*propose.areafac[c];
		
		double sum = 0; 
		auto dp = 0u; 
		auto v = c*data.nage; 
		for(auto w = wmin; w < wmax; w++){
			auto a = data.democatpos[dp][0];
			initial.lam[w] = initial.sus[dp]*(faci*Qmi[v+a] + initial.phi);
			propose.lam[w] = propose.sus[dp]*(facp*Qmp[v+a] + propose.phi);
			auto dlam = nboth_susceptible_list[w]*(propose.lam[w] - initial.lam[w]); if(dlam < 0) dlam = 0;
			if(std::isnan(dlam)){ emsgEC("Chain",400);}
			sum += dlam + npropose_only_susceptible_list[w]*propose.lam[w];
			dp++;
		}
		if(std::isnan(sum)) emsgEC("Chain",4);
		new_infection_rate[l][c] = sum;
	}
	
	for(int l = poptree.level-2; l >= 0; l--){                                 
		auto cmax = lev[l].node.size();
		for(auto c = 0u; c < cmax; c++){
			double sum = 0; for(const auto& ch : lev[l].node[c].child) sum += new_infection_rate[l+1][ch];
			
			new_infection_rate[l][c] = sum;
		}
	}
	
	timers.infection_sampler += clock();
}
	

/// Sets up lists of individual (those suscepticle in both initial and propose, only propose, and not at all)
void Chain::setup_susceptible_lists()
{	
	both_susceptible_list.clear(); both_susceptible_list.resize(data.nardp); 
	propose_only_susceptible_list.clear(); propose_only_susceptible_list.resize(data.nardp); 
	not_susceptible_list.clear(); not_susceptible_list.resize(data.nardp);

	nboth_susceptible_list.resize(data.nardp); 
	npropose_only_susceptible_list.resize(data.nardp); 
	nnot_susceptible_list.resize(data.nardp);
	
	susceptible_status.resize(data.popsize); susceptible_list_ref.resize(data.popsize); 

	for(auto c = 0u; c < data.narea; c++){
		for(auto dp = 0u; dp < data.ndemocatpos; dp++){
			auto w = c*data.ndemocatpos + dp;

			for(const auto& i : data.area[c].ind[dp]){
				susceptible_status[i] = both_sus;
				susceptible_list_ref[i] = both_susceptible_list[w].size();
				both_susceptible_list[w].push_back(i);
			}
			nboth_susceptible_list[w] = data.area[c].ind[dp].size();
			npropose_only_susceptible_list[w] = 0;
			nnot_susceptible_list[w] = 0;
		}
	}	
}


/// Makes a change in the susceptibility status of an individual
void Chain::change_susceptible_status(unsigned int i, unsigned int st, unsigned int updateR)
{
	auto c = data.ind[i].area;
	auto w = c*data.ndemocatpos + data.ind[i].dp;
	
	double dval = 0;
	if(updateR == 1){		
		auto dlam = nboth_susceptible_list[w]*(propose.lam[w] - initial.lam[w]); if(dlam < 0) dlam = 0;
		dval = -(dlam + npropose_only_susceptible_list[w]*propose.lam[w]);
	}
	
	int l = susceptible_list_ref[i];   // Removes the einitial.xsiting entry
	int n;
	switch(susceptible_status[i]){
  case both_sus:
		if(both_susceptible_list[w][l] != i) emsgEC("Chain",7);
		n = both_susceptible_list[w].size();
		if(l < n-1){
			both_susceptible_list[w][l] = both_susceptible_list[w][n-1];
			susceptible_list_ref[both_susceptible_list[w][l]] = l;
		}
		both_susceptible_list[w].pop_back();
		nboth_susceptible_list[w]--;
		break;
		
	case ponly_sus:
		if(propose_only_susceptible_list[w][l] != i) emsgEC("Chain",8);
		n = propose_only_susceptible_list[w].size();
		if(l < n-1){
			propose_only_susceptible_list[w][l] = propose_only_susceptible_list[w][n-1];
			susceptible_list_ref[propose_only_susceptible_list[w][l]] = l;
		}
		propose_only_susceptible_list[w].pop_back();
		npropose_only_susceptible_list[w]--;
		break;
		
	case not_sus:
		if(not_susceptible_list[w][l] != i) emsgEC("Chain",9);
		n = not_susceptible_list[w].size();
		if(l < n-1){
			not_susceptible_list[w][l] = not_susceptible_list[w][n-1];
			susceptible_list_ref[not_susceptible_list[w][l]] = l;
		}
		not_susceptible_list[w].pop_back();
		nnot_susceptible_list[w]--;
		break;
	
	default: emsgEC("Chain",10); break;
	}

	susceptible_status[i] = st;
	switch(susceptible_status[i]){
	case ponly_sus:
		susceptible_list_ref[i] = propose_only_susceptible_list[w].size();
		propose_only_susceptible_list[w].push_back(i);
		npropose_only_susceptible_list[w]++;
		break;
		
	case not_sus:
		susceptible_list_ref[i] = not_susceptible_list[w].size();
		not_susceptible_list[w].push_back(i);
		nnot_susceptible_list[w]++;
		break;
	
	case both_sus:
		susceptible_list_ref[i] = both_susceptible_list[w].size();
		both_susceptible_list[w].push_back(i);
		nboth_susceptible_list[w]++;
		break;
		
	default: emsgEC("Chain",11); break;
	}
	
	if(updateR == 1){                                       // Updates the infection sampler
		auto dlam = nboth_susceptible_list[w]*(propose.lam[w] - initial.lam[w]); if(dlam < 0) dlam = 0;
		dval += dlam + npropose_only_susceptible_list[w]*propose.lam[w];
		
		if(dval != 0){
			int l = poptree.level-1;
			do{
				new_infection_rate[l][c] += dval;
				c = lev[l].node[c].parent; l--;
			}while(l >= 0);
		}
	}
}


/// Places all individuals back onto the "both susceptible" list 
void Chain::reset_susceptible_lists()
{
	for(auto w = 0u; w < data.nardp; w++){
		for(const auto& i : propose_only_susceptible_list[w]){
			susceptible_status[i] = both_sus;
			susceptible_list_ref[i] = both_susceptible_list[w].size();
			both_susceptible_list[w].push_back(i);
			nboth_susceptible_list[w]++;
		}
		propose_only_susceptible_list[w].clear();
		npropose_only_susceptible_list[w] = 0;
		
		for(const auto& i : not_susceptible_list[w]){
			susceptible_status[i] = both_sus;
			susceptible_list_ref[i] = both_susceptible_list[w].size();
			both_susceptible_list[w].push_back(i);
			nboth_susceptible_list[w]++;
		}
		not_susceptible_list[w].clear();
		nnot_susceptible_list[w] = 0;
	}
}


/// Updates dQmap based on events that occur in the initial and proposed states
void Chain::update_dQmap(const vector <EVREF> &trei, const vector <EVREF> &trep)
{
	timers.timembpQmap -= clock();
	
	auto nage = data.nage;

	for(const auto& tre : trei){
		auto i = tre.ind; 
		auto tra = initial.indev[i][tre.e].trans;
		indmap[i][tra] = 1;
	}
	
	for(const auto& tre : trep){
		auto i = tre.ind; 
		auto tra = propose.indev[i][tre.e].trans;	
		if(indmap[i][tra] == 0){
			auto v = data.ind[i].area*nage+data.democatpos[data.ind[i].dp][0];
			auto dq = trans[tra].DQ[propose.indev[i][tre.e].timep];

			if(dq != UNSET){
				for(auto loop = 0u; loop < 2; loop++){
					auto q = model.DQ[dq].q[loop];
					if(q != UNSET){
						if(dQbuf[v][q] == 0){ dQbuflistv.push_back(v); dQbuflistq.push_back(q);}
						dQbuf[v][q] += model.DQ[dq].fac[loop];
					}
				}
			}
		}
		else indmap[i][tra] = 0;
	}	
	
	for(const auto& tre : trei){
		auto i = tre.ind; 
		auto tra = initial.indev[i][tre.e].trans;
		if(indmap[i][tra] == 1){
			auto v = data.ind[i].area*nage+data.democatpos[data.ind[i].dp][0];
			auto dq = trans[tra].DQ[initial.indev[i][tre.e].timep];
			if(dq != UNSET){
				for(auto loop = 0u; loop < 2; loop++){
					auto q = model.DQ[dq].q[loop];
					if(q != UNSET){
						if(dQbuf[v][q] == 0){ dQbuflistv.push_back(v); dQbuflistq.push_back(q);}
						dQbuf[v][q] -= model.DQ[dq].fac[loop];
					}
				}
			}
			indmap[i][tra] = 0;
		}
	}
	
	if(details.mode == sim){
		for(const auto& tre : trep){
			auto tra = propose.indev[tre.ind][tre.e].trans;
			N[trans[tra].from]--;
			N[trans[tra].to]++;
		}
	}

	nage = data.nage;
	auto jmax = dQbuflistv.size();
	for(auto j = 0u; j < jmax; j++){
		auto v = dQbuflistv[j]; 
		auto q = dQbuflistq[j]; 
		auto qt = data.Q[q].Qtenref;
		
		auto fac = dQbuf[v][q];
		if(fac < -vtiny || fac > vtiny){
			auto kmax = data.genQ.Qten[qt].ntof[v];
			
			auto& cref = data.genQ.Qten[qt].tof[v];
			auto& valref = data.genQ.Qten[qt].valf[v];
			if(nage == 1){
				for(auto k = 0u; k < kmax; k++){
					dQmap[cref[k]] += fac*valref[k][0];
				}
			}
			else{
				for(auto k = 0u; k < kmax; k++){
					auto vv = cref[k]*nage;	
					for(auto a = 0u; a < nage; a++){
						dQmap[vv] += fac*valref[k][a];
						vv++;
					}
				}
			}
		}
		dQbuf[v][q] = 0;
	}
	dQbuflistv.clear(); dQbuflistq.clear(); 
	
	timers.timembpQmap += clock();
}
	
	
/// This samples which area the next new infection occurs in 
/// Starting at the top level l=0 the algorith proceeds to finer and finer scales
unsigned int Chain::area_of_next_infection()
{
	double sumst[4];
	
	auto l = 0u, c = 0u;                            
	auto lmax = poptree.level;
	while(l < lmax-1){
		auto& node = lev[l].node[c];
		
		double sum = 0;
		auto jmax = node.child.size();
		for(auto j = 0u; j < jmax; j++){
			sum += new_infection_rate[l+1][node.child[j]];	
			sumst[j] = sum;
		}
		
		double z = ran()*sum; auto j = 0u; while(j < jmax && z > sumst[j]) j++; if(j == jmax) emsgEC("Chain",12);
		
		c = node.child[j];
		l++;
	};
	
	return c;
}


/// Adds a individual infected at time t in area c
void Chain::add_infection_in_area(unsigned int c, double t)
{
	auto dpmax = data.ndemocatpos;
	
	vector <double> sumst;
	sumst.resize(dpmax);
	
	double sum = 0;                                      // Selects which demographic possibility from the area
	for(auto dp = 0u; dp < dpmax; dp++){
		auto w = c*dpmax + dp;
		double dlam = nboth_susceptible_list[w]*(propose.lam[w] - initial.lam[w]); if(dlam < 0) dlam = 0;
		sum += dlam + npropose_only_susceptible_list[w]*propose.lam[w];
		sumst[dp] = sum;
	}

	double z = ran()*sum; auto dp = 0u; while(dp < dpmax && z > sumst[dp]) dp++; if(dp == dpmax) emsgEC("Chain",13);
	
	auto w = c*dpmax + dp;                               // Next selects susceptible list type
	double dlam = nboth_susceptible_list[w]*(propose.lam[w] - initial.lam[w]); if(dlam < 0) dlam = 0;

	unsigned int i;
	if(ran() < dlam/(dlam + npropose_only_susceptible_list[w]*propose.lam[w])){ // Both suscetible
		auto n = both_susceptible_list[w].size(); if(n == 0) emsgEC("Chain",14);
		i = both_susceptible_list[w][(unsigned int)(ran()*n)];
	}
	else{                                                // Only proposed state susceptible
		auto n = propose_only_susceptible_list[w].size(); if(n == 0) emsgEC("Chain",15);
		i = propose_only_susceptible_list[w][(unsigned int)(ran()*n)];
	}
	
	change_susceptible_status(i,not_sus,1);              // Changes the susceptibility status
	
	propose.simulate_compartmental_transitions(i,0,t);   // Simulates compartmental transitions after infection

	propose.add_indev(i);
}


/// Simulates an event sequence given a parameter set (returns fail if it is found not to be possible)
Status Chain::simulate(const vector <double>& paramv)
{
	vector <double> zero(paramv.size());                  // Sets up a parameter set with all zeros
	for(auto& pval : zero) pval = 0;
	
	initial.set_param(zero);
	
	if(mbp(paramv) == fail) return fail;                  // Performs an MBP (this effectively simulates the system)
	
	initial.copy(propose);                                // Copies proposed state into initial
	
	return success;
}

/// Used for checking the code is running correctly
void Chain::check(double t, unsigned int sett) const
{
	for(auto i = 0u; i < data.popsize; i++){    // Checks stat is correct
	  auto w = data.ind[i].area*data.ndemocatpos + data.ind[i].dp;
		
		if(nboth_susceptible_list[w] != both_susceptible_list[w].size()) emsgEC("Chain",24);
		if(npropose_only_susceptible_list[w] != propose_only_susceptible_list[w].size()) emsgEC("Chain",25);
		if(nnot_susceptible_list[w] != not_susceptible_list[w].size()) emsgEC("Chain",26);
		
		if((initial.indev[i].size() == 0 || t < initial.indev[i][0].t) && propose.indev[i].size() == 0){
			if(susceptible_status[i] != both_sus) emsgEC("Chain",27);
			if(both_susceptible_list[w][susceptible_list_ref[i]] != i) emsgEC("Chain",28);
		}
		else{
			if((initial.indev[i].size() != 0 && t >= initial.indev[i][0].t) && propose.indev[i].size() == 0){
				if(susceptible_status[i] != ponly_sus) emsgEC("Chain",29);
				if(propose_only_susceptible_list[w][susceptible_list_ref[i]] != i) emsgEC("Chain",30);
			}
			else{
				if(susceptible_status[i] != not_sus) emsgEC("Chain",31);
				if(not_susceptible_list[w][susceptible_list_ref[i]] != i) emsgEC("Chain",32);
			}
		}
	}
	
	auto l = poptree.level-1;
	for(auto c = 0u; c < data.narea; c++){
		auto wmin = c*data.ndemocatpos, wmax = wmin + data.ndemocatpos;
	
		double sum = 0; 
		auto dp = 0u; 
		auto v = c*data.nage; 
		for(auto w = wmin; w < wmax; w++){
			auto a = data.democatpos[dp][0];
			
			double dd;
			dd = initial.lam[w] - initial.sus[dp]*(initial.beta*initial.areafac[c]*initial.Qmap[sett][v+a] + initial.phi); if(sqrt(dd*dd) > tiny) emsgEC("Chain",33);
			dd = propose.lam[w] - propose.sus[dp]*(propose.beta*propose.areafac[c]*propose.Qmap[sett][v+a] + propose.phi); if(sqrt(dd*dd) > tiny) emsgEC("Chain",34);
	
			auto dlam = nboth_susceptible_list[w]*(propose.lam[w] - initial.lam[w]); if(dlam < 0) dlam = 0;
			sum += dlam + npropose_only_susceptible_list[w]*propose.lam[w];
			dp++;
		}
		auto dd = new_infection_rate[l][c] - sum; if(sqrt(dd*dd) > tiny){ emsgEC("Chain",35);}
	}
	
	for(auto i = 0u; i < data.popsize; i++){
		for(auto tra = 0u; tra < model.trans.size(); tra++){
			if(indmap[i][tra] != 0) emsgEC("Chain",36);
		}
	}
}

/// Calculates propose.Qmap based on the initial and final sequences
void Chain::calcproposeQmap()
{	
	for(auto& dQma : dQmap) dQma = 0;
	
	for(auto sett = 0u; sett < details.nsettime; sett++){
		propose.set_Qmap_using_dQ(sett,initial,dQmap);
		update_dQmap(initial.trev[sett],propose.trev[sett]);	
	} 
}

/// Adds and removes infectious individuals
void Chain::stand_event_prop(double EFcut)
{	
	timers.timeaddrem -= clock();

	auto probif = 0.0, probfi = 0.0;
	
	propose.copy(initial);                                   // Copies initial state into proposed state
		
	for(const auto& x : propose.x) change_susceptible_status(x.ind,not_sus,0);
	 
	if(ran() < 0.5){                                         // Adds individuals
		timers.timembptemp2 -= clock();
		infsampler(initial.Qmap);
		timers.timembptemp2 += clock();
		
		for(auto j = 0u; j < jump.naddrem; j++){
			if(propose.x.size() >= model.infmax){ reset_susceptible_lists(); return;}
			
			auto sumtot = lamsum[data.nsettardp-1]; if(sumtot == 0) emsgEC("Chain",56);
			auto z = ran()*sumtot;
			
			//k = 0; while(k < data.nsettardp && z > lamsum[k]) k += 1;
			//if(k == data.nsettardp) emsg("pr"); 
			
			auto k = 0u; auto dk = data.nsettardp/10; if(dk == 0) dk = 1;
			do{
				while(k < data.nsettardp && z > lamsum[k]) k += dk;
				if(dk == 1) break;
				if(k >= dk) k -= dk;
				dk /= 10; if(dk == 0) dk = 1;
			}while(1 == 1);
			if(k >= data.nsettardp) emsgEC("Chain",57);
		
			if(k > 0){
				if(k >= lamsum.size()) emsgEC("Chain",58);
				if(!(z < lamsum[k] && z > lamsum[k-1])) emsgEC("Chain",59);
			}
			
			probif += log(lam[k]/sumtot);

			auto sett = k/data.nardp, w = k%data.nardp;
			
			if(nboth_susceptible_list[w] == 0){ reset_susceptible_lists(); return;}
			auto i = both_susceptible_list[w][int(ran()*nboth_susceptible_list[w])];
			probif += log(1.0/nboth_susceptible_list[w]);
		
			change_susceptible_status(i,not_sus,0);
			
			auto dt = details.settime[sett+1]-details.settime[sett];
			auto t = details.settime[sett] + ran()*dt;
			probif += log(1.0/dt);
			
			propose.simulate_compartmental_transitions(i,0,t);
		
			propose.add_indev(i);
			
			probfi += log(1.0/propose.x.size());
		}
		
		timers.timembptemp3 -= clock();
		propose.sort_x();
		calcproposeQmap();
		timers.timembptemp3 += clock();
	}
	else{    // Removes individuals
		vector <int> kst;
		for(auto j = 0u; j < jump.naddrem; j++){
			if(propose.x.size() == 0){ reset_susceptible_lists(); return;}
			
			auto l = int(ran()*propose.x.size());
			probif += log(1.0/propose.x.size());
			auto i = propose.x[l].ind;
			auto sett = (unsigned int)(details.nsettime*initial.indev[i][propose.x[l].e].t/details.period); 

			auto c = data.ind[i].area;
			auto w = c*data.ndemocatpos + data.ind[i].dp;
	
			auto dt = details.settime[sett+1]-details.settime[sett];

			probfi += log(1.0/dt);
			kst.push_back(sett*data.nardp + w);
			
			propose.indev[i].clear();
			
			propose.x[l] = propose.x[propose.x.size()-1];
			propose.x.pop_back();
			
			change_susceptible_status(i,both_sus,0);
			
			probfi += log(1.0/nboth_susceptible_list[w]);
		}
		
		for(auto& trev : propose.trev){  // Removes events in propose.trev
			auto j = 0u;
			auto jmax = trev.size();
			while(j < jmax){
				if(propose.indev[trev[j].ind].size() == 0){
					jmax--;
					trev[j] = trev[jmax];
					trev.pop_back();
				}
				else j++;
			}
		}
		
		timers.timembptemp3 -= clock();
		propose.sort_x();
		calcproposeQmap();
		timers.timembptemp3 += clock();
		
		timers.timembptemp2 -= clock();
		infsampler(propose.Qmap);
		timers.timembptemp2 += clock();
		
		auto sumtot = lamsum[data.nsettardp-1]; 
		for(const auto& ks : kst) probfi += log(lam[ks]/sumtot);
	}
	
	timers.timembptemp4 -= clock();
	propose.set_likelihood();
	
	double al;
	if(details.mode == abcmbp){
		propose.EF = obsmodel.Lobs(propose.trev,propose.indev);
		if(propose.EF > EFcut) al = 0;
		else al = exp(propose.Lev-initial.Lev + probfi - probif);
		if(checkon == 1) cout << al << " " << initial.EF << " " << propose.EF << " " << initial.Lev << " " << propose.Lev <<  " " << EFcut << "al" << endl;		
	}
	else{
		propose.L = obsmodel.Lobs(propose.trev,propose.indev);
		al = exp(invT*(propose.L-initial.L) + propose.Lev-initial.Lev + probfi - probif);
		if(checkon == 1) cout << al << " " << initial.L << " " << propose.L << " " << initial.Lev << " " << propose.Lev << "al" << endl;		
	}
	timers.timembptemp4 += clock();
	
	if(ran() < al){
		initial.Lev = propose.Lev;
		if(details.mode == abcmbp) initial.EF = propose.EF;
		else initial.L = propose.L;
		
		initial.trev = propose.trev;
		initial.Qmap = propose.Qmap;
		
		for(const auto& x : initial.x) initial.indev[x.ind].clear();
		for(const auto& x : propose.x) initial.indev[x.ind] = propose.indev[x.ind];
		
		initial.x = propose.x;
		jump.standev_accept();
	}
	else{
		jump.standev_reject();
	}
	
	reset_susceptible_lists();

	if(checkon == 1) initial.check();
	timers.timeaddrem += clock();
}

/// Generates a sampler for adding infected individuals into the system
void Chain::infsampler(const vector< vector<double> > &Qmap)
{
	auto sum = 0.0;
	for(auto sett = 0u; sett < details.nsettime; sett++){
		auto phi = initial.disc_spline[model.phispline_ref][sett]; 
		auto beta = initial.disc_spline[model.betaspline_ref][sett];
	
		for(auto c = 0u; c < data.narea; c++){
			auto fac = beta*initial.areafac[c];
			for(auto dp = 0u; dp < data.ndemocatpos; dp++){
				auto w = c*data.ndemocatpos + dp;
				auto tot = sett*data.nardp + w;
				auto v = c*data.nage + data.democatpos[dp][0];
				
				auto val = nboth_susceptible_list[w]*initial.sus[dp]*(fac*Qmap[sett][v] + phi);
				sum += val;
				
				lam[tot] = val;				
				lamsum[tot] = sum;
			}
		}
	}
}

/// Compresses the events to take up as little memory as possible (used for abcmbp)
vector <FEV> Chain::event_compress(const vector < vector <FEV> > &indev) const
{
	vector <FEV> store;
	for(const auto& inde : indev){
		for(const auto& ev : inde) store.push_back(ev);
	}
	
	return store;
}

/// Generates a particle (used for abcmbp)
void Chain::generate_particle(Particle &part) const
{
	part.EF = initial.EF;
	part.paramval = initial.paramval;
	part.ev = event_compress(initial.indev);
}

Status Chain::abcmbp_proposal(const vector <double> &paramv, double EFcut)  
{
	auto al = 1.0;
	for(auto th = 0u; th < model.param.size(); th++){
		if(paramv[th] < model.param[th].min || paramv[th] > model.param[th].max) al = 0;
		if(paramv[th] < model.param[th].min || paramv[th] > model.param[th].max) al = 0;
	}

	if(al == 1){

		if(mbp(propose.paramval) == fail) al = 0;
		else{
			propose.EF = obsmodel.Lobs(propose.trev,propose.indev);
			if(propose.EF >= EFcut) al = 0;
			else{
				propose.Pr = model.prior(propose.paramval);
				al = exp(propose.Pr-initial.Pr);
				//cout << al << " " << propose.Pr << " " << initial.Pr <<  "al\n";
			}
		}
	}
	
	if(ran() < al){
		initial.copy(propose);
		return success;
	}

	return fail;
}

void Chain::initialise_variables()
{
	setup_susceptible_lists();
	
	indmap.resize(data.popsize);
	for(auto i = 0u; i < data.popsize; i++){
		indmap[i].resize(model.trans.size());
		for(auto tra = 0u; tra < model.trans.size(); tra++) indmap[i][tra] = 0;
	}
	
	dQmap.resize(data.narage);                                                 // Initialises vectors
	
	dQbuf.resize(data.narage);
	for(auto v = 0u; v < data.narage; v++){
		dQbuf[v].resize(data.Q.size()); for(auto q = 0u; q < data.Q.size(); q++) dQbuf[v][q] = 0;
	}
	dQbuflistv.clear(); dQbuflistq.clear();
	
	
	new_infection_rate.resize(poptree.level); for(auto l = 0u; l < poptree.level; l++) new_infection_rate[l].resize(lev[l].node.size()); 
	N.resize(comp.size()); 
	
	popw.resize(data.nardp);                                        // Used for event based changes
	lam.resize(data.nsettardp); lamsum.resize(data.nsettardp);
}

/// Clears variables ready for a MBP
void Chain::mbp_init()
{
	for(auto c = 0u; c < comp.size(); c++) N[c] = 0;
	N[0] = data.popsize;
		
	propose.clear();
	
	for(auto v = 0u; v < data.narage; v++) dQmap[v] = 0;
	
	// Set to true if MBPs on compartmental transitions needed
	do_mbp_event = model.dombpevents(initial.paramval,propose.paramval); 
}	
	