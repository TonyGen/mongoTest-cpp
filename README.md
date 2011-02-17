Install dependent libraries first:

- [cluster](https://github.com/TonyGen/cluster-cpp)
- [mongoDeploy](https://github.com/TonyGen/mongoDeploy-cpp)

Download and remove '-ccp' suffix:
	git clone git://github.com/TonyGen/mongoTest-cpp.git mongoTest
	cd mongoTest

Build program `mongoTest`:
	scons

Install program in `/usr/local/bin`:
	sudo scons install

Run its help to see usage:
	mongoTest help

Example - Run Shard2 test on localhost with random seed of 600:
	mongoTest 600 controller 127.0.0.1 mongoTest::Shard2
