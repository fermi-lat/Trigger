# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/Trigger/SConscript,v 1.14 2010/06/12 22:41:14 jrb Exp $ 
# Authors: T. Burnett <tburnett@u.washington.edu>
# Version: Trigger-07-01-04
Import('baseEnv')
Import('listFiles')
Import('packages')
progEnv = baseEnv.Clone()
libEnv = baseEnv.Clone()

libEnv.Tool('addLinkDeps', package='Trigger', toBuild='component')
Trigger = libEnv.SharedLibrary('Trigger',  listFiles(['src/*.cxx']))

progEnv.Tool('TriggerLib')

test_Trigger = progEnv.GaudiProgram('test_Trigger',
                                    listFiles(['src/test/*.cxx']),
                                    test = 1, package='Trigger')

progEnv.Tool('registerTargets', package = 'Trigger',
             libraryCxts = [[Trigger, libEnv]],
             testAppCxts = [[test_Trigger, progEnv]],
             includes = listFiles(['Trigger/*.h']),
             jo = ['src/jobOptions.txt', 'src/test/jobOptions.txt'] )

