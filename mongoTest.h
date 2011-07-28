/* Test-case helper functions */

#pragma once

#include <mongoDeploy/mongoDeploy.h>
#include <10cluster/cluster.h>

namespace mongoTest {

/** Specification for a replica set of given number of active servers */
std::vector<mongoDeploy::RsMemberSpec> rsSpecWithArbiter (unsigned numActiveServers);

/** Launch sharded Mongo deployment on cluster of servers with numReplicas per shard (+ one arbiter) and numShards */
mongoDeploy::ShardSet deployCluster (unsigned numReplicas, unsigned numShards);

}
