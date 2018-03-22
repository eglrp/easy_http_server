#include "log_config.h"

#include <fstream>
#include <sstream>

//get config file from log.conf 
int get_config_map(const char* config_file, std::map<std::string, std::string>& configs)
{
	std::ifstream fs(config_file);
	if(!fs.is_open())  //check if open
		return -1;
	
	while(fs.good()){  //good means none of the stream's error state flags (eofbit, failbit and badbit) is set
		std::string line;
		std::getline(fs, line);

		if(line[0] == '#') //it's note
			continue; 

		std::stringstream ss;
		ss<<line;
		std::string key, value;
		std::getline(ss, key, '=');  
		std::getline(ss, value, '=');

		configs[key] = value;  //insert the config info to map
	}
	fs.close();
	return 0;
}
