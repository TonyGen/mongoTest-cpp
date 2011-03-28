/* Two replica-set shards. Each replica set has 2 durable servers and 1 arbiter. Insert and update while the durable servers are killed and restarted. */

#include <iostream>
#include <mongoDeploy/mongoDeploy.h>
#include <cluster/cluster.h>
#include "mongoTest.h"
#include <job/thread.h>
#include <remote/thread.h>

using namespace std;

/** Specification for a replica set of given number of active servers */
static vector<mongoDeploy::RsMemberSpec> rsSpecWithArbiter (unsigned numActiveServers) {
	std::vector<mongoDeploy::RsMemberSpec> specs;
	for (unsigned i = 0; i < numActiveServers; i++)
		specs.push_back (mongoDeploy::RsMemberSpec (program::options ("dur", "", "noprealloc", "", "oplogSize", "200"), mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (program::options ("dur", "", "noprealloc", "", "oplogSize", "4"), BSON ("arbiterOnly" << true)));
	return specs;
}

/** Launch sharded Mongo deployment on cluster of servers */
static mongoDeploy::ShardSet deploy () {
	using namespace mongo;
	BSONObj info;

	// Launch empty shard set with one config server and one mongos with small chunk size
	mongoDeploy::ShardSet s = mongoDeploy::startShardSet (cluster::someServers(1), cluster::someClients(1), program::Options(), program::options ("chunkSize", "2"));

	// Launch two shards, each a replica set of 2 severs and one arbiter
	s.addStartShard (cluster::someServers(3), rsSpecWithArbiter(2));
	s.addStartShard (cluster::someServers(3), rsSpecWithArbiter(2));

	//connect to mongos
	string h = mongoDeploy::hostAndPort (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	// enable sharding on "test" db
	BSONObj cmd = BSON ("enablesharding" << "test");
	cout << cmd << " -> " << endl;
	c.runCommand ("admin", cmd, info);
	cout << info << endl;

	// shard "test.col" collection on "_id"
	cmd = BSON ("shardcollection" << "test.col" << "key" << BSON ("_id" << 1));
	cout << cmd << " -> " << endl;
	c.runCommand ("admin", cmd, info);
	cout << info << endl;

	return s;
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
	mongo::BSONObj cmd = BSON ("getlasterror" << 1 << "fsync" << true << "w" << 2);
	cout << cmd << " -> " << endl;
    c.runCommand ("test", cmd, info);
	cout << info << endl;
}

/** Use namespace for RPC Procedures to avoid name clashes */
namespace _Shard1 {

/** Insert batches of documents in a loop */
Unit insertData (mongoDeploy::ShardSet s) {
	using namespace mongo;
	BSONObj info;

	std::string h = mongoDeploy::hostAndPort (s.routers[0]);
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
		::thread::sleep (2);
	}

	try {
		BSONObj cmd = BSON ("dbstats" << 1);
		cout << cmd << " -> " << endl;
		c.runCommand ("test", cmd, info);
		cout << info << endl;
	} catch (...) {}
	return unit;
}

/** Multi-update docs in a loop */
Unit updateData (mongoDeploy::ShardSet s, unsigned z) {
	using namespace mongo;

	string h = mongoDeploy::hostPort (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	for (unsigned i = 0; i < 50; i++) {
		unsigned long long n;
		cout << "update " << z << " round " << i << endl;
		::thread::sleep (z + 15);
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
	return unit;
}

/** All replica-set shard processes excluding arbiters */
static vector<rprocess::Process> activeShardProcesses (mongoDeploy::ShardSet s) {
	vector<rprocess::Process> procs;
	for (unsigned i = 0; i < s.shards.size(); i ++) {
		vector<rprocess::Process> rsProcs = s.shards[i].activeReplicas();
		for (unsigned j = 0; j < rsProcs.size(); j ++) procs.push_back (rsProcs[j]);
	}
	return procs;
}

/** Kill random server every once in a while and restart it */
Unit killer (mongoDeploy::ShardSet s) {
	vector<rprocess::Process> procs = activeShardProcesses (s);
	thread::sleep (rand() % 10);
	while (true) {
		unsigned r = rand() % procs.size();
		rprocess::Process p = procs[r];
		rprocess::signal (SIGKILL, p);
		cout << "Killed " << p << endl;
		thread::sleep (rand() % 30);
		rprocess::restart (p);
		cout << "Restarted  " << p << endl;
		thread::sleep (rand() % 60);
	}
	return unit;
}
}

/** Register procedures that will be invoked from remote client */
void mongoTest::Shard1::registerProcedures () {
	REGISTER_PROCEDURE1 (_Shard1::insertData);
	REGISTER_PROCEDURE2 (_Shard1::updateData);
	REGISTER_PROCEDURE1 (_Shard1::killer);
}

/** Deploy Mongo shard set on servers in cluster and launch inserter, updaters, and killer on clients in cluster */
void mongoTest::Shard1::operator() () {
	mongoDeploy::ShardSet s = deploy ();

	// One insert actor and three update actors running on arbitrary clients in cluster
	vector< pair< remote::Host, Action0<Unit> > > actors = zip (cluster::someClients(3), mapAct (action1 (PROCEDURE2(_Shard1::updateData), s), enumerate<unsigned>(3)));
	actors.push_back (make_pair (cluster::someClient(), action0 (PROCEDURE1 (_Shard1::insertData), s)));

	// One killer actor running on arbitrary client in cluster
	vector< pair< remote::Host, Action0<Unit> > > killers;
	killers.push_back (make_pair (cluster::someClient(), action0 (PROCEDURE1 (_Shard1::killer), s)));

	// Launch all actors, if any fail then stop all actors
	rthread::parallel (actors, killers);
}
