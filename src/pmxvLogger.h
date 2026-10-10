#ifndef PMXV_LOGGER_H
#define PMXV_LOGGER_H

#include <fstream>
#include <string>

class pmxvLogger
{
public:
	static pmxvLogger* get();
	
	~pmxvLogger();
	
	void e(const std::string &error);
	void d(const std::string &debug);
	void m(const std::string &message);
	
protected:
	pmxvLogger();
	pmxvLogger(const std::string &);
	
	void startLog(const std::string &);

	std::ofstream* logfile;

};

// NOTE: these used to be named err/dbg/log, which clashes with log() from
// <cmath> on modern toolchains.
#define PMXV_ERR pmxvLogger::get()->e
#define PMXV_DBG pmxvLogger::get()->d
#define PMXV_LOG pmxvLogger::get()->m

#endif // PMXV_LOGGER_H

