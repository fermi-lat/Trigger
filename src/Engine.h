/**
*  @file Engine.h
*  @brief Declaration of the class Engine
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/Engine.h,v 1.2 2007/04/07 21:59:24 burnett Exp $
*/


#ifndef Trigger_Engine_h
#define Trigger_Engine_h

#include <vector>
#include <string>
#include <iostream>
namespace Trigger {

/** @class Engine
    @brief encapsulate the data and operation of a trigger engine

    @author T. Burnett <tburnett@u.washington.edu>
*/
class Engine //: public std::vector<Engine::BitStatus>
{
public:
    /** Initialize an engine
    @param condition the condition table, list of conditions for the bits
    @marker code to return if the condition is satisfied
    @param prescale prescale factor; if <0 disable the engine. If >=0, then allow only every prescale+1 counts

    */
    typedef enum { N=0, Y=1, X=2 } BitStatus; ///< for No, Yes, maybe
    Engine( std::vector<BitStatus>condition ,int marker, int prescale=0);
    Engine( std::string condition_string ,int marker, int prescale=0);

    /// test the pattern: true if it matches the condition for this Engine
    bool match(int gltword) const;

    void reset();

    void print(std::ostream& = std::cout)const;

    int marker()const{return m_marker;}

    bool range()const{return m_4range;}

    bool enabled()const{return m_prescale>=0;}

    /// return marker if pass prescale: -1 otherwise
    int check()const;

private:

    std::vector<BitStatus>m_condition;
    int m_marker;    ///< code to return?
    int m_prescale;  ///< how much to prescale (<0 if disabled)
    bool m_4range;   ///< flag for CAL readout (not used here)
    mutable int m_scalar;  ///< keeps a count for prescale
 
};

}
#endif
