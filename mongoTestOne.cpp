/* Deploy a mongo system, query and update in face of network interruptions. Error if query unexpected data */

#include <unistd.h>  // sleep
#include <iostream>
#include <mongoDeploy/mongoDeploy.h>
#include <cluster/cluster.h>
#include "mongoTest.h"

static mongoDeploy::ReplicaSetProcess deploy () {
	program::Options opts;
	opts.push_back (std::make_pair ("dur", ""));
	std::vector< std::pair<program::Options,mongo::BSONObj> > specs;
	specs.push_back (std::make_pair (opts, mongo::BSONObj()));
	specs.push_back (std::make_pair (opts, mongo::BSONObj()));
	specs.push_back (std::make_pair (opts, BSON ("arbiterOnly" << true)));
	mongoDeploy::ReplicaSetProgram rs = mongoDeploy::replicaSet (specs);
	return mongoDeploy::startRS (rs);
}

static unsigned long long numDocs = 10000;

static void loadInitialData (mongoDeploy::ReplicaSetProcess rs) {
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
void queryAndUpdateData (mongoDeploy::ReplicaSetProcess rs, unsigned z) {
	using namespace mongo;

	vector<mongo::HostAndPort> hs = fmap (mongoDeploy::hostAndPort, rs.replicas);
	DBClientReplicaSet c (rs.name(), hs);
	bool ok = c.connect();
	if (!ok) throw std::runtime_error ("Unable to connect to replica set " + rs.name());

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
}

void killer (mongoDeploy::ReplicaSetProcess rs) {
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
}
}

void mongoTest::One::registerProcedures () {
	REGISTER_PROCEDURE2 (_One::queryAndUpdateData);
	REGISTER_PROCEDURE1 (_One::killer);
}

void mongoTest::One::operator() () {
	using namespace std;
	mongoDeploy::ReplicaSetProcess rs = deploy ();
	loadInitialData (rs);
	boost::function1<void,unsigned> access = boost::bind (PROCEDURE2 (_One::queryAndUpdateData), rs, _1);
	boost::function0<void> kill = boost::bind (PROCEDURE1 (_One::killer), rs);
	vector< pair< remote::Host, boost::function0<void> > > kills;
	kills.push_back (make_pair (remote::thisHost(), kill));
	remote::parallel (cluster::clientActs (5, access), kills);
/*	start (networkProblems);
	start (addRemoveServers);
	sleep (hours (24));
	stopAll (); */
}
