
#include "mongoTest.h"
#include <fstream>
#include <10util/thread.h>

using namespace std;

/** Specification for a replica set of given number of active servers */
vector<mongoDeploy::RsMemberSpec> mongoTest::rsSpecWithArbiter (unsigned numActiveServers) {
	vector<mongoDeploy::RsMemberSpec> specs;
	for (unsigned i = 0; i < numActiveServers; i++)
		specs.push_back (mongoDeploy::RsMemberSpec (program::options ("journal", "", "noprealloc", "", "oplogSize", "200"), mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (program::options ("journal", "", "noprealloc", "", "oplogSize", "4"), BSON ("arbiterOnly" << true)));
	return specs;
}

/** Launch sharded Mongo deployment on cluster of servers with numReplicas per shard (+ one arbiter) and numShards */
mongoDeploy::ShardSet mongoTest::deployCluster (unsigned numReplicas, unsigned numShards) {
	// Launch empty shard set with one config server and one mongos with small chunk size
	mongoDeploy::ShardSet s = mongoDeploy::startShardSet (cluster::someServers(1), cluster::someServers(1), program::Options(), program::options ("chunkSize", "2"));
	// Launch N shards, each a replica set of numReplicas active servers and one arbiter
	for (unsigned i = 0; i < numShards; i++)
		s.addStartShard (cluster::someServers (numReplicas + 1), rsSpecWithArbiter (numReplicas));
	return s;
}

/** Watch logfile (as it grows) forever. Abort (return) when a line passes test, returning file position of that line. */
streampos mongoTest::waitLogFile (boost::function1<bool,string> lineTest, string filename) {
	ifstream file (filename.c_str());
	string line;
	while (true) {
		while (file.eof()) {thread::sleep (1.0); file.clear();}
		getline (file, line);
		if (lineTest (line)) return file.tellg() - (streampos)line.size();
	}
}

/** Watch logfile (as it grows) forever. Throw LogAlert when a line passes test, with N chars before and M chars after included in error message. */
void mongoTest::watchLogFile (boost::function1<bool,string> lineTest, unsigned charsBefore, unsigned charsAfter, string filename) {
	streampos pos = waitLogFile (lineTest, filename);
	ifstream file (filename.c_str());
	file.seekg (pos - (streampos)charsBefore);
	unsigned len = charsBefore + charsAfter;
	char* buf = new char[len];
	file.read (buf, len);
	string logSection = string (buf, len);
	delete[] buf;
	throw LogAlert (filename, logSection);
}
