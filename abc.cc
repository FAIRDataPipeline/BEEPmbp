/// Implements different types of ABC algorithms

#include <iostream>
#include <fstream>
#include <algorithm>

#include <assert.h>
#include <math.h>

#include "abc.hh"

#include "utils.hh"
#include "timers.hh"
#include "chain.hh"
#include "data.hh"
#include "output.hh"
#include "obsmodel.hh"
#include "pack.hh"
#include "timers.hh"

using namespace std;

struct PartEF                                                              // Structure used to order particle EFs
{
	unsigned int i;                                                          // The number of the particle
	double EF;                                                               // The error function
};

bool PartEF_ord (PartEF p1,PartEF p2) { return (p1.EF < p2.EF); }          // Used to order by EF

/// Initilaises the ABC class
ABC::ABC(const Details &details, const DATA &data, const MODEL &model, const POPTREE &poptree, const Mpi &mpi, const Inputs &inputs, const Output &output, const Obsmodel &obsmodel) : details(details), data(data), model(model), poptree(poptree), mpi(mpi), output(output), obsmodel(obsmodel)
{	
	total_time = inputs.find_int("cputime",UNSET);  
	
	G = inputs.find_int("ngeneration",UNSET);                  
	
	if(total_time == UNSET && G == UNSET) emsgroot("The algorithm must be limited by either 'cputime' or 'generation'");

	Ntot = inputs.find_int("nparticle",UNSET);                                            // Sets the total number of mcmc chains
	if(Ntot == UNSET) emsgroot("The number of particles must be set");
	if(Ntot%mpi.ncore != 0) emsgroot("The number of particles must be a multiple of the number of cores");

	N = Ntot/mpi.ncore;                                                           // The number of particles per core
	if(N == 0) emsgroot("'nparticle' must be non-zero");
	
	param_not_fixed.clear();                                                           // Finds the list of model parameteres that change
	for(auto th = 0u; th < model.param.size(); th++){
		if(model.param[th].min != model.param[th].max) param_not_fixed.push_back(th);
	}
	nvar = param_not_fixed.size();
	
	jumpv.resize(nvar); for(auto v = 0u; v < nvar; v++) jumpv[v] = 1;
}

/// Implements a version of abc which uses model-based proposals in MCMC
void ABC::mbp()
{
	Chain chain(details,data,model,poptree,obsmodel,0);
	
	vector <Particle> part;					
	part.resize(N);
	
	unsigned int partcopy[Ntot];

	vector <Generation> generation; 
	
	for(auto g = 0u; g < G; g++){
		timers.timeabc -= clock();

		Generation gen;
		gen.time = clock();
		
		if(g == 0){
			gen.EFcut = large;
			
			for(auto& pa : part){ 
				chain.sample_from_prior();
				double EF = obsmodel.Lobs(chain.propose.trev,chain.propose.indev);

				pa.paramval = chain.paramval;
				pa.EF = EF;
				pa.ev = chain.event_compress(chain.propose.indev);
				
				gen.param_samp.push_back(chain.paramval);
				gen.EF_samp.push_back(EF);
			}
		}
		else{
			gen.EFcut = next_generation_mpi(part,partcopy);
			
			mcmc_updates(gen,part,chain);

			if(mpi.core == 0) cout << "Generation " << g << ": EFcut " << gen.EFcut << endl;
		}

		generation.push_back(gen);

		exchange_samples_mpi(generation[g]);	
		cholesky(generation[g].param_samp);
			
		timers.timeabc += clock();
		
		if(g%5 == 0){
			if(mpi.core == 0){
				string file = "generation_mbp.txt";
				output.generation_plot(file,generation);
			}
		}
		
		if(g%5 == 0){
			results_mpi(generation,part,chain);
			if(mpi.core == 0){
				cout << int((100.0*timers.timeabcprop)/timers.timeabc) << "% CPU time on proposals\n";
				cout << int((100.0*timers.timestandard)/timers.timeabc) << "% CPU time on standard\n";
			}
		}

		if(g%5 == 0 && mpi.core == 0){
			string file = details.outputdir+"/M.txt";
			ofstream Mpl(file.c_str());
		
			for(auto i1 = 0u; i1 < nvar; i1++){
				for(auto i2 = 0u; i2 < nvar; i2++){
					Mpl << M[i1][i2] << " ";
				}
				Mpl << "M\n";
			}		
			
			Mpl << "coorela\n";
			for(auto i1 = 0u; i1 < nvar; i1++){
				for(auto i2 = 0u; i2 < nvar; i2++){
					if(i1 == i2) Mpl  << "--- ";
					else Mpl << M[i1][i2]/sqrt(M[i1][i1]*M[i2][i2]) << " ";
				}
				Mpl << "M\n";
			}		
		}
	
	
	
		double timetaken = timers.timeabc/(60.0*CLOCKS_PER_SEC);
		
		MPI_Bcast(&timetaken,1,MPI_DOUBLE,0,MPI_COMM_WORLD);

		if(timetaken > total_time) break;
	}
	
	if(mpi.core == 0) cout << int((100.0*timers.timeabcprop)/timers.timeabc) << "% CPU time on proposals\n";

	results_mpi(generation,part,chain);
	
	if(mpi.core == 0){
		string file = "model_evidence_mbp.txt";
		output.model_evidence_plot(file,generation);
	}	
}
 
