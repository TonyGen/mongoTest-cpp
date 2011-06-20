libname = 'mongoTest'

lib = SharedLibrary (libname, Glob('*.cpp'),
	CCFLAGS = ['-g', '-rdynamic'],
	CPPPATH = ['.', '/opt/local/include'],
	LIBPATH = ['/opt/local/lib'],
	LIBS = Split ('mongoDeploy 10cluster 10remote 10util mongoclient boost_regex-mt boost_thread-mt boost_serialization-mt boost_system-mt') )

Alias ('install', '/usr/local')
Install ('/usr/local/lib', lib)
Install ('/usr/local/include/' + libname, Glob('*.h'))
