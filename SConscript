# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/Trigger/SConscript,v 1.1 2008/08/15 21:22:45 ecephas Exp $ 
# Authors: T. Burnett <tburnett@u.washington.edu>
# Version: Trigger-06-03-03
Import('baseEnv')
Import('listFiles')
Import('packages')
progEnv = baseEnv.Clone()
libEnv = baseEnv.Clone()

libEnv.Tool('TriggerLib', depsOnly = 1)
Trigger = libEnv.SharedLibrary('Trigger',  listFiles(['src/*.cxx']))

progEnv.Tool('TriggerLib')
test_Trigger = progEnv.GaudiProgram('test_Trigger', listFiles(['src/test/*.cxx']), test = 1)

progEnv.Tool('registerObjects', package = 'Trigger', libraries = [Trigger], testApps = [test_Trigger], includes = listFiles(['Trigger/*.h']))
