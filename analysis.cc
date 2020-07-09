/*
Load mpi: module load mpi/openmpi-x86_64
Compile using: make

Simulation:  
 ./run inputfile="sim.toml"        

Inference:    
mpirun -n 1 ./run inputfile="inf.toml"
*/

/*
 Commands for running on DiRAC:
 ./submit-csd3 --groups 65536 --seed 1 --samples 10 --nprocs 32 --nnode 1  --walltime 1:00:00 --dir ~/rds/rds-dirac-dc003/dc-pool1/test
 dos2unix submit-csd3
 squeue -u dc-pool1
 scancel <jobid>
 mybalance
*/

#include <iostream>
#include <sstream>
#include <math.h>
#include <map>
#include <algorithm>
#include <vector>
#include <iterator>

#include "toml11/toml.hpp"

#include "stdlib.h"
#include "time.h"

#include "utils.hh"
#include "timers.hh"
#include "poptree.hh"
#include "model.hh"
#include "data.hh"

#include "simulate.hh"
#include "PMCMC.hh"
#include "gitversion.hh"
#include "MBP.hh"
#include "consts.hh"

using namespace std;
 


string lookup_string_parameter(const map<string,string> &params,
															 const toml::basic_value<::toml::discard_comments, std::unordered_map, std::vector> &tomldata,
															 const string &key, bool verbose, const string &def="")
{
	string val;
	auto val_it = params.find(key);
	if (val_it != params.end()) {
		val = val_it->second;
	} else {
		if (tomldata.contains(key)) {
			val = toml::find<string>(tomldata,key);
		} else {
			val = def;
			// emsgroot("ERROR: Parameter \'"+key+"\' must be supplied");
		}
	}
	if (verbose)
		cout << "  " << key << " = " << val << endl;
	return val;
}


int lookup_int_parameter(const map<string,string> &params,
												 const toml::basic_value<::toml::discard_comments, std::unordered_map, std::vector> &tomldata,
												 const string &key, bool verbose, int def=-1)
{
	int val;
	auto val_it = params.find(key);
	if (val_it != params.end()) {
		const std::string& valstr = val_it->second;
		try {
			size_t idx;
			val = stoi(valstr,&idx);
			if (idx != valstr.length()) {
				std::ostringstream oss;
				oss << "Should be integer, found '"<< valstr;
				throw std::invalid_argument(oss.str());
			}
		} catch (const std::exception& e) {
			std::ostringstream oss;
			oss << "Bad command-line parameter value for key '"<< key <<"'\n";
			// Add exception description if it's informative
			std::string what = e.what();
			if (what == "stoi") {
				if (valstr == "")
					oss << "Should be integer, found no value\n";
			} else {
				oss << what;
			}
			emsg(oss.str());
#ifdef USE_MPI
			MPI_Finalize();
#endif
			exit(EXIT_FAILURE);
		}
	} else {
		if (tomldata.contains(key)) {
			val = toml::find<int>(tomldata,key);
		} else {
			val = def;
			// emsgroot("ERROR: Parameter \'"+key+"\' must be supplied");
		}
	}
	if (verbose)
		cout << "  " << key << " = " << val << endl;
	return val;
}

vector<string> string_split(const string &s)
{
	std::stringstream ss(s);
	std::istream_iterator<std::string> begin(ss);
	std::istream_iterator<std::string> end;
	std::vector<std::string> vstrings(begin, end);
	// std::copy(vstrings.begin(), vstrings.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
	return vstrings;
}

vector<string> lookup_stringlist_parameter(
	const map<string,string> &params,
	const toml::basic_value<::toml::discard_comments, std::unordered_map, std::vector> &tomldata,
	const string &key, bool verbose)
{
	vector<string> val;
	auto val_it = params.find(key);
	if (val_it != params.end()) {
		val = string_split(val_it->second);
	} else {
		if (tomldata.contains(key)) {
			val = toml::find<vector<string>>(tomldata,key);
		} else {
			emsgroot("ERROR: Parameter \'"+key+"\' must be supplied");
		}
	}

	if (verbose) {
		cout << "  " << key << " = ";

		for (auto it = val.begin(); it != val.end(); it++) {
			cout << *it << " ";
		}
		cout << endl;
	}
	return val;
}

vector<string> get_toml_keys(
	const toml::basic_value<::toml::discard_comments, std::unordered_map, std::vector> &data)
{
	vector<string> keys;
	for(const auto& p : data.as_table())
	{
		keys.push_back(p.first);
	}
	return keys;
}