/// This is an implementation of an ABC-SMC algorithm, which is used to compare against the MBP-MCMC approach 
void ABC::smc()
{	
	Chain chain(details,data,model,poptree,obsmodel,0);
	
	const double jump = 1;
	
	vector <Generation> generation; 
	
	auto nparam = model.param.size();

	for(auto g = 0u; g < G; g++){
		timers.timeabc -= clock();
			
		Generation gen;
		gen.time = clock();
		if(g == 0){
			for(auto i = 0u; i < N; i++){     // For the first generation 
				chain.sample_from_prior();
				
				gen.param_samp.push_back(chain.paramval);
				gen.EF_samp.push_back(obsmodel.Lobs(chain.propose.trev,chain.propose.indev));
			}
		}
		else{
			Generation &gen_last = generation[g-1];
			
			cholesky(gen_last.param_samp);      // Generated matrix for sampling MVN samples
		
			double EFcut = gen_last.EFcut;

			double sumst[Ntot];	           // Generate particle sampler
			double sum = 0; for(auto i = 0u; i < Ntot; i++){ sum += gen_last.w[i]; sumst[i] = sum;}
			
			double EF=UNSET;
			auto ntr = 0u;
			auto nac = 0u;
			for(auto i = 0u; i < N; i++){ 
				unsigned int fl;
				do{
					fl = 0;
					double z = ran()*sum; unsigned int k = 0; while(k < Ntot && z > sumst[k]) k++;
					if(k == Ntot) emsg("Problem");
			
					vector <double> param_propose = gen_last.param_samp[k];
					cholesky_propose(param_propose,jump);
					
					for(auto th = 0u; th < nparam; th++){
						if(param_propose[th] < model.param[th].min || param_propose[th] > model.param[th].max) fl = 1;
					}
				
					if(fl == 0){
						fl = chain.simulate(param_propose);
						if(fl == 0){
							EF = obsmodel.Lobs(chain.propose.trev,chain.propose.indev);
							if(EF > EFcut) fl = 1;
						}
					}
					ntr++;
				}while(fl == 1);
				nac++;
				
				gen.param_samp.push_back(chain.paramval);
				gen.EF_samp.push_back(EF);
			}
			
			if(mpi.core == 0) cout << "Generation " << g <<  ":  Cuttoff " << EFcut << "  Acceptance " << double(nac)/ntr << "  Neff " << Neff(gen_last.w) << endl;
		}
		
		exchange_samples_mpi(gen);	   // Copies parameter and EF samples across cores
		
		generation.push_back(gen);
		
		calculate_w(generation,jump);  // Calculates the weights for the next generation
		
		timers.timeabc += clock();
			
		if(mpi.core == 0){
			string file = "generation_smc.txt";
			output.generation_plot(file,generation);
		}
		
		if(timers.timeabc/(60.0*CLOCKS_PER_SEC) > total_time) break;
	}

	if(mpi.core == 0){
		string file = "Posterior_parameters.txt";
		output.plot_distribution(file,generation[generation.size()-1]);
	}	
}

