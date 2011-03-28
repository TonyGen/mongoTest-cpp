/* Declaration of all test routines, plus helper functions for creating tests */

#ifndef TESTS_H_
#define TESTS_H_

#include <cluster/run.h>
#include <string>
#include <sstream>
#include <vector>
#include <mongo/client/dbclient.h>

namespace mongoTest {

class Shard1 : public clusterRun::Routine {
	void registerProcedures ();  // override
	void operator() ();  // override
};

class BadResult : public std::exception {
public:
	std::string message;
	BadResult (std::string message) : message(message) {}
	~BadResult () throw () {}
	const char* what() const throw () {  // override
		return message.c_str();
	}
};

template <class A> void checkEqual (A x, A y) {
	if (x != y) {
		stringstream ss;
		ss << "Mismatch: " << x << " != " << y;
		throw BadResult (ss.str());
	}
}

}

#endif /* TESTS_H_ */
