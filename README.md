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

Example - Run Shard1 test across cluster of hosts with random seed of 42:
	host1> mongoTest 42 0 worker
	host2> mongoTest 42 0 worker
	host3> mongoTest 42 0 controller host1,host2,host3 mongoTest::Shard1