/// Updates particles using MBPs
void ABC::mcmc_updates(Generation &gen, vector <Particle> &part, Chain &chain)
{	
	const auto p_mbp = 0.9;
	const auto sampstep = 5u;
	const double beta = 2;
	const double facup = 1.2, facdown = 0.8;

	unsigned int ntr_v[nvar], nac_v[nvar];
	for(auto v = 0u; v < nvar; v++){ ntr_v[v] = 0; nac_v[v] = 0;}
	
	double EFcut = gen.EFcut;
	
	auto jmax = 0u;
	for(auto& ju : jumpv){
		auto num = 1.0/(ju*ju);
		if(num < 1) num = 1;
		jmax += beta*num;
	}
	
	if(jmax < 1) jmax = 1;
	if(mpi.core == 0) cout << jmax << "jmax\n";
	
	for(auto& pa : part){
		chain.initialise_from_particle(pa);

		for(auto j = 0u; j < jmax; j++){
			if(j%sampstep == 0){
				gen.param_samp.push_back(chain.paramval);				
				gen.EF_samp.push_back(chain.initial.EF);
			}
			
			if(ran() < p_mbp){
				auto v = (unsigned int)(ran()*nvar);
				auto th = param_not_fixed[v];
					
				vector <double> param_propose = chain.paramval;
				param_propose[th] += normal(0,jumpv[v]*sqrt(M[v][v]));
						
				timers.timeabcprop -= clock();
				ntr_v[v]++;
				if(chain.abcmbp_proposal(param_propose,EFcut) == 1) nac_v[v]++;
				timers.timeabcprop += clock();
			}
			else{
				chain.standard_prop(0,1,EFcut);
			}
		}
			
		chain.generate_particle(pa);
	}	
			
	for(auto v = 0u; v < nvar; v++){
		double ac_rate = acceptance(double(nac_v[v])/(double(ntr_v[v])+0.01));
		
		if(ac_rate > 0.4){ jumpv[v] *= facup; if(jumpv[v] > 2) jumpv[v] = 2;}
		else{
			if(ac_rate < 0.3) jumpv[v] *= facdown; 
		}
	}
}

/// Finds the effective number of particles
unsigned int ABC::Neff(vector <double> w)
{
	sort(w.begin(),w.end());

	double sum = 0; for(auto i = 0u; i < w.size(); i++) sum += w[i];
	double sum2 = 0; auto i = 0u; while(i < w.size() && sum2 < sum/2){ sum2 += w[i]; i++;}
	return 2*(1+w.size()-i); 
}

/// Calculates the weights for the different particles
void ABC::calculate_w(vector <Generation> &generation, double jump)
{
	unsigned int g = generation.size()-1;
	Generation &gen = generation[g];

	auto Ntot = gen.param_samp.size();
	auto Ncut = int(0.5*Ntot);
		
	double ef[Ntot];
	for(auto i = 0u; i < Ntot; i++) ef[i] = gen.EF_samp[i];
	sort(ef,ef+Ntot);
	double EFcut = ef[Ncut];
	
	gen.EFcut = EFcut;
	gen.w.resize(Ntot);		
		
	if(g == 0){
		for(auto i = 0u; i < Ntot; i++){
			double w;
			if(gen.EF_samp[i] >= EFcut) w = 0; else w = 1;
			gen.w[i] = w;
		}
	}
	else{
		Generation &gen_last = generation[g-1];

		for(auto i = 0u; i < Ntot; i++){
			double w;
			if(gen.EF_samp[i] >= EFcut) w = 0;
			else{
				
				double sum = 0;
				for(auto j = 0u; j < Ntot; j++)	sum += gen_last.w[j]*mvn_prob(gen.param_samp[i],gen_last.param_samp[j],jump);
			
				w = exp(model.prior(gen.param_samp[i]))/sum;
			}
			gen.w[i] = w;
		}	
	}
	
	double wsum = 0;
	for(auto i = 0u; i < Ntot; i++) wsum += gen.w[i];
	wsum /= Ncut;
	for(auto i = 0u; i < Ntot; i++) gen.w[i] /= wsum;
}

