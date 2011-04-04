progname = 'mongoTest'

prog = Program (progname, Glob('*.cpp'),
	CCFLAGS = ['-g'],
	CPPPATH = ['.', '/opt/local/include'],
	LIBPATH = ['/opt/local/lib'],
	LIBS = Split ('boost_thread-mt boost_filesystem-mt boost_system-mt boost_serialization-mt mongoclient pcre 10util job remote cluster mongoDeploy'))

Alias ('install', '/usr/local')
Install ('/usr/local/bin', prog)
