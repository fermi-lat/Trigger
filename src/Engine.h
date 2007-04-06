/**
*  @file Engine.h
*  @brief Declaration of the class Engine
*
*  $Header$
*/


#ifndef Trigger_Engine_h
#define Trigger_Engine_h

#include <vector>
#include <string>
#include <iostream>

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
    @param prescale prescale factor

    */
    typedef enum { N=0, Y=1, X=2 } BitStatus; ///< for No, Yes, maybe
    Engine( std::vector<BitStatus>condition ,int marker, int prescale=0);
    Engine( std::string condition_string ,int marker, int prescale=0);

    /// return the marker if the condtion is satisfied, or -1
    int operator()(int gltword) const;

    void reset();

    void print(std::ostream& = std::cout)const;

    int marker()const{return m_marker;}

    bool range()const{return m_4range;}


private:

    std::vector<BitStatus>m_condition;
    int m_prescale;  ///< how much to prescale
    int m_marker;    ///< code to return?
    bool m_4range;
    mutable int m_scalar;  ///< keeps a count for prescale
};


#endif
