/* Helper functions for tests */

#include "mongoTest.h"
#include <10util/util.h>
#include <job/thread.h>
#include <execinfo.h>

using namespace std;

/** All possible routines to run.
 * Add reference to your routine below. */
static map <string, boost::shared_ptr<clusterRun::Routine> > routines () {
	map <string, boost::shared_ptr<clusterRun::Routine> > routines;
	ROUTINE (mongoTest::Shard1);
	return routines;
}

void segfaultHandler(int sig) {
  void *array[30];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 30);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, 2);
  exit(1);
}

int main (int argc, char* argv[]) {
	//signal (SIGSEGV, segfaultHandler);
	clusterRun::main (routines(), argc, argv);
}
