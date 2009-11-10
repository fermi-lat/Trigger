# $Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/Trigger/TriggerLib.py,v 1.2 2008/10/27 18:42:03 ecephas Exp $
def generate(env, **kw):
    if not kw.get('depsOnly', 0):
        env.Tool('addLibrary', library = ['Trigger'])
    env.Tool('addLibrary', library = env['gaudiLibs'])
    env.Tool('addLibrary', library = env['clhepLibs'])
    env.Tool('EventLib')
    env.Tool('GlastSvcLib')
    env.Tool('configDataLib')
    env.Tool('ConfigSvcLib')
    env.Tool('CalXtalResponseLib')
    env.Tool('LdfEventLib')
    env.Tool('identsLib')
def exists(env):
    return 1;
