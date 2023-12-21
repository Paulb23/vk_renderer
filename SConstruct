#!/usr/bin/env python

EnsureSConsVersion(3, 0, 0)
EnsurePythonVersion(3, 6)

env = Environment(CPPPATH=['usr/include', '/opt/local/include','#.'])
env.Append(CCFLAGS=["-g3", "-Werror", "-Wextra", "-Wall", "-Wshadow", "-Wfloat-equal", "-Wpointer-arith", "-Wcast-align", "-Wstrict-prototypes", "-Wstrict-overflow=5", "-Wwrite-strings", "-Waggregate-return", "-Wcast-qual", "-Wswitch-default", "-Wswitch-enum", "-Wconversion", "-Wunreachable-code", "-Wformat=2", "-D_REENTRANT"])
env.Append(LIBS=['SDL2main','SDL2']);

def add_sources(self, sources, filetype, lib_env = None, shared = False):
	import glob;
	import string;
	#if not lib_objects:
	if not lib_env:
		lib_env = self
	if type(filetype) == type(""):

		dir = self.Dir('.').abspath
		list = glob.glob(dir + "/"+filetype)
		for f in list:
			sources.append( self.Object(f) )
	else:
		for f in filetype:
			sources.append(self.Object(f))

env.__class__.add_sources=add_sources

Export("env")

env.libs=[]

SConscript("src/SCsub")
SConscript('bin/SCsub');
