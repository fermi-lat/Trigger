// $Header: /nfs/slac/g/glast/ground/cvs/trigger/src/trigger_load.cxx,v 1.2 2001/05/28 23:47:37 burnett Exp $
//====================================================================
//
//  Description: Implementation of <Package>_load routine.
//               This routine is needed for forcing the linker
//               to load all the components of the library. 
//
//====================================================================

#include "GaudiKernel/ICnvFactory.h"
#include "GaudiKernel/ISvcFactory.h"
#include "GaudiKernel/IAlgFactory.h"


#define DLL_DECL_SERVICE(x)    extern const ISvcFactory& x##Factory; x##Factory.addRef();
#define DLL_DECL_CONVERTER(x)  extern const ICnvFactory& x##Factory; x##Factory.addRef();
#define DLL_DECL_ALGORITHM(x)  extern const IAlgFactory& x##Factory; x##Factory.addRef();
#define DLL_DECL_OBJECT(x)     extern const IFactory& x##Factory; x##Factory.addRef();

//! Load all  services: 
void trigger_load() {
    DLL_DECL_ALGORITHM( Level1 );
    DLL_DECL_ALGORITHM( ACDthrottle );

} 

extern "C" void trigger_loadRef()    {
    trigger_load();
}

