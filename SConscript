# -*- python -*-
# $Header: /nfs/slac/g/glast/ground/cvs/Trigger/SConscript,v 1.19 2012/04/25 04:53:29 heather Exp $ 
# Authors: T. Burnett <tburnett@u.washington.edu>
# Version: Trigger-07-05-00
Import('baseEnv')
Import('listFiles')
Import('packages')
progEnv = baseEnv.Clone()
libEnv = baseEnv.Clone()

libEnv.Tool('addLinkDeps', package='Trigger', toBuild='component')
Trigger = libEnv.ComponentLibrary('Trigger',  listFiles(['src/*.cxx']))

progEnv.Tool('TriggerLib')

test_Trigger = progEnv.GaudiProgram('test_Trigger',
                                    listFiles(['src/test/*.cxx']),
                                    test = 1, package='Trigger')

progEnv.Tool('registerTargets', package = 'Trigger',
             libraryCxts = [[Trigger, libEnv]],
             testAppCxts = [[test_Trigger, progEnv]],
             includes = listFiles(['Trigger/*.h']),
             jo = ['src/jobOptions.txt', 'src/test/jobOptions.txt'] )

