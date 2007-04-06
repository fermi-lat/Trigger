/**
*  @file Engine.cxx
*  @brief Implementation of the class Engine
*
*  $Header$
*/
#include <stdexcept>
#include <iostream>

#include "Engine.h"

Engine::Engine( std::vector<Engine::BitStatus>condition , int marker, int prescale )
: m_condition(condition)
, m_marker(marker)
, m_prescale(prescale)
, m_4range(false)
{}

Engine::Engine( std::string condition_summary ,  int marker, int prescale)
: m_marker(marker)
, m_prescale(prescale)
, m_4range(false)
{
    int k(7);
    m_condition.resize(8);
    for( int i=0; i< condition_summary.size(); ++i){
        char c=condition_summary[i];
        switch (c){
            case ' ': break;
            case '1':
            case 'Y': m_condition[k--] = Engine::Y; break;
            case '0':
            case 'N': m_condition[k--] = Engine::N; break;
            case 'x':
            case 'X': m_condition[k--] = Engine::X; break;
            default:
                std::string err("Engine parse error, unrecognized character "+c);
                std::cerr << err << std::endl;
                throw std::invalid_argument(err);
        }
    }
    if( k!=-1){
        std::string err("Engine parse error, string wrong size: "+condition_summary);
        std::cerr << err << std::endl;
        throw std::invalid_argument(err);

    }

}

void Engine::print(std::ostream& out)const
{
    char code[]={'0', '1', 'x'};
    out << "\t";
    for(std::vector<BitStatus>::const_reverse_iterator it= m_condition.rbegin(); it!=m_condition.rend(); ++it){
        out << code[*it] << "      ";
    }
    out << std::endl;
}


int Engine::operator()(int gltword) const
{
    int i(0);
    for( ; i<8; ++i){
        int c(m_condition[i]);
        if( c== Engine::X ||  (gltword&1) == c ){
            gltword /=2 ; continue;
        }
        return -1; // fail
    }
    // pass: check prescale
    if( m_prescale==0 ) return m_marker;
    if( ++m_scalar < m_prescale) return -1;
    m_scalar=0;
    return m_marker;
}

void Engine::reset() 
{  
    m_scalar=0;
}