#ifndef _LOG_CONFIG_H
#define _LOG_CONFIG_H

#include <map>
#include <string>

int get_config_map(const char* config_file, std::map<std::string, std::string>& configs);

#endif
