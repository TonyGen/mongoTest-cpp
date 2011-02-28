/* Deploy a mongo system, query and update in face of network interruptions. Error if query unexpected data */

#include <unistd.h>  // sleep
#include <iostream>
#include <mongoDeploy/mongoDeploy.h>
#include <cluster/cluster.h>
#include "mongoTest.h"

static mongoDeploy::ShardSet deploy () {
	mongoDeploy::ShardSet s = mongoDeploy::startShardSet (cluster::someServers(1), cluster::someClients(1));
	program::Options opts;
	opts.push_back (std::make_pair ("dur", ""));
	opts.push_back (std::make_pair ("oplogSize", "600"));
	opts.push_back (std::make_pair ("noprealloc", ""));
	std::vector<mongoDeploy::RsMemberSpec> specs;
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (opts, BSON ("arbiterOnly" << true)));
	s.addStartShard (cluster::someServers(3), specs);
	return s;
}

static unsigned long long numDocs = 10000;

static void loadInitialData (mongoDeploy::ShardSet s) {
	using namespace mongo;
	BSONObj info;

	std::string h = mongoDeploy::hostAndPort (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	//BSONObj cmd = BSON ("enablesharding" << "test");
	//cout << cmd << " -> " << endl;
	//c.runCommand ("admin", cmd, info);
	//cout << info << endl;

	for (unsigned i = 0; i < numDocs/1000; i++)
		c.insert ("test.col", mongoTest::xDocs (1000));

	BSONObj getLastErr = BSON ("getlasterror" << 1 << "fsync" << true << "w" << 2);
	cout << getLastErr << " -> " << endl;
    c.runCommand ("test", getLastErr, info);
	cout << info << endl;
}

/** Use namespace for Procedures to avoid name clashes */
namespace _Shard1 {
Unit queryAndUpdateData (mongoDeploy::ShardSet s, unsigned z) {
	using namespace mongo;

	std::string h = mongoDeploy::hostPort (s.routers[0]);
	DBClientConnection c;
	c.connect (h);

	for (unsigned i = 0; i < 100; i++) {
		unsigned long long n;
		std::cout << "update " << z << " round " << i << std::endl;
		job::sleep (z + 5);
		try {
			c.update ("test.col", BSONObj(), BSON ("$push" << BSON ("z" << z)), false, true);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		//mongoTest::checkEqual (n, numDocs);
		try {
			c.update ("test.col", BSONObj(), BSON ("$pull" << BSON ("z" << z)), false, true);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		//mongoTest::checkEqual (n, (unsigned long long) 0);
	}
	return unit;
}

Unit killer (mongoDeploy::ShardSet s) {
	mongoDeploy::ReplicaSet rs = s.shards[0];
	while (true) {
		unsigned pause = rand() % 60;
		job::sleep (pause);
		unsigned r = rand() % rs.replicas.size();
		remote::signal (SIGKILL, rs.replicas[r]);
		std::cout << "Killed " << r << std::endl;
		pause = rand() % 30;
		job::sleep (pause);
		remote::restart (rs.replicas[r]);
		std::cout << "Restarted  " << r << std::endl;
	}
	return unit;
}
}

void mongoTest::Shard1::registerProcedures () {
	REGISTER_PROCEDURE2 (_Shard1::queryAndUpdateData);
	REGISTER_PROCEDURE1 (_Shard1::killer);
}

void mongoTest::Shard1::operator() () {
	using namespace std;
	mongoDeploy::ShardSet s = deploy ();
	loadInitialData (s);
	vector< pair< remote::Host, Action0<Unit> > > kills;
	kills.push_back (make_pair (remote::thisHost(), action0 (PROCEDURE1 (_Shard1::killer), s)));
	remote::parallel (cluster::clientActs (5, action1 (PROCEDURE2 (_Shard1::queryAndUpdateData), s)), kills);

	/*	start (networkProblems);
	start (addRemoveServers);
	sleep (hours (24));
	stopAll (); */
}
