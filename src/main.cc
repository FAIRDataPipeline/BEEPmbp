/*
Load mpi: module load mpi/openmpi-x86_64

Compile using: make

This lists a possible set of ways to run BEEPmbp. 
Note, the optiont "-n 20" can be replace by the number of CPU nodes available.

Simulation:                 
./beepmbp inputfile="examples/EX1.toml" mode="sim" 
	
Multiple simulations:       
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="multisim" nsimulation=100
OPTIONS: nsimulation

Prediction (uses posterior samples from inference to predict the future, with potential model modificiations):                 
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="prediction" 
OPTIONS: prediction_start, prediction_end, modification, nsim_per_sample

ABC-MBP inference:
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="abcmbp" nparticle=50 ngeneration=5 nrun=4
OPTIONS: nparticle, ngeneration / cutoff_final, nupdate, GR_max, nrun

PAIS inference:
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="pais" nparticle=50 ngeneration=5 nrun=4
OPTIONS: nparticle, ngeneration / invT_final, nupdate, GR_max, nrun

Simple ABC inference:
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="abc" nsample=100 cutoff_frac=0.1 nrun=4
OPTIONS: nsample / GR_max, cutoff / cutoff_frac, nrun

ABC-SMC inference:
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="abcsmc" ngeneration=5 cutoff_frac=0.5 nsample=200 nrun=4
OPTIONS: nsample / GR_max, ngeneration / cutoff_final, cutoff_frac, nrun

PMCMC inference:
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="pmcmc" nparticle=20 nsample=200
OPTIONS: nparticle, nsample / GR_max, invT, nburnin, nthin, nrun

MCMC-MBP inference:
mpirun -n 4 ./beepmbp inputfile="examples/EX1.toml" mode="mcmcmbp" invT=303 nsample=200 nrun=4
OPTIONS: nsample / GR_max, invT, nburnin, nthin, nrun

MC3 inference:
mpirun -n 20 ./beepmbp inputfile="examples/EX1.toml" mode="mc3" nchain=20 invT_final=303 nsample=200 nrun=4
OPTIONS: nchain, nsample / GR_max, invT_start, invT_final, nburnin, nquench, nthin, nrun

./beepmbp inputfile="examples/EX2.toml" mode="sim" 
mpirun -n 20 ./beepmbp inputfile="examples/EX2.toml" mode="abcmbp" nparticle=200 ngeneration=5 

./beepmbp inputfile="examples/EX_covid.toml" mode="sim"
mpirun -n 20 ./beepmbp inputfile="examples/EX_covid.toml" mode="abcmbp"  nparticle=20 ngeneration=3 

mpirun -n 20 ./beepmbp inputfile="examples/EX_covid.toml" mode="abcmbp"  nparticle=20 ngeneration=80 

./beepmbp inputfile="examples/EX1.toml" mode="sim" 

./beepmbp inputfile="examples/EX1temp.toml" mode="sim" 
mpirun -n 20 ./beepmbp inputfile="examples/EX1temp.toml" mode="abcmbp"  nparticle=20 ngeneration=20


./beepmbp inputfile="examples/EX2temp.toml" mode="sim" 
mpirun -n 20 ./beepmbp inputfile="examples/EX2.toml" mode="abcmbp"  nparticle=20 ngeneration=3 
mpirun -n 20 ./beepmbp inputfile="examples/EX2temp.toml" mode="abcmbp"  nparticle=20 ngeneration=3 

mpirun -n 20 ./beepmbp inputfile="examples/EX2temp.toml" mode="pais" nparticle=20 ngeneration=10

./beepmbp inputfile="examples/temp.toml" mode="sim" 

./beepmbp inputfile="examples/EX1.toml" mode="sim" 
./beepmbp inputfile="examples/EX2.toml" mode="sim" 

mpirun -n 20 ./beepmbp inputfile="examples/EX3.toml" mode="abcmbp" nparticle=20 ngeneration=10

./beepmbp inputfile="examples/EX_IZ.toml" mode="sim" 

mpirun -n 20 ./beepmbp inputfile="examples/EX_IZ.toml" mode="abcmbp" nparticle=20 ngeneration=3

./beepmbp inputfile="examples/Covid.toml" mode="sim" 

mpirun -n 20 ./beepmbp inputfile="examples/Covid_age.toml" mode="abcmbp" nparticle=20 ngeneration=3

./beepmbp inputfile="examples/Covid.toml" mode="sim" 

nohup mpirun -n 20 ./beepmbp inputfile="examples/Covid_spatial.toml" mode="abcmbp" nparticle=100 ngeneration=100 > oppp.txt&


mpirun -n 10 ./beepmbp inputfile="examples/Covid_spatial_IZ.toml" mode="abcmbp" nparticle=10 ngeneration=3

 ./beepmbp inputfile="examples/Covid_spatial_IZ.toml" mode="data"
 
 mpirun -n 20 ./beepmbp inputfile="examples/England.toml" mode="abcmbp" nparticle=20 ngeneration=3
 
 
 mpirun -n 20 ./beepmbp inputfile="examples/England_with_H.toml" mode="abcmbp" nparticle=20 ngeneration=3
 
  mpirun -n 20 ./beepmbp inputfile="examples/England_with_H_age.toml" mode="abcmbp" nparticle=20 ngeneration=3
	
	  mpirun -n 20 ./beepmbp inputfile="examples/England_with_H_age.toml" mode="pais" nparticle=20 ngeneration=3
		
	 nohup mpirun -n 20 ./beepmbp inputfile="examples/England_with_H_age.toml" mode="abcmbp" nparticle=100 ngeneration=120 > opq.txt &
	 
	  ./beepmbp inputfile="examples/England_with_H_age_sim.toml" mode="sim"
		
		 mpirun -n 20 ./beepmbp inputfile="examples/England_with_H_age_sim.toml" mode="abcmbp" nparticle=20 ngeneration=3
		 
		 mpirun -n 20 ./beepmbp inputfile="examples/England_with_H_age_2020.toml" mode="abcmbp" nparticle=20 ngeneration=3 
			
		 nohup mpirun -n 20 ./beepmbp inputfile="examples/England_with_H_age_2020.toml" mode="abcmbp" nparticle=40 ngeneration=120 > opq2.txt &
		 
			nohup  mpirun -n 20 ./beepmbp inputfile="examples/England_with_CH.toml" mode="abcmbp" nparticle=20 ngeneration=250 > g8.txt &
			
		
		mpirun -n 20 ./beepmbp inputfile="examples/England_Simple.toml" mode="abcmbp" nparticle=20 ngeneration=3
		
		nohup mpirun -n 20 ./beepmbp inputfile="examples/England_Simple.toml" mode="abcmbp" nparticle=200 ngeneration=100 > g13.txt &

nohup mpirun -n 20 ./beepmbp inputfile="examples/England_with_CH.toml" mode="abcmbp" nparticle=20 ngeneration=200 > g30.txt &
		
		
nohup mpirun -n 20 ./beepmbp inputfile="examples/England_with_CH_perturb.toml" mode="abcmbp" nparticle=20 ngeneration=200 > g31.txt &

nohup mpirun -n 20 ./beepmbp inputfile="examples/England_with_CH_perturb_simp.toml" mode="abcmbp" nparticle=20 ngeneration=200 > g32.txt &
		
		
	nohup 	mpirun -n 20 ./beepmbp inputfile="examples/England_with_CH.toml" mode="abcmbp" nparticle=100 ngeneration=300 > g43.txt &

		
	nohup 	mpirun -n 20 ./beepmbp inputfile="examples/England_with_CH_reduceGT.toml" mode="abcmbp" nparticle=20 ngeneration=250 > g44.txt &
	
*/