/// Calculate a measure of how well a generation is mixed (by comparing the similarity between the two sets of copied particles)
double ABC::mix(const vector <Particle> &part, unsigned int *partcopy) const
{
	auto nparam = model.param.size();
	auto nparamgen = nparam*N;
	auto nparamgentot = nparam*Ntot;
	double paramval[nparamgen], paramvaltot[nparamgentot];
	
	for(auto p = 0u; p < N; p++){ 
		for(auto th = 0u; th < nparam; th++){
			paramval[p*nparam+th] = part[p].paramval[th];
		}
	}
	
	MPI_Gather(paramval,nparamgen,MPI_DOUBLE,paramvaltot,nparamgen,MPI_DOUBLE,0,MPI_COMM_WORLD);
	
	double mix = 0;
	if(mpi.core == 0){
		vector < vector <double> > between;
		for(auto i = 0u; i < Ntot; i++){
			auto p = partcopy[i];
			if(p != UNSET){
				vector <double> vec; for(auto th = 0u; th < nparam; th++) vec.push_back(paramvaltot[p*nparam+th] - paramvaltot[i*nparam+th]);
				between.push_back(vec);
			}
		}
		vector <double> var_between	= variance_vector(between);
	
		vector < vector <double> > across;
		for(auto i = 0u; i < 10*Ntot; i++){
			unsigned int p1, p2;
			p1 = (unsigned int)(Ntot*ran());
			do{	p2 = (unsigned int)(Ntot*ran());}while(p2 == p1);
			vector <double> vec; for(auto th = 0u; th < nparam; th++) vec.push_back(paramvaltot[p1*nparam+th] - paramvaltot[p2*nparam+th]);
			across.push_back(vec);
		}
		vector <double> var_across = variance_vector(across);
		
		for(auto i = 0u; i < nvar; i++) mix += sqrt(var_between[i]/var_across[i]);
		mix /= nvar;
	}

	return mix;
}

