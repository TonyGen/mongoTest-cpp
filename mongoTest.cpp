/* Helper functions for tests */

#include "mongoTest.h"
#include <util/util.h>

using namespace std;

unsigned mongoTest::xDocTextSize = 1000;

string mongoTest::makeText (unsigned length) {
	stringstream ss;
	for (unsigned i = 0; i < length; i++) ss << "a";
	return ss.str();
}

vector<mongo::BSONObj> mongoTest::xDocs (unsigned count) {
	string text = makeText ();
	vector<mongo::BSONObj> docs;
	for (unsigned i = 0; i < count; i++)
		docs.push_back (BSON ("x" << i << "text" << text));
	return docs;
}


#include <cluster/cluster.h>

namespace _simple {
void act (unsigned delay) {
	using namespace std;
	cout << "Yo Simple " << delay << " " << job::thisThread() << endl;
	job::sleep (delay);
	if (delay == 2) throw mongoTest::BadResult ("ha");
	cout << "yo done " << delay << " " << job::thisThread() << endl;
}
void act2 () {
	using namespace std;
	cout << "act2 begin " << job::thisThread() << endl;
	job::sleep (30);
	cout << "act2 end " << job::thisThread() << endl;
}
}

void mongoTest::Simple::operator() () {
	std::vector< std::pair< remote::Host, boost::function0<void> > > acts;
	acts.push_back (std::make_pair (remote::thisHost(), PROCEDURE0 (_simple::act2)));
	remote::parallel (cluster::clientActs (4, PROCEDURE1 (_simple::act)), acts);
}

void mongoTest::Simple::registerProcedures() {
	REGISTER_PROCEDURE1 (_simple::act);
	REGISTER_PROCEDURE0 (_simple::act2);
}

/** All possible routines to run.
 * Add reference to your routine below. */
static map <string, boost::shared_ptr<clusterRun::Routine> > routines () {
	map <string, boost::shared_ptr<clusterRun::Routine> > routines;
	ROUTINE (mongoTest::One);
	ROUTINE (mongoTest::Shard1);
	ROUTINE (mongoTest::Simple);
	return routines;
}

int main (int argc, char* argv[]) {
	clusterRun::main (routines(), argc, argv);
}
