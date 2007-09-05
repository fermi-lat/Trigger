/** 
* @file Trigger_load.cxx
* @brief This is needed for forcing the linker to load all components
* of the library.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/Trigger_load.cxx,v 1.8 2007/08/16 05:06:10 heather Exp $
*/

#include "GaudiKernel/DeclareFactoryEntries.h"

DECLARE_FACTORY_ENTRIES(Trigger) {
    DECLARE_ALGORITHM( TriggerAlg );
    DECLARE_ALGORITHM( TriRowBitsAlg );
    DECLARE_SERVICE( LivetimeSvc );
    DECLARE_SERVICE( TrgConfigSvc );
} 

