/** 
* @file Trigger_load.cxx
* @brief This is needed for forcing the linker to load all components
* of the library.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/Trigger_load.cxx,v 1.9 2007/09/05 16:43:38 heather Exp $
*/

#include "GaudiKernel/DeclareFactoryEntries.h"

DECLARE_FACTORY_ENTRIES(Trigger) {
    DECLARE_ALGORITHM( TriggerAlg );
    DECLARE_ALGORITHM( TriRowBitsAlg );
    DECLARE_SERVICE( LivetimeSvc );
} 

