/* Two replica-set shards. Each replica set has 2 durable servers and 1 arbiter. Insert and update while the durable servers are killed and restarted. */

#pragma once

#include <mongoDeploy/mongoDeploy.h>
#include <boost/function.hpp>
#include <10util/process.h>

namespace Shard1 {
	void run ();
}

namespace _Shard1 {
	extern remote::Module module;
	void insertData (mongoDeploy::ShardSet);
	void updateData (mongoDeploy::ShardSet, unsigned);
	void killer (mongoDeploy::ShardSet);
	boost::function1<void,process::Process> watchLog ();
}
