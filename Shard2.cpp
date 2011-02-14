/* Deploy a mongo system, query and update in face of network interruptions. Error if query unexpected data */

#include <unistd.h>  // sleep
#include <iostream>
#include <mongoDeploy/mongoDeploy.h>
#include <cluster/cluster.h>
#include "mongoTest.h"

static mongoDeploy::ShardSet deploy () {
	program::Options sopts;
	sopts.push_back (std::make_pair ("chunkSize", "2"));
	mongoDeploy::ShardSet s = mongoDeploy::startShardSet (cluster::someServers(1), cluster::someClients(1), program::emptyOptions, sopts);
	program::Options opts;
	opts.push_back (std::make_pair ("dur", ""));
	opts.push_back (std::make_pair ("oplogSize", "300"));
	opts.push_back (std::make_pair ("noprealloc", ""));
	std::vector<mongoDeploy::RsMemberSpec> specs;
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	program::Options opts1;
	opts1.push_back (std::make_pair ("dur", ""));
	specs.push_back (mongoDeploy::RsMemberSpec (opts1, BSON ("arbiterOnly" << true)));
	s.addStartShard (cluster::someServers(3), specs);
	s.addStartShard (cluster::someServers(3), specs);
	return s;
}

static unsigned long long numDocs = 30000;

static vector<mongo::BSONObj> docs (unsigned round, unsigned count) {
	string text = mongoTest::makeText ();
	vector<mongo::BSONObj> docs;
	for (unsigned i = 0; i < count; i++)
		docs.push_back (BSON ("_id" << (count * round) + i << "text" << text));
	return docs;
}

static void confirmWrite (mongo::DBClientConnection& c) {
	mongo::BSONObj info;
	mongo::BSONObj cmd = BSON ("getlasterror" << 1 << "fsync" << true << "w" << 2);
	std::cout << cmd << " -> " << std::endl;
    c.runCommand ("test", cmd, info);
	std::cout << info << std::endl;
}

static void loadInitialData (mongoDeploy::ShardSet s) {
	using namespace mongo;
	BSONObj info;

	std::string h = mongoDeploy::hostAndPort (s.routers[0]);
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

	for (unsigned i = 0; i < numDocs/1000; i++)
		c.insert ("test.col", docs (i, 1000));
	confirmWrite (c);

	cmd = BSON ("dbstats" << 1);
	cout << cmd << " -> " << endl;
    c.runCommand ("test", cmd, info);
	cout << info << endl;
}

/** Use namespace for Procedures to avoid name clashes */
namespace _Shard2 {
void queryAndUpdateData (mongoDeploy::ShardSet s, unsigned z) {
	using namespace mongo;

	std::string h = mongoDeploy::hostPort (s.routers[0]);
	DBClientConnection c;
	c.connect (h);
 std::cout << "3. Q&U " << z << std::endl;

	for (unsigned i = 0; i < 100; i++) {
		unsigned long long n;
		std::cout << "update " << z << " round " << i << std::endl;
		job::sleep (z + 5);
		try {
			c.update ("test.col", BSONObj(), BSON ("$push" << BSON ("z" << z)), false, true);
			confirmWrite (c);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		//mongoTest::checkEqual (n, numDocs);
		try {
			c.update ("test.col", BSONObj(), BSON ("$pull" << BSON ("z" << z)), false, true);
			confirmWrite (c);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		//mongoTest::checkEqual (n, (unsigned long long) 0);
	}
}

static std::vector<remote::Process> allShardProcesses (mongoDeploy::ShardSet s) {
	std::vector<remote::Process> procs;
	for (unsigned i = 0; i < s.shards.size(); i ++)
		procs.insert (procs.end(), s.shards[i].replicas.begin(), s.shards[i].replicas.end());
	return procs;
}

void killer (mongoDeploy::ShardSet s) {
	std::vector<remote::Process> procs = allShardProcesses (s);
	while (true) {
		job::sleep (rand() % 60);
		unsigned r = rand() % procs.size() - 1;  //exclude config server
		remote::Process p = procs[r+1];
		remote::signal (SIGKILL, p);
		std::cout << "Killed " << p << std::endl;
		job::sleep (rand() % 30);
		remote::restart (p);
		std::cout << "Restarted  " << p << std::endl;
	}
}
}

void mongoTest::Shard2::registerProcedures () {
	REGISTER_PROCEDURE2 (_Shard2::queryAndUpdateData);
	REGISTER_PROCEDURE1 (_Shard2::killer);
}

void mongoTest::Shard2::operator() () {
	using namespace std;
	mongoDeploy::ShardSet s = deploy ();
	loadInitialData (s);
	cout << "1. loaded" << endl;
	boost::function1<void,unsigned> access = boost::bind (PROCEDURE2 (_Shard2::queryAndUpdateData), s, _1);
	boost::function0<void> kill = boost::bind (PROCEDURE1 (_Shard2::killer), s);
	vector< pair< remote::Host, boost::function0<void> > > kills;
	kills.push_back (make_pair (remote::thisHost(), kill));
	cout << "2. functions bound" << endl;
	remote::parallel (cluster::clientActs (5, access), kills);

	/*	start (networkProblems);
	start (addRemoveServers);
	sleep (hours (24));
	stopAll (); */
}