void check_for_undefined_parameters(vector<string> allowed, vector<string> given,
																		const string &context)
{
	vector<string> undefined;

	sort(allowed.begin(), allowed.end());
	sort(given.begin(), given.end());

	set_difference(given.begin(), given.end(),
								 allowed.begin(), allowed.end(),
								 inserter(undefined, undefined.begin()));

	if (undefined.size() != 0) {
		stringstream ss;
		ss << "Unrecognised parameter(s) "+context+":";

		for (const auto &k : undefined) {
			ss << " " << k;
		}
		
		emsgroot(ss.str());
	}
}

map<string,string> get_command_line_params(int argc, char *argv[])
{
	map<string,string> cmdlineparams;

	// Store the parameters passed on the command line in cmdlineparams
	for(int op = 1; op < argc; op++){ // Goes the various input options
		string str = string(argv[op]);
		int j = 0; int jmax = str.length(); while(j < jmax && str.substr(j,1) != "=") j++;
		if(j == jmax){
			stringstream ss; ss << "Cannot understand " << str; 
			emsgroot(ss.str());
		}
		
		string command = str.substr(0,j);
		string value = str.substr(j+1,jmax-(j+1));

		if (cmdlineparams.count(command) == 0) {
			cmdlineparams[command] = value;
		} else {
			// Encode repeated parameters as space-separatedd
			cmdlineparams[command] += " " + value;
		}
	}
	return cmdlineparams;
}

vector<string> keys_of_map(map<string,string> m)
{
	vector<string> keys;
	for (const auto &p : m) {
		keys.push_back(p.first);
	}
	return keys;
}


