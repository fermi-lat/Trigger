/**
*  @file EnginePrescaleCounter.cxx
*  @brief Keeps track of the counters to prescale trigger engines
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/EnginePrescaleCounter.cxx,v 1.1 2007/05/30 18:05:58 kocian Exp $
*/
#include "EnginePrescaleCounter.h"
#include <assert.h>

EnginePrescaleCounter::EnginePrescaleCounter(const std::vector<int>& prescales):m_prescales(prescales),m_useprescales(false){
  if (!m_prescales.empty())m_useprescales=true;
  for (int i=0;i<16;i++)m_counter[i]=0;
}
void EnginePrescaleCounter::reset(){
  for (int i=0;i<16;i++)m_counter[i]=0;
}
  
const bool EnginePrescaleCounter::decrementAndCheck(int condsummary, const TrgConfig *tcf){
  int enginenumber=tcf->lut()->engineNumber(condsummary);
  bool retval=false;
  m_counter[enginenumber]--;
  if (m_counter[enginenumber]<0){
    if (m_useprescales){
      assert((unsigned)enginenumber<m_prescales.size());//Not enough prescales defined for trigger engines
      m_counter[enginenumber]=m_prescales[enginenumber];
    }
    else m_counter[enginenumber]=tcf->trgEngine()->prescale(enginenumber);
  }
  if(m_counter[enginenumber]==0 && !tcf->trgEngine()->inhibited(enginenumber)) retval=true;
  return retval;
}

