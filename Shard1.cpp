/* Two replica-set shards. Each replica set has 2 durable servers and 1 arbiter. Insert and update while the durable servers are killed and restarted. */

#include "Shard1.h"
#include "mongoTest.h"
#include <iostream>
#include <fstream>
#include <boost/function.hpp>
#include <boost/regex.hpp>
#include <10cluster/cluster.h>
#include <10util/thread.h>
#include <10util/vector.h>
#include <10util/except.h>

using namespace std;

module::Module _Shard1::module (
	items<string>("mongoTest", "mongoDeploy", "mongoclient", "10cluster", "10remote", "10util", "boost_thread-mt", "boost_serialization-mt", "boost_system-mt"),
	"mongoTest/Shard1.h");

class BadResult : public std::exception {
public:
	std::string message;
	BadResult (std::string message) : message(message) {}
	~BadResult () throw () {}
	const char* what() const throw () {  // override
		return message.c_str();
	}
};

template <class A> void checkEqual (A x, A y) {
	if (x != y) {
		stringstream ss;
		ss << "Mismatch: " << x << " != " << y;
		throw BadResult (ss.str());
	}
}

/** Sample 1KB text */
static string largeText () {
	static string LargeText;
	if (LargeText.size() == 0) {
		stringstream ss;
		for (unsigned i = 0; i < 1000; i++) ss << "a";
		LargeText = ss.str();
	}
	return LargeText;
}

static unsigned long long numDocs = 100000;
static unsigned long long batchSize = 1000;

/** Generate many documents */
static vector<mongo::BSONObj> docs (unsigned round, unsigned count) {
	vector<mongo::BSONObj> docs;
	for (unsigned i = 0; i < count; i++)
		docs.push_back (BSON ("_id" << (count * round) + i << "x" << i << "text" << largeText()));
	return docs;
}

/** Issue getLastError check */
static void confirmWrite (mongo::DBClientConnection& c) {
	mongo::BSONObj info;
	mongo::BSONObj cmd = BSON ("getlasterror" << 1 << "fsync" << true << "w" << 1);
	cout << cmd << " -> " << endl;
    c.runCommand ("test", cmd, info);
	cout << info << endl;
}

/** All replica-set shard processes excluding arbiters */
static vector<remote::Process> activeShardProcesses (mongoDeploy::ShardSet s) {
	vector<remote::Process> procs;
	for (unsigned i = 0; i < s.shards.size(); i ++) {
		vector<remote::Process> rsProcs = s.shards[i].activeReplicas();
		for (unsigned j = 0; j < rsProcs.size(); j ++) procs.push_back (rsProcs[j]);
	}
	return procs;
}

/** Insert batches of documents in a loop */
void _Shard1::insertData (mongoDeploy::ShardSet s) {
	using namespace mongo;
	BSONObj info;

	std::string h = mongoDeploy::hostPortString (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	for (unsigned i = 0; i < numDocs/batchSize; i++) {
		cout << "insert round " << i << endl;
		try {
			c.insert ("test.col", docs (i, batchSize));
			confirmWrite (c);
		} catch (...) {
			cout << "Insert connection failed, retry next round" << endl;
		}
		::thread::sleep (3);
	}

	try {
		BSONObj cmd = BSON ("dbstats" << 1);
		cout << cmd << " -> " << endl;
		c.runCommand ("test", cmd, info);
		cout << info << endl;
	} catch (...) {}
}

/** Multi-update docs in a loop */
void _Shard1::updateData (mongoDeploy::ShardSet s, unsigned z) {
	using namespace mongo;

	string h = mongoDeploy::hostPortString (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	for (unsigned i = 0; i < 50; i++) {
		unsigned long long n;
		cout << "update " << z << " round " << i << endl;
		::thread::sleep (z + 10);
		try {
			c.update ("test.col", BSON ("x" << i), BSON ("$push" << BSON ("z" << z)), false, true);
			confirmWrite (c);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			cout << "First query after kill failed as expected" << endl;
			continue;
		}
		//mongoTest::checkEqual (n, numDocs/batchSize);
		::thread::sleep (z);
		try {
			c.update ("test.col", BSON ("x" << i), BSON ("$pull" << BSON ("z" << z)), false, true);
			confirmWrite (c);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			cout << "First query after kill failed as expected" << endl;
			continue;
		}
		//mongoTest::checkEqual (n, (unsigned long long) 0);
	}
}

/** Kill random server every once in a while and restart it */
void _Shard1::killer (mongoDeploy::ShardSet s) {
	vector<remote::Process> procs = activeShardProcesses (s);
	while (true) {
		thread::sleep (10 + (rand() % 60));
		unsigned r = rand() % procs.size();
		remote::Process p = procs[r];
		remote::signal (SIGKILL, p);
		cout << "Killed " << p << endl;
		thread::sleep (rand() % 30);
		remote::restart (p);
		cout << "Restarted  " << p << endl;
	}
}

/** Task that will watch log of process, and raise error on any ASSERT */
static boost::function0<void> logWatcher (remote::Process proc) {
	return boost::bind (mongoTest::watchLogR, remote::bind (MFUN(mongoTest,regex_match), string("ASSERT")), 100, 500, proc);
}

/** Task that will watch logs of mongod and mongos's, and raise error on any ASSERT */
static vector< boost::function0<void> > logWatchers (mongoDeploy::ShardSet s) {
	vector< boost::function0<void> > tasks;
	push_all (tasks, fmap (logWatcher, s.configSet.cfgServers));
	push_all (tasks, fmap (logWatcher, s.routers));
	for (unsigned i = 0; i < s.shards.size(); i++)
		push_all (tasks, fmap (logWatcher, s.shards[i].replicas));
	return tasks;
}

/** Deploy Mongo shard set on servers in cluster, then launch inserter, updaters, killer and watchers on clients in cluster */
void Shard1::run () {
	mongoDeploy::ShardSet s = mongoTest::deployCluster (2, 2);
	mongoDeploy::shardDatabase (s.routers[0], "test");
	mongoDeploy::shardCollection (s.routers[0], "test.col", BSON ("_id" << 1));

	// One insert actor and one update actor running on arbitrary clients in cluster
	vector< boost::function0<void> > fore;
	fore.push_back (boost::bind (remote::eval<void>, remote::bind (MFUN(_Shard1,insertData), s), cluster::someClient()));
	fore.push_back (boost::bind (remote::eval<void>, remote::bind (MFUN(_Shard1,updateData), s, (unsigned)1), cluster::someClient()));

	// One thread watching each mongod/s log, plus one thread killing random servers
	vector< boost::function0<void> > aft;
	push_all (aft, logWatchers (s));
	aft.push_back (boost::bind (_Shard1::killer, s));

	// Launch all fore and aft threads, if any fail then stop them all and raise failure here
	thread::parallel (fore, aft);
}
