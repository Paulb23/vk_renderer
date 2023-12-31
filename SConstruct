#!/usr/bin/env python

EnsureSConsVersion(3, 0, 0)
EnsurePythonVersion(3, 6)

env = Environment(CPPPATH=['usr/include', '/opt/local/include','#.'])
env.Append(CCFLAGS=["-g3", "-Werror", "-Wextra", "-Wall", "-Wshadow", "-Wfloat-equal", "-Wcast-align", "-Wstrict-prototypes", "-Wstrict-overflow=2", "-Wwrite-strings", "-Wcast-qual", "-Wswitch-default", "-Wswitch-enum", "-Wunreachable-code", "-Wformat=2", "-D_REENTRANT"])
env.Append(LIBS=['SDL2main','SDL2', 'vulkan', 'm']);

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

def build_shaders(self, sources, output):
	import glob;
	import string;
	from pathlib import Path;
	shaders=[]
	
	shaders += glob.glob(sources + "*.vert")
	shaders += glob.glob(sources + "*.frag")
	
	for shader in shaders:
		file_name = str(Path(shader).stem)
		folder = str(Path(shader).parent)
		self.Glslc(output + folder + "/" + file_name + ".spv", shader)

env.__class__.add_sources=add_sources
env.__class__.build_shaders=build_shaders

Export("env")

env.libs=[]

glslc_builder = Builder(action='glslc $SOURCE -o $TARGET')
env.Append(BUILDERS={'Glslc' : glslc_builder})

env.build_shaders("shaders/", "bin/")

SConscript("src/SCsub")
SConscript('bin/SCsub');
#SConscript("thirdparty/SCsub")
