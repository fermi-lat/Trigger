# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/Trigger/SConscript,v 1.12 2009/11/12 01:13:26 jrb Exp $ 
# Authors: T. Burnett <tburnett@u.washington.edu>
# Version: Trigger-07-01-02-gr01
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

