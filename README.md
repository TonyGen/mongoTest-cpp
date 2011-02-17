Install dependent libraries first

- [cluster](https://github.com/TonyGen/cluster-cpp)
- [mongoDeploy](https://github.com/TonyGen/mongoDeploy-cpp)

Remove '-ccp' suffix when downloading
	git clone git://github.com/TonyGen/mongoTest-cpp.git mongoTest
	cd mongoTest

Build program `mongoTest`
	scons

Install program in `/usr/local/bin`
	sudo scons install

Run its help to see usage
	mongoTest help
