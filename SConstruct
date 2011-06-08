libname = 'mongoTest'

lib = SharedLibrary (libname, Glob('*.cpp'),
	CCFLAGS = ['-g'],
	CPPPATH = ['.', '/opt/local/include'],
	LIBPATH = ['/opt/local/lib'],
	LIBS = Split ('mongoDeploy cluster remote 10util mongoclient boost_regex-mt boost_thread-mt boost_serialization-mt boost_system-mt') )

Alias ('install', '/usr/local')
Install ('/usr/local/lib', lib)
Install ('/usr/local/include/' + libname, Glob('*.h'))
