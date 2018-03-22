#ifndef _LOG_H
#define _LOG_H

//#define _NO_PRINT_LOG

#include <string>
#include <string.h>
#include <fstream>
#include <sys/time.h>

const int ERROR_LEVEL = 1;
const int WARN_LEVEL = 2;
const int INFO_LEVEL = 3;
const int DEBUG_LEVEL = 4;

extern int log_level;  //read from log config

int  log_init(std::string dir, std::string file);
void log_error(const char* format, ...);
void log_warn(const char* format, ...);
void log_info(const char* format, ...);
void log_debug(const char* format, ...);
void set_log_level(const char* level);

class log_manager {
public:
	log_manager();
	~log_manager();
private:
	log_manager(const log_manager& rhs);
	log_manager& operator=(const log_manager& rhs);
public:
	bool is_inited();
	void set_retain_day(int rt_day);
	int  init(std::string& dir, std::string& file);
	int  write_log(char* log, const char* format, va_list ap);
	int  shift_file_if_need(struct timeval tv, struct timezone tz);
	int  delete_old_log(timeval tv);
private:
	std::fstream    fs_;
	std::string     log_file_;
	std::string     log_dir_;
	std::string     log_file_path_;
	long            last_sec_;
	bool            is_inited_;
	int             retain_day_;
	pthread_mutex_t writelock_;
};

#ifndef _NO_PRINT_LOG
#define LOG_ERROR(format, args...) if(log_level >= ERROR_LEVEL) \
		log_error("%s %s(%d): " format, "ERROR", __FILE__, __LINE__, ##args); 
#define LOG_WARN(format, args...) if(log_level >= WARN_LEVEL) \
		log_warn("%s %s(%d): " format, "WARN", __FILE__, __LINE__, ##args); 
#define LOG_INFO(format, args...) if(log_level >= INFO_LEVEL) \
		log_info("%s %s(%d): " format, "INFO", __FILE__, __LINE__, ##args); 
#define LOG_DEBUG(format, args...) if(log_level >= DEBUG_LEVEL)  \
		log_debug("%s %s(%d): " format, "DEBUG", __FILE__, __LINE__, ##args); 
#else
#define LOG_ERROR(format, args...) { }
#define LOG_WARN(format, args...) { }
#define LOG_INFO(format, args...) { }
#define LOG_DEBUG(format, args...) { }
#endif 

#endif
