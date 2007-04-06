/**
*  @file TriggerTables.cxx
*  @brief Implementation of the class TriggerTables
*
*  $Header$
*/

#include "TriggerTables.h"


TriggerTables::TriggerTables()
{
// make a default table for now
    push_back(Engine("1 x x x x x x x", 0)); //0
    push_back(Engine("0 x x x x x 0 1", 0)); //1
    push_back(Engine("0 1 x x x x x x", 0)); //2
    push_back(Engine("0 0 1 x x x x x", 0)); //3
    push_back(Engine("0 0 0 1 x 1 1 1", 1)); //4
    push_back(Engine("0 0 0 1 x x x x", 0)); //5
    push_back(Engine("0 0 0 0 1 x x x", 2)); //6
    push_back(Engine("0 0 0 0 0 x 1 0", 3)); //7
    push_back(Engine("0 0 0 0 0 1 0 0", 4)); //8
    push_back(Engine("0 0 0 0 0 1 1 1", 0)); //9
    push_back(Engine("0 0 0 0 0 0 1 1", 0)); //10
    push_back(Engine("0 0 0 0 0 0 0 0", 0)); //11

}


int TriggerTables::operator()(int gltword)const
{
    for(const_iterator it = begin(); it!=end(); ++it){
        int ret( (*it)(gltword) );
        if( ret>=0 ) return it->marker(); 
    }
    return -1; // this should not actually happen
}

void TriggerTables::print(std::ostream& out )const
{
    int n(0);
    out << "Trigger tables\n num\tExt  solic period   CNO    CALHI  CALLO  TRK    ROI\n";
                        //0       1      x      x      x      x      x      x      x
    for( const_iterator it=begin(); it!=end(); ++it){
        out << n++;
        (*it).print(out);
    }
    out << std::endl;
}