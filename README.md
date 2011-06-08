Install dependent libraries first:

- [cluster](https://github.com/TonyGen/cluster-cpp)
- [mongoDeploy](https://github.com/TonyGen/mongoDeploy-cpp)

Download and remove '-ccp' suffix:

	git clone git://github.com/TonyGen/mongoTest-cpp.git mongoTest
	cd mongoTest

Build library `libmongoTest.so`:

	scons

Install library in `/usr/local/lib` and header files in `/usr/local/include/mongoTest`:

	sudo scons install

Do this installation on every machine in cluster, then invoke it from `10shell`.
