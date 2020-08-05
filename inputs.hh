#ifndef BEEPMBP__INPUTS_HH
#define BEEPMBP__INPUTS_HH

#include <string>
#include <map>

using namespace std;

#include "consts.hh"
#include "toml11/toml.hpp"

class Inputs
{
public:
	Inputs(int argc, char** argv, bool verbose);

	int find_int(const string &key, int def) const;
	double find_double(const string &key, double def) const;
	string find_string(const string &key, const string &def) const;	

	Mode mode() const;
	
private:
	void set_command_line_params(int argc, char *argv[]);  
	void read_toml_file(bool verbose); 
	
	vector<string> get_toml_keys( const toml::basic_value<::toml::discard_comments, std::unordered_map, std::vector> &data) const;
	void check_for_undefined_parameters(vector<string> allowed, vector<string> given,	const string &context) const;
	
	map<string,string> cmdlineparams;                                                   // A map of all the parameters entered on the command line
	toml::basic_value<toml::discard_comments, std::unordered_map, std::vector> tomldata;// Stores information from the TOML file

	vector<string> definedparams;                                                       // Stores all the possible parameters 
};

#endif
