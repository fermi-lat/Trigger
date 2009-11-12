# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/Trigger/SConscript,v 1.11 2009/11/10 23:41:06 jrb Exp $ 
# Authors: T. Burnett <tburnett@u.washington.edu>
# Version: Trigger-07-01-02
Import('baseEnv')
Import('listFiles')
Import('packages')
progEnv = baseEnv.Clone()
libEnv = baseEnv.Clone()

libEnv.Tool('TriggerLib', depsOnly = 1)
Trigger = libEnv.SharedLibrary('Trigger',  listFiles(['src/*.cxx']))

progEnv.Tool('TriggerLib')

test_Trigger = progEnv.GaudiProgram('test_Trigger', listFiles(['src/test/*.cxx']), test = 1)

progEnv.Tool('registerTargets', package = 'Trigger',
             libraryCxts = [[Trigger, libEnv]],
             testAppCxts = [[test_Trigger, progEnv]],
             includes = listFiles(['Trigger/*.h']))

