/** @file TriggerTables.h
  *  @brief Declaration of the class TriggerTables
  *
  *  $Header$
*/

#ifndef Trigger_TriggerTables_h
#define Trigger_TriggerTables_h

#include "Engine.h"
#include <vector>
#include <iostream>

/** @class Engine
    @brief encapsulate the data and operation of the table-driven trigger scheme

    @author T. Burnett <tburnett@u.washington.edu>

    This is a list of engines
*/

class TriggerTables : public std::vector<Engine> {
public:

    /// ctor -- expect to create the engines
    TriggerTables();
    /// for a gltword, return associated engine
    int operator()(int gltword)const;

    /// make a table of the current trigger table
    void print(std::ostream& out = std::cout)const;

private:
};


#endif
