#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdarg.h>
#include <fstream>
#include <sstream>
#include <map>
#include <string>

#include "log_config.h"
#include "log.h"
#include "mutex.h"
#include "condition.h"

const int MAX_SINGLE_LOG_SIZE = 2048;
const int ONE_DAY_SECONDS = 865400;  // 24 * 3600

int log_level = DEBUG_LEVEL;  //default log_level
std::string g_dir;
std::string g_config_file;
bool use_log_manager = false;
log_manager g_log_manager;

log_manager::log_manager() 
	: last_sec_(0), is_inited_(false), retain_day_(-1)
{
}

log_manager::~log_manager()
{
	if(fs_.is_open())
		fs_.close();
	if(is_inited_)
		pthread_mutex_destroy(&writelock_);
}

int log_manager::init(std::string& dir, std::string& log_file)
{
	//try to open the dir, and if failed, use the current "." dir
	if(!dir.empty()) {
		int ret = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); //see baidu baike "mkdir"
		if(ret != 0 && errno != EEXIST){
			printf("mkdir error which dir:%s err:%s\n", dir.c_str(), strerror(errno));
			is_inited_ = true;
			return -1;
		}
	}
	else {
		dir = "."; 
	}

	log_dir_ = dir;
	log_file_ = log_file;
	log_file_path_ = dir + "/" + log_file;
	fs_.open(log_file_path_.c_str(), std::fstream::out | std::fstream::app);  //append
	is_inited_ = true;
	pthread_mutex_init(&writelock_, NULL);
	return 0;
}

inline bool log_manager::is_inited()
{
	return is_inited_;
}

inline void log_manager::set_retain_day(int rt_day)
{
	retain_day_ = rt_day;
}

int log_manager::write_log(char* log, const char* format, va_list ap)
{
	pthread_mutex_lock(&writelock_);
	if(fs_.is_open()) {
		vsnprintf(log, MAX_SINGLE_LOG_SIZE-1, format, ap);
		fs_<<log<<"\n";
		fs_.flush();
	}
	pthread_mutex_unlock(&writelock_);
	return 0;
}

int log_manager::shift_file_if_need(struct timeval tv, struct timezone tz)
{
	if(last_sec_ == 0) {
		last_sec_ = tv.tv_sec;
		return 0;
	}

	long fix_now_sec = tv.tv_sec - tz.tz_minuteswest * 60;  //GMT time diff
	long fix_last_sec = last_sec_ - tz.tz_minuteswest * 60;
	if(fix_now_sec / ONE_DAY_SECONDS - fix_last_sec / ONE_DAY_SECONDS) {  //save the interger part
		pthread_mutex_lock(&writelock_);
		
		struct tm* tm;
		time_t y_sec = tv.tv_sec - ONE_DAY_SECONDS;   
		tm = localtime(&y_sec);   //yesterday time
		char new_file[100];
		memset(new_file, 0, sizeof(new_file));
		sprintf(new_file, "%s.%04d-%02d-%02d", log_file_.c_str(), tm->tm_year+1900,
											   tm->tm_mon+1, tm->tm_mday);
		std::string new_file_path = log_dir_ + "/" + new_file;
		if(access(new_file_path.c_str(), F_OK) != 0) {
			rename(log_file_path_.c_str(), new_file_path.c_str());  //rename yesterday's log name to "new_file_path" 
			fs_.close();
			//reopen new log file, still use "log_file_path", because the previous log_file_path has been rename.
			fs_.open(log_file_path_.c_str(), std::fstream::out | std::fstream::app);  
		}

		pthread_mutex_unlock(&writelock_);
		
		delete_old_log(tv);
	}

	last_sec_ = tv.tv_sec;
	return 0;
}

int log_manager::delete_old_log(timeval tv)
{
	if(retain_day_ <= 0) 
		return 0;

	struct timeval old_tv;
	old_tv.tv_sec = tv.tv_sec - retain_day_ * 3600 * 24;
	old_tv.tv_usec = tv.tv_usec;
	char old_file[100];
	memset(old_file, 0, 100);
	struct tm* tm;
	tm = localtime(&old_tv.tv_sec);
	sprintf(old_file, "%s.%04d-%02d-%02d", log_file_.c_str(), tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday-1);
	std::string old_file_path = log_dir_ + "/" + old_file;
	return remove(old_file_path.c_str());
}

int _get_log_level(const char* level_str)
{
	if(strcasecmp(level_str, "ERROR") == 0)
		return ERROR_LEVEL;
	else if(strcasecmp(level_str, "WARN") == 0)
		return WARN_LEVEL;
	else if(strcasecmp(level_str, "INFO") == 0)
		return INFO_LEVEL;
	else if(strcasecmp(level_str, "DEBUG") == 0)
		return DEBUG_LEVEL;
	return DEBUG_LEVEL;  //
}

inline void set_log_level(const char* level)
{
	log_level = _get_log_level(level);
}

//check need start log module from config file
int _check_config_file() 
{
	std::map<std::string, std::string> configs;
	std::string log_config_file = g_dir + "/" + g_config_file;
	
	get_config_map(log_config_file.c_str(), configs);
	if(configs.empty())
		return 0;
	
	//read log level
	std::string log_level_str = configs["log_level"];
	set_log_level(log_level_str.c_str());

	std::string rt_day = configs["retai_day"];
	if(!rt_day.empty())
		g_log_manager.set_retain_day(atoi(rt_day.c_str()));
	
	//read log file
	std::string dir = configs["log_dir"];
	std::string log_file = configs["log_file"];

	int ret = 0;
	if(!log_file.empty()) {  //if log_file not empty, start the file appender
		use_log_manager = true;
		if(!g_log_manager.is_inited()) {
			ret = g_log_manager.init(dir, log_file);
		}
	}
	return ret;
}

//log init function, invoke by main function
int log_init(std::string dir, std::string file)
{
	g_dir = dir;
	g_config_file = file;
	return _check_config_file();
}

std::string _get_show_time(timeval tv)
{
	char show_time[40];
	memset(show_time, 0, sizeof(show_time));

	struct tm* tm;
	tm = localtime(&tv.tv_sec);

	sprintf(show_time, "%04d-%02d-%02d %02d:%02d:%02d.%03d", 
			tm->tm_year + 1900, tm->tm_mon+1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000));
	return std::string(show_time);
}

void _log(const char* format, va_list ap)
{
	if(!use_log_manager) {   //if no config, send log to stdout
		vprintf(format, ap);
		printf("\n");
		return ;
	}

	struct timeval now;
	struct timezone tz;
	gettimeofday(&now, &tz);
	std::string final_format = _get_show_time(now) + " " + format;

	//g_log_manager.shift_file_if_need(now, tz);
	char single_log[MAX_SINGLE_LOG_SIZE];
	memset(single_log, 0, MAX_SINGLE_LOG_SIZE);
	g_log_manager.write_log(single_log, final_format.c_str(), ap);
}

void log_error(const char* format, ...)
{
	if(log_level < ERROR_LEVEL)
		return ;
	
	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}

void log_warn(const char* format, ...)
{
	if(log_level < WARN_LEVEL)
		return ;
	
	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}

void log_info(const char* format, ...)
{
	if(log_level < INFO_LEVEL)
		return ;
	
	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}

void log_debug(const char* format, ...)
{
	if(log_level < DEBUG_LEVEL)
		return ;

	va_list ap;
	va_start(ap, format);

	_log(format, ap);

	va_end(ap);
}
