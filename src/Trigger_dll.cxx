//====================================================================
//
//====================================================================

// DllMain entry point
#include "GaudiKernel/DllMain.icpp"
#include <iostream>
void GaudiDll::initialize(void*) 
{
}

void GaudiDll::finalize(void*) 
{
}
extern void trigger_load();
#include "GaudiKernel/FactoryTable.h"

extern "C" FactoryTable::EntryList* getFactoryEntries() {
  static bool first = true;
  if ( first ) {  // Don't call for UNIX
    trigger_load();
    first = false;
  }
  return FactoryTable::instance()->getEntries();
} 

//needed for this
void FATAL(const char * msg) {
    std::cerr << "Fatal error from trigger DLL: " << msg << std::endl; exit(-1);
}

void WARNING(const char * msg) {
    std::cerr << "Warning message from trigger DLL:" << msg << std::endl;
}
