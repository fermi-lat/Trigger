# $Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/Trigger/TriggerLib.py,v 1.1 2008/08/15 21:42:37 ecephas Exp $
def generate(env, **kw):
    if not kw.get('depsOnly', 0):
        env.Tool('addLibrary', library = ['Trigger'])
    env.Tool('addLibrary', library = env['gaudiLibs'])
    env.Tool('addLibrary', library = env['clhepLibs'])
    env.Tool('EventLib')
    env.Tool('configDataLib')
    env.Tool('CalXtalResponseLib')
def exists(env):
    return 1;