/// Calculates the acceptance rate averaged across cores
double ABC::acceptance(double rate) const 
{
	double ratetot[mpi.ncore];
	MPI_Gather(&rate,1,MPI_DOUBLE,ratetot,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
	if(mpi.core == 0){
		rate = 0;
		for(auto co = 0u; co < mpi.ncore; co++) rate += ratetot[co];
		rate /= mpi.ncore;
	}
	MPI_Bcast(&rate,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
	
	return rate;	
}

/// Gets a sample from a particle
SAMPLE ABC::get_sample(const Particle &part, Chain &chain) const
{
	SAMPLE sample;
	chain.initialise_from_particle(part);
	sample.meas = obsmodel.getmeas(chain.initial.trev,chain.initial.indev);;
	sample.R0 = model.R0calc(chain.paramval);
	sample.phi = model.create_disc_spline(model.phispline_ref,chain.paramval); 
	
	return sample;
}

/// Gathers together results from all the cores
void ABC::results_mpi(const vector <Generation> &generation, const vector <Particle> &part, Chain &chain) const
{
	auto g = generation.size()-1;
	const Generation &gen = generation[g];
	
	auto N = part.size();
	
	if(mpi.core == 0){
		vector <PARAMSAMP> psamp;      // Stores parameter samples
		for(auto& samp : gen.param_samp){
			PARAMSAMP psa; 
			psa.paramval = samp;
			psamp.push_back(psa);
		}
		
		vector <SAMPLE> opsamp;        // Stores output samples
		for(const auto& pa : part) opsamp.push_back(get_sample(pa,chain));
		
		for(auto co = 1u; co < mpi.ncore; co++){
			unsigned int si;
			MPI_Recv(&si,1,MPI_UNSIGNED,co,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			packinit(si);
			MPI_Recv(packbuffer(),si,MPI_DOUBLE,co,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			
			for(auto i = 0u; i < N; i++){
				Particle part;
				unpack(part);
				opsamp.push_back(get_sample(part,chain));
			}
		}
		
		output.results(psamp,opsamp);
	}
	else{
		packinit(0);
		for(auto i = 0u; i < N; i++) pack(part[i]);
		
		unsigned int si = packsize();
		MPI_Send(&si,1,MPI_UNSIGNED,0,0,MPI_COMM_WORLD);
		MPI_Send(packbuffer(),si,MPI_DOUBLE,0,0,MPI_COMM_WORLD);
	}
}

/// Uses mpi to swap parameter and EF samples across all chains
void ABC::exchange_samples_mpi(Generation &gen)
{
	if(mpi.core == 0){
		for(auto co = 1u; co < mpi.ncore; co++){
			unsigned int si;
			MPI_Recv(&si,1,MPI_UNSIGNED,co,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			packinit(si);
			MPI_Recv(packbuffer(),si,MPI_DOUBLE,co,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			vector< vector <double> > paramsa;
			unpack(paramsa);
			for(auto i = 0u; i < paramsa.size(); i++) gen.param_samp.push_back(paramsa[i]);
			
			vector <double> EFsa;
			unpack(EFsa);
			for(auto i = 0u; i < EFsa.size(); i++) gen.EF_samp.push_back(EFsa[i]);
		}
	}
	else{
		packinit(0);
		pack(gen.param_samp);
		pack(gen.EF_samp);
		
		unsigned int si = packsize();
		MPI_Send(&si,1,MPI_UNSIGNED,0,0,MPI_COMM_WORLD);
		MPI_Send(packbuffer(),si,MPI_DOUBLE,0,0,MPI_COMM_WORLD);
	}
	
	unsigned int si;
	if(mpi.core == 0){
		packinit(0);
		pack(gen.param_samp);
		pack(gen.EF_samp);
		si = packsize();
	}
	
	MPI_Bcast(&si,1,MPI_UNSIGNED,0,MPI_COMM_WORLD);
	if(mpi.core != 0) packinit(si);
	MPI_Bcast(packbuffer(),si,MPI_DOUBLE,0,MPI_COMM_WORLD);
	if(mpi.core != 0){ unpack(gen.param_samp); unpack(gen.EF_samp);}
}

/// Uses mpi to get the next generation of particles. It returns the EF cutoff used
double ABC::next_generation_mpi(vector<Particle> &part, unsigned int *partcopy)
{
	auto N = part.size();
	const auto Ntot = N*mpi.ncore;
	
	double EF[N], EFtot[Ntot];
		
	for(auto i = 0u; i < N; i++) EF[i] = part[i].EF; 
	
	MPI_Gather(EF,N,MPI_DOUBLE,EFtot,N,MPI_DOUBLE,0,MPI_COMM_WORLD);
	
	double EFcut;
	if(mpi.core == 0){
		PartEF partef[Ntot];
		for(auto i = 0u; i < Ntot; i++){ partef[i].i = i; partef[i].EF = EFtot[i];}
		
		sort(partef,partef+Ntot,PartEF_ord);
		
		if(Ntot%2 != 0) emsg("The number of particles must be even.");
		
		auto mid = Ntot/2;
		EFcut = 0.5*(partef[mid-1].EF + partef[mid].EF);
		
		for(auto j = 0u; j < Ntot; j++){
			auto i = partef[j].i;
			if(j < mid) partcopy[i] = UNSET;
			else partcopy[i] = partef[j-mid].i;
		}
	}
	MPI_Bcast(&EFcut,1,MPI_DOUBLE,0,MPI_COMM_WORLD);
		
	MPI_Bcast(partcopy,Ntot,MPI_INT,0,MPI_COMM_WORLD);
	
	vector< vector<double> > sendbuffer;
	vector< vector<double> > recibuffer;
	vector <unsigned int> sendbuffer_to;
	vector <unsigned int> recibuffer_from;
	vector <unsigned int> recibuffer_to;
		
	unsigned int buffersize[N], buffersizetot[Ntot];

	for(auto i = 0u; i < N; i++){
		buffersize[i] = 0;
		
		auto itot = mpi.core*N+i;
		if(partcopy[itot] == UNSET){
			for(auto iitot = 0u; iitot < Ntot; iitot++){
				if(partcopy[iitot] == itot && iitot/N != itot/N){
					packinit(0);
					pack(part[i]);
		
					sendbuffer.push_back(copybuffer()); 
					sendbuffer_to.push_back(iitot);
				
					buffersize[i] = packsize();
					//cout << "send " << itot << " -> " << iitot << " \n";
				}				
			}
		}
		else{
			auto iitot = partcopy[itot];
			if(iitot/N == itot/N){
				part[i] = part[iitot%N]; //cout << "internal copy " <<iitot << " -> " << itot << "\n";
			}
			else{
				recibuffer.push_back(vector <double> ());
				recibuffer_from.push_back(iitot);
				recibuffer_to.push_back(itot);
				//cout << "receive " << iitot << " -> " << itot << " \n"; 
			}
		}
	}
	
	MPI_Gather(buffersize,N,MPI_UNSIGNED,buffersizetot,N,MPI_UNSIGNED,0,MPI_COMM_WORLD);
	MPI_Bcast(buffersizetot,Ntot,MPI_UNSIGNED,0,MPI_COMM_WORLD);
		
	auto buftot = recibuffer.size()+sendbuffer.size();
	if(buftot > 0){
		MPI_Request reqs[buftot];                 // These are information used Isend and Irecv
		MPI_Status stats[buftot];
		unsigned int nreqs = 0;

		for(auto j = 0u; j < recibuffer.size(); j++){
			auto from = recibuffer_from[j];
			auto to = recibuffer_to[j];
			recibuffer[j].resize(buffersizetot[from]);
			MPI_Irecv(&recibuffer[j][0],recibuffer[j].size(),MPI_DOUBLE,from/N,to,MPI_COMM_WORLD,&reqs[nreqs]); nreqs++;
		}
		
		for(auto j = 0u; j < sendbuffer.size(); j++){
			auto to = sendbuffer_to[j];
			MPI_Isend(&sendbuffer[j][0],sendbuffer[j].size(),MPI_DOUBLE,to/N,to,MPI_COMM_WORLD,&reqs[nreqs]); nreqs++;		
		}	
	
		if(MPI_Waitall(nreqs,reqs,stats) != MPI_SUCCESS) emsgEC("ABC",1);
		
		for(auto j = 0u; j < recibuffer.size(); j++){
			setbuffer(recibuffer[j]);
			unpack(part[recibuffer_to[j]%N]);
		}
	}
	
	return EFcut;
}

/// Generates a covariance matrix from a sets of parameter samples
vector <double> ABC::variance_vector(const vector <vector <double> > &param_samp) const 
{
	vector <double> vec;
	
	auto N = param_samp.size();                             // Generates the covariance matrix

	vec.resize(nvar);
	for(auto i = 0u; i < nvar; i++){
		auto th = param_not_fixed[i]; 
	
		double av = 0;
		for(auto k = 0u; k < N; k++) av += param_samp[k][th];
		av /= N;
		
		double av2 = 0;
		for(auto k = 0u; k < N; k++){
			double val = param_samp[k][th] - av;
			av2 += val*val;
		}		
		vec[i] = av2/N;
	}
	
	return vec;
}

/// Generates a covariance matrix from a sets of parameter samples
vector <vector <double> > ABC::covariance_matrix(const vector <vector <double> > &param_samp) const 
{
	vector <vector <double> > M;
	
	auto N = param_samp.size();                             // Generates the covariance matrix

	M.resize(nvar);
	for(auto i1 = 0u; i1 < nvar; i1++){
		M[i1].resize(nvar);
		auto th1 = param_not_fixed[i1]; 
		for(auto i2 = 0u; i2 < nvar; i2++){
			auto th2 = param_not_fixed[i2];
			
			double av1 = 0, av2 = 0;
			for(auto k = 0u; k < N; k++){
				double val1 = param_samp[k][th1];
				double val2 = param_samp[k][th2];
				av1 += val1;
				av2 += val2;
			}
			av1 /= N; av2 /= N; 
			
			double av12 = 0;
			for(auto k = 0u; k < N; k++){
				double val1 = param_samp[k][th1] - av1;
				double val2 = param_samp[k][th2] - av2;
				av12 += val1*val2;
			}		
			M[i1][i2] = av12/N;
		
			if(std::isnan(M[i1][i2])) emsg("not");
			if(i1 == i2 && M[i1][i2] < 0) emsg("Negative");
		}
	}
	
	return M;
}

/// Calculates a lower diagonal matrix used in Cholesky decomposition
void ABC::cholesky(const vector <vector <double> > &param_samp)
{
	M = covariance_matrix(param_samp);
	
	/*
	cout << endl << endl;
	for(auto i1 = 0u; i1 < nvar; i1++){
		for(auto i2 = 0u; i2 < nvar; i2++){
			cout << M[i1][i2] << " ";
		}
		cout << "M\n";
	}
	*/
	//emsg("P");
	
	auto fl=0u;
	do{
		vector <vector <double> > A;
		A.resize(nvar);
		Zchol.resize(nvar);
		for(auto v1 = 0u; v1 < nvar; v1++){
			A[v1].resize(nvar);
			Zchol[v1].resize(nvar);
			for(auto v2 = 0u; v2 < nvar; v2++){
				A[v1][v2] = M[v1][v2];
				if(v1 == v2) Zchol[v1][v2] = 1; else Zchol[v1][v2] = 0;
			}
		}

		double Lch[nvar][nvar];
		double Tch[nvar][nvar];
		for(auto i = 0u; i < nvar; i++){
			for(auto v1 = 0u; v1 < nvar; v1++){
				for(auto v2 = 0u; v2 < nvar; v2++){
					if(v1 == v2) Lch[v1][v2] = 1; else Lch[v1][v2] = 0;
				}
			}

			double aii = A[i][i];
			Lch[i][i] = sqrt(aii);
			for(auto j = i+1; j < nvar; j++){
				Lch[j][i] = A[j][i]/sqrt(aii);
			}

			for(auto ii = i+1; ii < nvar; ii++){
				for(auto jj = i+1; jj < nvar; jj++){
					A[jj][ii] -= A[ii][i]*A[jj][i]/aii;
				}
			}
			A[i][i] = 1;
			for(auto j = i+1; j < nvar; j++){ A[j][i] = 0; A[i][j] = 0;}

			for(auto v1 = 0u; v1 < nvar; v1++){
				for(auto v2 = 0u; v2 < nvar; v2++){
					double sum = 0u; for(auto ii = 0u; ii < nvar; ii++) sum += Zchol[v1][ii]*Lch[ii][v2];
					Tch[v1][v2] = sum;
				}
			}

			for(auto v1 = 0u; v1 < nvar; v1++){
				for(auto v2 = 0u; v2 < nvar; v2++){
					Zchol[v1][v2] = Tch[v1][v2];
				}
			}
		}
		
		fl = 0;
		for(auto v1 = 0u; v1 < nvar; v1++){
			for(auto v2 = 0u; v2 < nvar; v2++){
				if(std::isnan(Zchol[v1][v2])) fl = 1;
			}
		}
		
		if(fl == 1){    // Reduces correlations to allow for convergence
			for(auto v1 = 0u; v1 < nvar; v1++){
				for(auto v2 = 0u; v2 < nvar; v2++){
					if(v1 != v2) M[v1][v2] /= 2;
				}
			}		
			
			/*
			for(auto v1 = 0u; v1 < nvar; v1++){
				for(auto v2 = 0u; v2 < nvar; v2++){
					cout << M[v1][v2] << " ";
				}
				cout << "M\n";
			}	
			*/			
			cout << "Cholesky converegence" << endl;
		}
	}while(fl == 1);
	
	inv = invert_matrix(M);
}

/// Inverts a matrix
vector <vector <double> > ABC::invert_matrix(const vector <vector <double> > &mat) const    // inverts the matrix
{
	unsigned int nvar = mat.size();
	vector <vector <double> > inv;
	
	double A2[nvar][nvar];

	inv.resize(nvar);
  for(auto i = 0u; i < nvar; i++){
		inv[i].resize(nvar);
    for(auto j = 0u; j < nvar; j++){
      A2[i][j] = mat[i][j];
      if(i == j) inv[i][j] = 1; else inv[i][j] = 0;
    }
  }

  for(auto ii = 0u; ii < nvar; ii++){
    double r = A2[ii][ii];
    for(auto i = 0u; i < nvar; i++){
      A2[ii][i] /= r; inv[ii][i] /= r; 
    }

    for(auto jj = ii+1; jj < nvar; jj++){
      double r = A2[jj][ii];
      for(auto i = 0u; i < nvar; i++){ 
        A2[jj][i] -= r*A2[ii][i];
        inv[jj][i] -= r*inv[ii][i];
      }
    }
  }

  for(int ii = nvar-1; ii > 0; ii--){
    for(int jj = ii-1; jj >= 0; jj--){
      double r = A2[jj][ii];
      for(auto i = 0u; i < nvar; i++){ 
        A2[jj][i] -= r*A2[ii][i];
        inv[jj][i] -= r*inv[ii][i];
      }
    }
  }

	// check inverse
	/*
	for(auto j = 0u; j < nvar; j++){
		for(auto i = 0u; i < nvar; i++){
			double sum = 0; for(auto ii = 0u; ii < nvar; ii++) sum += mat[j][ii]*inv[ii][i];
			cout << sum << "\t"; 
		}
		cout << " kk\n";
	}
	*/
	
	return inv;
}

/// The probability of drawing a vector from a MVN distribution
double ABC::mvn_prob(const vector<double> &pend,const vector<double> &pstart, double fac) const
{
	double sum = 0;
	for(auto v1 = 0u; v1 < nvar; v1++){
		for(auto v2 = 0u; v2 < nvar; v2++){
			double val1 = pend[param_not_fixed[v1]] - pstart[param_not_fixed[v1]];
			double val2 = pend[param_not_fixed[v2]] - pstart[param_not_fixed[v2]];
			sum += (val1/fac)*inv[v1][v2]*(val2/fac);
		}
	}	
	return exp(-0.5*sum);
}

/// Generates a proposed set of parameters from a MVN distribution
void ABC::cholesky_propose(vector <double> &paramval, double fac)
{
	double norm[nvar];	
	for(auto v = 0u; v < nvar; v++) norm[v] = normal(0,1);
	
  for(auto v = 0u; v < nvar; v++){
		double dva = 0; for(auto v2 = 0u; v2 <= v; v2++) dva += Zchol[v][v2]*norm[v2];

		auto th = param_not_fixed[v];
		paramval[th] += fac*dva;
	}
}
