
#include "mongoTest.h"
#include <10cluster/cluster.h>

using namespace std;
using namespace mongo;

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
