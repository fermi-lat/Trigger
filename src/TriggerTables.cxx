/**
*  @file TriggerTables.cxx
*  @brief Implementation of the class TriggerTables
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerTables.cxx,v 1.2 2007/04/07 21:59:24 burnett Exp $
*/

#include "TriggerTables.h"

#include <stdexcept>

using namespace Trigger;


TriggerTables::TriggerTables(std::string type)
{
// make a default table for now
    if( type!="default" ){
        throw std::invalid_argument("TriggerTables only accepts \"default\" configuration");
    }

    push_back(Engine("1 x x x x x x x", 0,-1)); //0
    push_back(Engine("0 x x x x x 0 1", 0,-1)); //1
    push_back(Engine("0 1 x x x x x x", 0,-1)); //2
    push_back(Engine("0 0 1 x x x x x", 0,-1)); //3

    // CNO guys
    push_back(Engine("0 0 0 1 x 1 1 1", 1, 0)); //4 addept CNO only with CALHI+CALLO+ROI
    push_back(Engine("0 0 0 1 x x x x", 0,-1)); //5

    // gamma patterns
    push_back(Engine("0 0 0 0 1 x x x", 2, 0)); //6  ACDH+ anything
    push_back(Engine("0 0 0 0 0 x 1 0", 3, 0)); //7  TKR, not ROI possible CALLO
    push_back(Engine("0 0 0 0 0 1 0 0", 4,-1)); //8  veto CALLO only
    push_back(Engine("0 0 0 0 0 1 1 1", 0, 0)); //9

    // usually vetoed
    push_back(Engine("0 0 0 0 0 0 1 1", 0,-1)); //10 -veto if TRK+ROI
    // cannot happen
    push_back(Engine("0 0 0 0 0 0 0 0", 0,-1)); //11

    // cache table of Engine for each bit pattern
    m_table.resize(256);
    for(int t=0; t<256; ++t){
        int n = engineNumber(t);
        const Engine& e(at(n));
        m_table[t]=&e;
    }
}

int TriggerTables::engineNumber(int gltword)const
{
    for(const_iterator it = begin(); it!=end(); ++it){
        if( it->match(gltword&255) ) return it-begin(); 
    }
    return -1; // this should not  happen: will cause assert failure
}

int TriggerTables::operator()(int gltword)const
{
    // get the cached engine object, check to see if enabled
    const Engine& e = *m_table[gltword&255];
    return e.check();
}


void TriggerTables::print(std::ostream& out )const
{
    int n(0);
    out << "Trigger tables\n num\tExt  solic period   CNO    CALHI  CALLO  TRK    ROI   prescale  marker\n";
                        //0       1      x      x      x      x      x      x      x
    for( const_iterator it=begin(); it!=end(); ++it){
        out << n++;
        (*it).print(out);
    }
    out << std::endl;
}
