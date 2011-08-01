/* Test-case helper functions */

#pragma once

#include <mongoDeploy/mongoDeploy.h>
#include <10cluster/cluster.h>
#include <boost/regex.hpp>

namespace mongoTest {

const module::Module module ("mongoTest", "mongoTest/mongoTest.h");

/** Specification for a replica set of given number of active servers */
std::vector<mongoDeploy::RsMemberSpec> rsSpecWithArbiter (unsigned numActiveServers);

/** Launch sharded Mongo deployment on cluster of servers with numReplicas per shard (+ one arbiter) and numShards */
mongoDeploy::ShardSet deployCluster (unsigned numReplicas, unsigned numShards);

inline bool regex_match (std::string regex, std::string string) {
	boost::regex re (regex);
	return boost::regex_match (string, re);}

/** Watch logfile (as it grows) forever. Abort (return) when a line passes test, returning file position of that line. */
std::streampos waitLogFile (boost::function1<bool,std::string> lineTest, std::string filename);

class LogAlert : public std::exception {
public:
	std::string filename;
	std::string logSection;
	LogAlert (std::string filename, std::string logSection)
		: filename (filename), logSection (logSection) {}
	~LogAlert () throw () {}
	const char* what() const throw () {  // overriden
		return ("Log alert " + filename + ":\n" + logSection) .c_str();
	}
};

/** Watch logfile (as it grows) forever. Throw LogAlert when a line passes test, with N chars before and M chars after included in error message. */
void watchLogFile (boost::function1<bool,std::string> lineTest, unsigned charsBefore, unsigned charsAfter, std::string filename);

/** Watch process log (as it grows) forever. Throw LogAlert when a log line passes test, with N chars before and M chars after included in error message. */
inline void watchLog (boost::function1<bool,std::string> lineTest, unsigned charsBefore, unsigned charsAfter, process::Process proc) {
	watchLogFile (lineTest, charsBefore, charsAfter, proc->outFilename());}
inline void watchLog_ (remote::Function1<bool,std::string> lineTest, unsigned charsBefore, unsigned charsAfter, process::Process proc) {
	watchLog (*lineTest, charsBefore, charsAfter, proc);}
inline void watchLogR (remote::Function1<bool,std::string> lineTest, unsigned charsBefore, unsigned charsAfter, remote::Process proc) {
	remote::apply (remote::bind (MFUN(mongoTest,watchLog_), lineTest, charsBefore, charsAfter), proc);}

}
