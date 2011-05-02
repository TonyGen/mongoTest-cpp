progname = 'mongoTest'

prog = Program (progname, Glob('*.cpp'),
	CCFLAGS = ['-g'],
	CPPPATH = ['.', '/opt/local/include'],
	LIBPATH = ['/opt/local/lib'],
	LIBS = Split ('mongoDeploy cluster remote 10util boost_thread boost_filesystem boost_system boost_serialization mongoclient pcre'))

Alias ('install', '/usr/local')
Install ('/usr/local/bin', prog)
