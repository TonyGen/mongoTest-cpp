/* Deploy a mongo system, query and update in face of network interruptions. Error if query unexpected data */

#include <unistd.h>  // sleep
#include <iostream>
#include <mongoDeploy/mongoDeploy.h>
#include <cluster/cluster.h>
#include "mongoTest.h"

static mongoDeploy::ShardSet deploy () {
	using namespace mongo;
	BSONObj info;

	// Launch 2 shards of replica-sets (2 mongod + 1 arbiter), 1 config, and 1 mongos
	program::Options sopts;
	sopts.push_back (std::make_pair ("chunkSize", "2"));
	mongoDeploy::ShardSet s = mongoDeploy::startShardSet (cluster::someServers(1), cluster::someClients(1), program::emptyOptions, sopts);
	program::Options opts;
	opts.push_back (std::make_pair ("dur", ""));
	opts.push_back (std::make_pair ("oplogSize", "200"));
	opts.push_back (std::make_pair ("noprealloc", ""));
	std::vector<mongoDeploy::RsMemberSpec> specs;
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	program::Options opts1;
	opts1.push_back (std::make_pair ("dur", ""));
	opts1.push_back (std::make_pair ("oplogSize", "4"));
	opts1.push_back (std::make_pair ("noprealloc", ""));
	specs.push_back (mongoDeploy::RsMemberSpec (opts1, BSON ("arbiterOnly" << true)));
	s.addStartShard (cluster::someServers(3), specs);
	s.addStartShard (cluster::someServers(3), specs);

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

static unsigned long long numDocs = 200000;
static unsigned long long batchSize = 1000;

static vector<mongo::BSONObj> docs (unsigned round, unsigned count) {
	string text = mongoTest::makeText ();
	vector<mongo::BSONObj> docs;
	for (unsigned i = 0; i < count; i++)
		docs.push_back (BSON ("_id" << (count * round) + i << "x" << i << "text" << text));
	return docs;
}

static void confirmWrite (mongo::DBClientConnection& c) {
	mongo::BSONObj info;
	mongo::BSONObj cmd = BSON ("getlasterror" << 1 << "fsync" << true << "w" << 2);
	std::cout << cmd << " -> " << std::endl;
    c.runCommand ("test", cmd, info);
	std::cout << info << std::endl;
}

/** Use namespace for Procedures to avoid name clashes */
namespace _Shard2 {

void insertData (mongoDeploy::ShardSet s) {
	using namespace mongo;
	BSONObj info;

	std::string h = mongoDeploy::hostAndPort (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	for (unsigned i = 0; i < numDocs/batchSize; i++) {
		std::cout << "insert round " << i << std::endl;
		try {
			c.insert ("test.col", docs (i, batchSize));
			confirmWrite (c);
		} catch (...) {
			std::cout << "Insert connection failed, retry next round" << std::endl;
		}
		job::sleep (3);
	}

	try {
		BSONObj cmd = BSON ("dbstats" << 1);
		cout << cmd << " -> " << endl;
		c.runCommand ("test", cmd, info);
		cout << info << endl;
	} catch (...) {}
}

void updateData (mongoDeploy::ShardSet s, unsigned z) {
	using namespace mongo;

	std::string h = mongoDeploy::hostPort (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	for (unsigned i = 0; i < 50; i++) {
		unsigned long long n;
		std::cout << "update " << z << " round " << i << std::endl;
		job::sleep (z + 15);
		try {
			c.update ("test.col", BSON ("x" << i), BSON ("$push" << BSON ("z" << z)), false, true);
			confirmWrite (c);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		//mongoTest::checkEqual (n, numDocs/batchSize);
		job::sleep (z);
		try {
			c.update ("test.col", BSON ("x" << i), BSON ("$pull" << BSON ("z" << z)), false, true);
			confirmWrite (c);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		//mongoTest::checkEqual (n, (unsigned long long) 0);
	}
}

/** All replica-set shard process excluding arbiters */
static std::vector<remote::Process> activeShardProcesses (mongoDeploy::ShardSet s) {
	std::vector<remote::Process> procs;
	for (unsigned i = 0; i < s.shards.size(); i ++)
		procs.insert (procs.end(), s.shards[i].replicas.begin(), --s.shards[i].replicas.end());
		// assumes last is arbiter. TODO: be smarter
	return procs;
}

void killer (mongoDeploy::ShardSet s) {
	std::vector<remote::Process> procs = activeShardProcesses (s);
	job::sleep (rand() % 10);
	while (true) {
		unsigned r = rand() % procs.size();
		remote::Process p = procs[r];
		remote::signal (SIGKILL, p);
		std::cout << "Killed " << p << std::endl;
		job::sleep (rand() % 30);
		remote::restart (p);
		std::cout << "Restarted  " << p << std::endl;
		job::sleep (rand() % 60);
	}
}
}

void mongoTest::Shard2::registerProcedures () {
	REGISTER_PROCEDURE1 (_Shard2::insertData);
	REGISTER_PROCEDURE2 (_Shard2::updateData);
	REGISTER_PROCEDURE1 (_Shard2::killer);
}

void mongoTest::Shard2::operator() () {
	using namespace std;
	mongoDeploy::ShardSet s = deploy ();
	vector< pair< remote::Host, boost::function0<void> > > mods;
	boost::function0<void> load = boost::bind (PROCEDURE1 (_Shard2::insertData), s);
	boost::function1<void,unsigned> access = boost::bind (PROCEDURE2 (_Shard2::updateData), s, _1);
	mods.push_back (make_pair (remote::thisHost(), load));
	push_all (mods, cluster::clientActs (2, access));
	vector< pair< remote::Host, boost::function0<void> > > kills;
	boost::function0<void> kill = boost::bind (PROCEDURE1 (_Shard2::killer), s);
	kills.push_back (make_pair (remote::thisHost(), kill));
	remote::parallel (mods, kills);

	/*	start (networkProblems);
	start (addRemoveServers);
	sleep (hours (24));
	stopAll (); */
}
