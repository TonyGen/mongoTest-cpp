/* Helper functions for tests */

#include "mongoTest.h"
#include <10util/util.h>

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
Unit act (unsigned delay, int z) {
	using namespace std;
	cout << "Yo Simple " << delay << " " << z << " " << job::thisThread() << endl;
	job::sleep (delay);
	if (delay == 2) throw mongoTest::BadResult ("ha");
	cout << "yo done " << delay << " " << z << " " << job::thisThread() << endl;
	return unit;
}
Unit act2 () {
	using namespace std;
	cout << "act2 begin " << job::thisThread() << endl;
	job::sleep (30);
	cout << "act2 end " << job::thisThread() << endl;
	return unit;
}
}

void mongoTest::Simple::operator() () {
	Action0<Unit> act = action0 (action1 (PROCEDURE2 (_simple::act), (unsigned)10), 3);
	Unit u = act ();
	std::cout << u << std::endl;
	//std::vector< std::pair< remote::Host, Action0<Unit> > > acts;
	//acts.push_back (std::make_pair (remote::thisHost(), action0 (PROCEDURE0 (_simple::act2))));
	//remote::parallel (cluster::clientActs (4, PROCEDURE1 (_simple::act)), acts);
}

void mongoTest::Simple::registerProcedures() {
	REGISTER_PROCEDURE2 (_simple::act);
	REGISTER_PROCEDURE0 (_simple::act2);
}

/** All possible routines to run.
 * Add reference to your routine below. */
static map <string, boost::shared_ptr<clusterRun::Routine> > routines () {
	map <string, boost::shared_ptr<clusterRun::Routine> > routines;
	ROUTINE (mongoTest::One);
	ROUTINE (mongoTest::Shard1);
	ROUTINE (mongoTest::Shard2);
	ROUTINE (mongoTest::Simple);
	return routines;
}

int main (int argc, char* argv[]) {
	clusterRun::main (routines(), argc, argv);
}

/*
void mongoTest::printShardingStatus (mongo::DbClientConnection& c, bool verbose) {
	using namespace std;
	using namespace mongo;

    BSONObj ver = c.findOne ("config.version", BSONObj());
    if (ver.isEmpty()) {cout << "not a shard db!" << endl; return;}

    cout << "--- Sharding Status --- " << endl;
    cout << "  sharding version: " << ver << endl;

    cout << "  shards:" << endl;
    auto_ptr<DbClientCursor> cursor = c.query ("config.shards", BSONObj());
    while (cursor->more()) {cout << "      " << cursor->next() << endl;}

    cout << "  databases:" << endl;
    auto_ptr<DbClientCursor> cursor = c.query ("config.databases", QUERY().sort("name"));
    while (cursor->more()) {
    	BSONObj db = cursor->next();
        cout << "\t" << db << endl;
        if (db.getBoolField ("partitioned")) {
        	auto_ptr<DbClientCursor> cursor = c.query ("config.collections", QUERY ("_id" << BSON ("$regex" << "^" + db._id + ".")) .sort ("_id"));
        	while (cursor->more()) {
        		BSONObj coll = cursor->next();
                if (!coll.getBoolField("dropped")) {
                    cout << "\t\t" << coll._id << " chunks:" << endl;
                    BSONObjBuilder gb;
                    gb.append ("cond", BSON ("ns" << coll.getStringField("_id")));
                    gb.append ("key", BSON ("shard" << 1));
                    gb.appendCodeWScope ("reduce", "function (doc, out) {out.nChunks++}", BSONObj());
                    gb.append ("initial", BSON ("nChunks" << 0));
                    BSONObj info;
                    bool ok = c.runCommand ("config", gb.obj(), info);
                    if (!ok) throw runtime_error (info.toString());

                    unsigned totalChunks = 0;

                    res.forEach(function (z) {
                        totalChunks += z.nChunks;
                        output("\t\t\t\t" + z.shard + "\t" + z.nChunks);
                    });
                    if (totalChunks < 1000 || verbose) {
                        configDB.chunks.find({ns:coll._id}).sort({min:1}).forEach(function (chunk) {
                            output("\t\t\t" + tojson(chunk.min) + " -->> " + tojson(chunk.max) + " on : " + chunk.shard + " " + tojson(chunk.lastmod));
                        });
                    } else {output("\t\t\ttoo many chunksn to print, use verbose if you want to force print");}
                }
            });
        }
    });
    print(raw);
}
*/