int main(int argc, char** argv)
{
	unsigned int ncore, core;                 // Stores the number of cores and the core of the current process
	unsigned int nsamp=UNSET;                 // The number of samples for inference
	unsigned int nchain=UNSET;                // The number of chains per core (MBP only)
	unsigned int npart=UNSET;                 // The number of particles per core (PMCMC only)
	
	unsigned int mode=UNSET;                  // Sets the mode of operation (sim/PMCMC/MBP)
	unsigned int period=UNSET;                // The period of time for simulation / inference
	
	int seed=0;                               // Sets the random seed for simulation
	
	unsigned int j, k;

	TRANSDATA transdata;
	string str, command, value;

	#ifdef USE_MPI
  MPI_Init(&argc, &argv);
	int num;
	MPI_Comm_size(MPI_COMM_WORLD,&num); ncore =num;
  MPI_Comm_rank(MPI_COMM_WORLD,&num); core =num;
  #endif

	#ifndef USE_MPI
	ncore = 1;
	core = 0;
	#endif

	if (core == 0)
		cout << "CoronaPMCMC version " << GIT_VERSION << endl;
	
	DATA data;    // The following file names will need to be read in by the interface:
	MODEL model(data);
		
	data.outputdir="Output";                // The default output directory
		
	// A list of all supported parameters
	vector<string>  definedparams {"covars", "infmax", "datadir", "trans", "betaspline", "phispline", "comps", "params", "priors", "democats", "ages", "regions", "areas", "Q", "timep", "mode", "model", "npart", "nchain", "nsamp", "period", "seed", "transdata", "outputdir", "inputfile"};

	// Read command line parameters
	map<string,string> cmdlineparams = get_command_line_params(argc, argv);
	check_for_undefined_parameters(definedparams, keys_of_map(cmdlineparams), "on command line");
	
	// Read TOML parameters
	string inputfilename = "/dev/null";
	if (cmdlineparams.count("inputfile") == 1) {
		inputfilename = cmdlineparams["inputfile"];
	}
	decltype(toml::parse(inputfilename)) tomldata;
	try {
		tomldata = toml::parse(inputfilename);
	} catch (const std::exception& e) {
		std::ostringstream oss;
		oss << "toml::parse returns exception\n" << e.what();
		emsg(oss.str());
#ifdef USE_MPI
		MPI_Finalize();
#endif
		exit(EXIT_FAILURE);
	}
		
	vector<string>  tomlkeys = get_toml_keys(tomldata);
	check_for_undefined_parameters(definedparams, tomlkeys, "in " + inputfilename);

	// The code could be simplified by reading the TOML parameters into a
	// map<string,string> and merging it with cmdlineparams. However, this would
	// require casting all the TOML values to strings, and we would lose the
	// vectors and types in the TOML file, and have to parse them out of
	// strings. An alternative would be to interpret the command line as a TOML
	// fragment, and work with just TOML values. That might be a better approach.

	/*********************************************************************************
	/ Process parameters
  **********************************************************************************/

	bool param_verbose = (core == 0);

	if (param_verbose)
		cout << endl << "Settings:" << endl;

	string key,val;

	// DETAILS
	
	// mode
	val = lookup_string_parameter(cmdlineparams, tomldata, "mode", param_verbose, "UNSET");
	map<string,int>  modemap{{"sim", MODE_SIM}, {"pmcmc", MODE_PMCMC}, {"inf", MODE_MBP}};
	if (modemap.count(val) != 0) {
		mode = modemap[val];
	} else {
		emsgroot("Unrecoginsed value " + val + " for mode parameter");
	}

	if(mode == MODE_PMCMC){
		// npart
		int nparttot = lookup_int_parameter(cmdlineparams, tomldata, "npart", param_verbose, npart);
		if (npart != UNSET) {
			if(nparttot%ncore != 0) emsgroot("The number of particles must be a multiple of the number of cores");
			npart = nparttot/ncore;
		}
	}

	if(mode != MODE_SIM){
		// nchain
		int nchaintot = lookup_int_parameter(cmdlineparams, tomldata, "nchain", param_verbose, UNSET);
		if (nchaintot != UNSET) {
			if(nchaintot%ncore != 0) emsgroot("The number of chains must be a multiple of the number of cores");
			nchain = nchaintot/ncore;
		}
	}
	
	if(mode != MODE_SIM){
		// nsamp
		nsamp = lookup_int_parameter(cmdlineparams, tomldata, "nsamp", param_verbose);
		
		// infmax
		if(tomldata.contains("infmax")) {
			model.infmax = lookup_int_parameter(cmdlineparams, tomldata, "infmax", param_verbose);
		}
		else emsgroot("Input file must contain a limit on the maximum number of individuals through 'infmax'.");
	}
	
	// period
	period = lookup_int_parameter(cmdlineparams, tomldata, "period", param_verbose);

	// seed
	if(mode == MODE_SIM){
		seed = lookup_int_parameter(cmdlineparams, tomldata, "seed", param_verbose);
	}
		
	// outputdir
	data.outputdir = lookup_string_parameter(cmdlineparams, tomldata, "outputdir", param_verbose, data.outputdir);

	// data directory
	data.datadir = lookup_string_parameter(cmdlineparams, tomldata, "datadir", param_verbose, "UNSET");

	// region data
	data.regiondatafile = lookup_string_parameter(cmdlineparams, tomldata, "regions", param_verbose, "UNSET");

	// area data
	data.areadatafile = lookup_string_parameter(cmdlineparams, tomldata, "areas", param_verbose, "UNSET");

	// End of parameters
	if (param_verbose)
		cout << endl;
	
	// THE MODEL
	
	// age categories
	if(tomldata.contains("ages")) {
		const auto ages = toml::find(tomldata,"ages");
		vector<string> pos, possus;
		for(j = 0; j < ages.size(); j++){
			const auto ag = toml::find(ages,j);
			
			if(!ag.contains("range")) emsgroot("A 'range' must be specified in 'ages'.");
			const auto range = toml::find<std::string>(ag,"range");
			pos.push_back(range);
			
			if(!ag.contains("sus")) emsgroot("A 'sus' must be specified in 'ages'.");
			const auto sus = toml::find<std::string>(ag,"sus");
			possus.push_back(sus);
		}
		data.adddemocat("Age",pos,possus);
	}
	else emsgroot("The 'ages' parameter must be set.");
	
	if(core == 0){	
		cout << "Age categories: " << endl;
		for(j = 0; j < data.democat[0].value.size(); j++){
			if(j != 0) cout << ", ";
			cout << data.democat[0].value[j] << " sus='" <<  data.democat[0].param[j] << "'";
		}
		cout << endl << endl;
	}
	
	// democats
	if(tomldata.contains("democats")) {
		const auto democats = toml::find(tomldata,"democats");
	
		for(k = 0; k < democats.size(); k++){
			vector<string> pos, possus;
			
			const auto democat = toml::find(democats,k);
			
			for(j = 0; j < democat.size(); j++){
				const auto demoval = toml::find(democat,j);
				
				if(!demoval.contains("value")) emsgroot("A 'value' must be specified in 'democats'.");
				const auto value = toml::find<std::string>(demoval,"value");
				pos.push_back(value);
				
				if(!demoval.contains("sus")) emsgroot("The property 'sus' must be specified in 'democats'.");
				const auto sus = toml::find<std::string>(demoval,"sus");
				vector<string> pos;
				possus.push_back(sus);
			}
			
			data.adddemocat("",pos,possus);
		}
	}
	
	if(core == 0 && data.democat.size() > 1){
		cout << "Demographic categories: " << endl;
		for(k = 1; k < data.democat.size(); k++){
			for(j = 0; j < data.democat[k].value.size(); j++){
				if(j != 0) cout << ", ";
				cout << data.democat[k].value[j] << " sus='" <<  data.democat[k].param[j] << "'";
			}	
			cout << endl;
		}
		cout << endl;
	}
	
	// area covariates
	if(tomldata.contains("covars")) {
		const auto covars = toml::find(tomldata,"covars");
		for(j = 0; j < covars.size(); j++){
			const auto covar = toml::find(covars,j);
			
			if(!covar.contains("name")) emsgroot("A 'name' must be specified in 'covars'.");
			const auto name = toml::find<std::string>(covar,"name");
	
			if(!covar.contains("param")) emsgroot("A 'param' must be specified in 'covars'.");
			const auto par = toml::find<std::string>(covar,"param");

			if(!covar.contains("func")) emsgroot("A 'func' must be specified in 'covars'.");
			const auto func = toml::find<std::string>(covar,"func");

			data.addcovar(name,par,func);
		}
		
		if(core == 0){
			cout << "Area covariates: " << endl;
			for(j = 0; j < data.covar.size(); j++) cout << data.covar[j].name << "   param='" << data.covar[j].param << "'" << endl; 
		}
		cout << endl;
	}

	// timep
	if(tomldata.contains("timep")) {
		const auto timep = toml::find(tomldata,"timep");
		for(j = 0; j < timep.size(); j++){
			const auto tim = toml::find(timep,j);
			
			if(!tim.contains("name")) emsgroot("A 'name' must be specified in 'timep'.");
			const auto name = toml::find<std::string>(tim,"name");
			
			if(!tim.contains("tend")) emsgroot("'tend' must be specified in 'timep'.");
			const auto tend = toml::find<double>(tim,"tend");
			
			data.addtimep(name,tend);
		}
	}
	else emsgroot("Property 'timep' defining time periods must be set.");

	if(core == 0){
		cout << "Time periods defined:" << endl;
		for(j = 0; j < data.timeperiod.size(); j++){
			cout << data.timeperiod[j].name << ": ";
			if(j == 0) cout << "0"; else cout << data.timeperiod[j-1].tend;
			cout << " - " <<  data.timeperiod[j].tend << endl;
		}
		cout << endl;
	}

	// Q
	if(tomldata.contains("Q")) {
		const auto Qlist = toml::find(tomldata,"Q");
		for(j = 0; j < Qlist.size(); j++){
			const auto Q = toml::find(Qlist,j);
			
			if(!Q.contains("timep")) emsgroot("A 'timep' must be specified in 'Q'.");
			const auto timep = toml::find<std::string>(Q,"timep");
			
			if(!Q.contains("comp")) emsgroot("'comp' must be specified in 'Q'.");
			const auto comp = toml::find<std::string>(Q,"comp");
			
			if(!Q.contains("file")) emsgroot("'file' must be specified in 'Q'.");
			const auto file = toml::find<std::string>(Q,"file");
			
			data.addQtensor(timep,comp,file);
		}
	}
	else emsgroot("Property 'timep' defining time periods must be set.");
	
	if(core == 0){
		cout << "Q tensors loaded:" << endl;
		for(j = 0; j < data.Q.size(); j++){
			cout << "timep: " << data.timeperiod[data.Q[j].timep].name << "  ";
			cout << "compartment: " << data.Q[j].comp << "  ";
			cout << "file: " << data.Q[j].file << "  ";
			cout << endl;
		}
		cout << endl;
	}
	
	// THE DATA
	

	// transdata
	
	if(tomldata.contains("transdata")) {
		const auto tdata = toml::find(tomldata,"transdata");

		for(j = 0; j < tdata.size(); j++){
			const auto td = toml::find(tdata,j);
			
			if(!td.contains("from")) emsgroot("A 'from' property must be specified in 'transdata'.");
			const auto from = toml::find<std::string>(td,"from");
			transdata.from = from;
		
			if(!td.contains("to")) emsgroot("A 'to' property must be specified in 'transdata'.");
			const auto to = toml::find<std::string>(td,"to");
			transdata.to = to;
			
			if(!td.contains("area")) emsgroot("An 'area' property must be specified in 'transdata'.");
			const auto area = toml::find<std::string>(td,"area");
			transdata.type = area;
			if(transdata.type != "reg" && transdata.type != "all") emsgroot("Transition data type not recognised"); 
			
			if(!td.contains("file")) emsgroot("A 'file' property must be specified in 'transdata'.");
			const auto file = toml::find<std::string>(td,"file");
			transdata.file = file;
			
			if(!td.contains("start")) emsgroot("A 'start' property must be specified in 'transdata'.");
			const auto start = toml::find<unsigned int>(td,"start");
			transdata.start = start;
			
			if(!td.contains("units")) emsgroot("A 'units' property must be specified in 'transdata'.");
			const auto units = toml::find<std::string>(td,"units");
			if(units == "days") transdata.units = 1;
			else{
				if(units == "weeks") transdata.units = 7;
				else emsgroot("Units in 'transdata' not recognised");
			}
			
			if(mode == MODE_SIM){
				transdata.rows = (unsigned int)((period - start)/transdata.units);
				if(transdata.rows == 0) emsgroot("Transition data '"+file+"' cannot be generated because the time period is not sufficiently long.");
			}
			
			data.transdata.push_back(transdata);
		}
	}
	else emsgroot("The 'transdata' parameter must be set.");

	if(mode == MODE_SIM && ncore != 1) emsgroot("Simulation only requires one core");
	
	if(mode == UNSET) emsgroot("The property 'mode' must be set"); 
 
	MPI_Barrier(MPI_COMM_WORLD);

	if(period == UNSET) emsgroot("The property 'period' must be set");
	
	data.readdata(core,ncore,mode,period);

	POPTREE poptree;
	poptree.init(data,core);	

	model.definemodel(core,data.period,data.popsize,tomldata);
	model.addQ();
	model.checktransdata();

	if(core == 0) cout << "Running...." << endl;

	timersinit();
	timers.timetot = -clock();
	
	MPI_Barrier(MPI_COMM_WORLD);

	switch(mode){
	case MODE_SIM:
		srand(seed); 
		simulatedata(data,model,poptree);
		break;
		
	case MODE_PMCMC:
		if(nsamp == UNSET) emsgroot("The number of samples must be set");
		if(npart == UNSET) emsgroot("The number of particles must be set");

		PMCMC(data,model,poptree,nsamp,core,ncore,npart);
		break;

	case MODE_MBP: 
		if(nsamp == UNSET) emsgroot("The number of samples must be set");
		if(nchain == UNSET) emsgroot("The number of chains must be set");
		MBP(data,model,poptree,nsamp,core,ncore,nchain);
		break;

	default: emsgroot("Mode not recognised"); break;
 	}
	
	timers.timetot += clock();
	
	if(core == 0){
		cout << double(timers.timetot)/CLOCKS_PER_SEC << " Total time (seconds)" << endl;
		if(mode != MODE_MBP) cout << double(timers.timesim)/CLOCKS_PER_SEC << " Simulation time (seconds)" << endl;
		if(mode == MODE_PMCMC) cout << double(timers.timeboot)/CLOCKS_PER_SEC << " Bootstrap time (seconds)" << endl;
		if(mode == MODE_MBP) cout << double(timers.timewait)/CLOCKS_PER_SEC << " MBP waiting time (seconds)" << endl;
		if(mode == MODE_MBP) cout << double(timers.timembp)/CLOCKS_PER_SEC << " MBP time (seconds)" << endl;
		if(mode == MODE_MBP) cout << double(timers.timembpinit)/CLOCKS_PER_SEC << " MBP init (seconds)" << endl;
		if(mode == MODE_MBP) cout << double(timers.timembpQmap)/CLOCKS_PER_SEC << " MBP Qmap (seconds)" << endl;
		if(mode == MODE_MBP) cout << double(timers.timembpprop)/CLOCKS_PER_SEC << " MBP prop (seconds)" << endl;
	}
	
	#ifdef USE_MPI
	MPI_Finalize();
	#endif
}
