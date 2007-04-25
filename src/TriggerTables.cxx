/**
*  @file TriggerTables.cxx
*  @brief Implementation of the class TriggerTables
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerTables.cxx,v 1.7 2007/04/11 01:31:47 burnett Exp $
*/

#include "TriggerTables.h"

#include <stdexcept>

using namespace Trigger;


TriggerTables::TriggerTables(std::string type, const std::vector<int>& prescale)
{
// make a default table for now
    if( type!="default" ){
        throw std::invalid_argument("TriggerTables only accepts \"default\" configuration");
    }
    static int psdata[] = {0,0,0,0, 0, 249, 0,0,0 ,0, 49,-1};
    std::vector<int>defaultps(std::vector<int>(psdata, psdata+12));
    std::vector<int>::const_iterator ps = defaultps.begin();
    if( !prescale.empty() ){
        ps = prescale.begin();
    }

    int n(0);
    push_back(Engine("1 x x x x x x x", n++, *ps++)); //0
    push_back(Engine("0 x x x x x 0 1", n++, *ps++)); //1
    push_back(Engine("0 1 x x x x x x", n++, *ps++)); //2
    push_back(Engine("0 0 1 x x x x x", n++, *ps++)); //3

    // CNO guys
    push_back(Engine("0 0 0 1 x 1 1 1", n++, *ps++)); //4 accept CNO only with CALHI+CALLO+ROI
    push_back(Engine("0 0 0 1 x x x x", n++, *ps++)); //5

    // gamma patterns
    push_back(Engine("0 0 0 0 1 x x x", n++, *ps++)); //6  ACDH+ anything
    push_back(Engine("0 0 0 0 0 x 1 0", n++, *ps++)); //7  TKR, not ROI possible CALLO
    push_back(Engine("0 0 0 0 0 1 0 0", n++, *ps++)); //8  veto CALLO only
    push_back(Engine("0 0 0 0 0 1 1 1", n++, *ps++)); //9

    // usually vetoed
    push_back(Engine("0 0 0 0 0 0 1 1", n++, *ps++)); //10 veto if TRK+ROI
    // cannot happen
    push_back(Engine("0 0 0 0 0 0 0 0", n++, *ps++)); //11

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
    out << "Trigger tables\n num\tExt  solic period   CNO    CALHI  CALLO  TRK    ROI   prescale  number\n";
                        //0       1      x      x      x      x      x      x      x
    for( const_iterator it=begin(); it!=end(); ++it){
        out << n++;
        (*it).print(out);
    }
    out << std::endl;
}
