# $Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/Trigger/TriggerLib.py,v 1.3 2009/11/10 23:41:06 jrb Exp $
def generate(env, **kw):
    if not kw.get('depsOnly', 0):
        env.Tool('addLibrary', library = ['Trigger'])
        if env['PLATFORM']=='win32' and env.get('CONTAINERNAME','')=='GlastRelease':
	    env.Tool('findPkgPath', package = 'Trigger') 
    env.Tool('addLibrary', library = env['gaudiLibs'])
    env.Tool('addLibrary', library = env['clhepLibs'])
    env.Tool('EventLib')
    env.Tool('GlastSvcLib')
    env.Tool('configDataLib')
    env.Tool('ConfigSvcLib')
    env.Tool('CalXtalResponseLib')
    env.Tool('LdfEventLib')
    env.Tool('identsLib')
    if env['PLATFORM']=='win32' and env.get('CONTAINERNAME','')=='GlastRelease':
        env.Tool('findPkgPath', package = 'enums')
def exists(env):
    return 1;
