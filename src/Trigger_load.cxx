/** 
* @file Trigger_load.cxx
* @brief This is needed for forcing the linker to load all components
* of the library.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/Trigger_load.cxx,v 1.6 2004/07/25 21:13:59 burnett Exp $
*/

#include "GaudiKernel/DeclareFactoryEntries.h"

DECLARE_FACTORY_ENTRIES(Trigger) {
    DECLARE_ALGORITHM( TriggerAlg );
    DECLARE_SERVICE( LivetimeSvc );
} 

