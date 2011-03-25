/* Deploy a mongo system, query and update in face of network interruptions. Error if query unexpected data */

#include <unistd.h>  // sleep
#include <iostream>
#include <mongoDeploy/mongoDeploy.h>
#include <cluster/cluster.h>
#include "mongoTest.h"
#include <job/thread.h>
#include <remote/thread.h>

static mongoDeploy::ReplicaSet deploy () {
	program::Options opts;
	opts.push_back (std::make_pair ("dur", ""));
	opts.push_back (std::make_pair ("oplogSize", "600"));
	std::vector<mongoDeploy::RsMemberSpec> specs;
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (opts, mongo::BSONObj()));
	specs.push_back (mongoDeploy::RsMemberSpec (opts, BSON ("arbiterOnly" << true)));
	return mongoDeploy::startReplicaSet (cluster::someServers(3), specs);
}

static unsigned long long numDocs = 10000;

static void loadInitialData (mongoDeploy::ReplicaSet rs) {
	using namespace mongo;
	BSONObj info;

	vector<mongo::HostAndPort> hs = fmap (mongoDeploy::hostAndPort, rs.replicas);
	DBClientReplicaSet c (rs.name(), hs);
	bool ok = c.connect();
	if (!ok) throw std::runtime_error ("Unable to connect to replica set " + rs.name());

	for (unsigned i = 0; i < numDocs/1000; i++)
		c.insert ("test.col", mongoTest::xDocs (1000));

	BSONObj getLastErr = BSON ("getlasterror" << 1 << "fsync" << true << "w" << 2);
	cout << getLastErr << " -> " << endl;
    c.runCommand ("test", getLastErr, info);
	cout << info << endl;
}

/** Use namespace for Procedures to avoid name clashes */
namespace _One {
Unit queryAndUpdateData (mongoDeploy::ReplicaSet rs, unsigned z) {
	using namespace mongo;

	vector<mongo::HostAndPort> hs = fmap (mongoDeploy::hostAndPort, rs.replicas);
	DBClientReplicaSet c (rs.name(), hs);
	bool ok = c.connect();
	if (!ok) throw std::runtime_error ("Unable to connect to replica set " + rs.name());

	for (unsigned i = 0; i < 100; i++) {
		unsigned long long n;
		std::cout << "update " << z << " round " << i << std::endl;
		::thread::sleep (z + 5);
		try {
			c.update ("test.col", BSONObj(), BSON ("$push" << BSON ("z" << z)), false, true);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		mongoTest::checkEqual (n, numDocs);
		try {
			c.update ("test.col", BSONObj(), BSON ("$pull" << BSON ("z" << z)), false, true);
			n = c.count ("test.col", BSON ("z" << z));
		} catch (...) {
			std::cout << "First query after kill failed as expected" << std::endl;
			continue;
		}
		mongoTest::checkEqual (n, (unsigned long long) 0);
	}
	return unit;
}

Unit killer (mongoDeploy::ReplicaSet rs) {
	while (true) {
		unsigned pause = rand() % 60;
		thread::sleep (pause);
		unsigned r = rand() % rs.replicas.size();
		rprocess::signal (SIGKILL, rs.replicas[r]);
		std::cout << "Killed " << r << std::endl;
		pause = rand() % 30;
		thread::sleep (pause);
		rprocess::restart (rs.replicas[r]);
		std::cout << "Restarted  " << r << std::endl;
	}
	return unit;
}
}

void mongoTest::One::registerProcedures () {
	REGISTER_PROCEDURE2 (_One::queryAndUpdateData);
	REGISTER_PROCEDURE1 (_One::killer);
}

void mongoTest::One::operator() () {
	using namespace std;
	mongoDeploy::ReplicaSet rs = deploy ();
	loadInitialData (rs);
	vector< pair< remote::Host, Action0<Unit> > > kills;
	kills.push_back (make_pair (remote::thisHost(), action0 (PROCEDURE1 (_One::killer), rs)));
	rthread::parallel (cluster::clientActs (5, action1 (PROCEDURE2 (_One::queryAndUpdateData), rs)), kills);
/*	start (networkProblems);
	start (addRemoveServers);
	sleep (hours (24));
	stopAll (); */
}
