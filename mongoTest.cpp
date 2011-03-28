/* Helper functions for tests */

#include "mongoTest.h"
#include <10util/util.h>
#include <job/thread.h>

using namespace std;

/** All possible routines to run.
 * Add reference to your routine below. */
static map <string, boost::shared_ptr<clusterRun::Routine> > routines () {
	map <string, boost::shared_ptr<clusterRun::Routine> > routines;
	ROUTINE (mongoTest::Shard1);
	return routines;
}

int main (int argc, char* argv[]) {
	clusterRun::main (routines(), argc, argv);
}
