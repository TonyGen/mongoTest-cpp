progname = 'mongoTest'

prog = Program (progname, Glob('*.cpp'),
	CCFLAGS = ['-g'],
	CPPPATH = ['.', '/opt/local/include'],
	LIBPATH = ['/opt/local/lib'],
	LIBS = Split ('boost_thread boost_filesystem boost_system boost_serialization mongoclient pcre 10util remote cluster mongoDeploy'))

Alias ('install', '/usr/local')
Install ('/usr/local/bin', prog)
