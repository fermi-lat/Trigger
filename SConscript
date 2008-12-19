# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/Trigger/SConscript,v 1.6 2008/12/19 00:07:46 glastrm Exp $ 
# Authors: T. Burnett <tburnett@u.washington.edu>
# Version: Trigger-07-00-01
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