#include <iostream>
#include <sstream>
#include <math.h>
#include <map>
#include <algorithm>
#include <vector>
#include <iterator>
#include <signal.h>

#include "stdlib.h"
#include "time.h"

#include "inputs.hh"
#include "details.hh"
#include "data.hh"
#include "model.hh"
#include "output.hh"
#include "obsmodel.hh"

#include "utils.hh"
#include "timers.hh"

#include "simulate.hh"
#include "abc.hh"
#include "abcmbp.hh"
#include "abcsmc.hh"
#include "mc3.hh"
#include "pais.hh"
#include "pmcmc.hh"
#include "consts.hh"

#ifdef USE_Data_PIPELINE
#include "pybind11/embed.h"
#include "datapipeline.hh"
#endif

using namespace std;

string gitversion();

unsigned int signum;

void term(int sign)
{
  if(false) cout << sign << " " << signum << "Caught!" << endl;
}

int main(int argc, char** argv)
{
	timersinit();
	timer[TIME_TOTAL].start();
	
	struct sigaction action;
  action.sa_handler = term;
  sigaction(SIGTERM, &action, NULL);	
	
	#ifdef USE_MPI
  MPI_Init(&argc,&argv);
  #endif

	if(false){                                                 	// Outputs the git version
		cout << "BEEPmbp version " << gitversion() << endl << endl; 
	}	

	Inputs inputs(argc,argv);                                   // Loads command line arguments and TOML file into inputs

	Details details(inputs);                                    // Loads up various details of the model
	
	Mpi mpi(details);                                           // Stores mpi information (core and ncore)
	
	bool verbose = (mpi.core == 0);                             // Parameter which ensures that only core 0 outputs results
	
	if(verbose) cout << endl;
	
#ifdef USE_Data_PIPELINE                                      // Sets up data
	pybind11::scoped_interpreter guard{};

	using namespace pybind11::literals;

  pybind11::module::import("logging").attr("basicConfig")("level"_a="DEBUG", "format"_a="%(asctime)s %(filename)s:%(lineno)s %(levelname)s - %(message)s");

 	DataPipeline *dp = new DataPipeline(
		"dpconfig.yaml", "https://github.com/ScottishCovidResponse/BEEPmbp",
		gitversion());

	Data data(inputs,details,mpi,dp);   
#else
	Data data(inputs,details,mpi); 
#endif
	
	Model model(inputs,details,data,mpi);                       // Loads up the model
	
	auto seed = inputs.find_integer("seed",0);                  // Sets up the random seed

	switch(details.siminf){
	case SIMULATE: sran(mpi.core*10000+seed+10); break;
	default: sran(mpi.core*10000+seed+100); break;
	}
	
	ObservationModel obsmodel(details,data,model);              // Creates an observation model

	Output output(details,data,model,inputs,obsmodel,mpi);      // Creates an output class

	if(verbose) cout << endl << "Running...." << endl << endl;

	switch(details.mode){
	case SIM:                                                   // Performs a single simulation from the model 
		{		
			Simulate simu(details,data,model,inputs,output,obsmodel,mpi);
			simu.run();
		}
		break;
	
	case MULTISIM:                                              // Performs multiple simulations from the model
		{
			Simulate simu(details,data,model,inputs,output,obsmodel,mpi);
			simu.multisim();
		}
		break;
			
	case PREDICTION:                                            // Performs prediction from the model using posterior samples
		{
			Simulate simu(details,data,model,inputs,output,obsmodel,mpi);
			simu.model_modification();
		}
		break;
			
	case ABC_SIMPLE:                                            // Performs inference using a simple ABC rejection algorithm
		{	
			ABC abc(details,data,model,inputs,output,obsmodel,mpi);
			abc.run();
		}
		break;
		
	case ABC_SMC:                                               // Performs inference using the ABC-SMC algorithm
		{	
			ABCSMC abcsmc(details,data,model,inputs,output,obsmodel,mpi);
			abcsmc.run();
		}
		break;
		
	case ABC_MBP:                                               // Peforms inference using the ABC-MBP algorithm
		{	
			ABCMBP abcmbp(details,data,model,inputs,output,obsmodel,mpi);
			abcmbp.run();
		}
		break;
		
	case MC3_INF:                                               // Peforms inference using the MC3 algorithm
		{	
			MC3 mc3(details,data,model,inputs,output,obsmodel,mpi);
			mc3.run();
		}
		break;
		
	case MCMC_MBP:                                              // Peforms inference using the MCMC-MBP algorithm
		{	
			MC3 mc3(details,data,model,inputs,output,obsmodel,mpi);
			mc3.run();
		}
		break;
		
	case PAIS_INF:                                              // Peforms inference using the PAIS algorithm
		{	
			PAIS pais(details,data,model,inputs,output,obsmodel,mpi);
			pais.run();
		}
		break;
		
	case PMCMC_INF:                                             // Peforms inference using the PMCMC algorithm
		{
			PMCMC pmcmc(details,data,model,inputs,output,obsmodel,mpi);
			pmcmc.run();
		}
		break;

	case DATAONLY:
		{
			vector <Particle> particle_store;
			auto param = model.sample_from_prior();   
			State state(details,data,model,obsmodel);
			state.simulate(param);
			particle_store.push_back(state.create_particle(0));
			output.generate_graphs(particle_store); 
		}
		break;
		
	default: emsgroot("Mode not recognised"); break;
 	}
	
	timer[TIME_TOTAL].stop();
	
	if(details.siminf == INFERENCE){
		output_timers(details.output_directory+"/Diagnostics/CPU_timings.txt",mpi);
	}
	
	auto time_av = mpi.average(timer[TIME_TOTAL].val);
	if(verbose){
		inputs.print_commands_not_used();
		 
		cout <<  "Total time: " << prec(double(time_av)/(60.0*CLOCKS_PER_SEC),3) << " minutes." << endl;
	}
	
#ifdef USE_Data_PIPELINE
	delete dp;
#endif

#ifdef USE_MPI
	MPI_Finalize();
#endif
}
