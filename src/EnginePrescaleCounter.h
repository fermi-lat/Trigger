/**
*  @file EnginePrescaleCounter.h
*  @brief Keeps track of the counters to prescale trigger engines
*
*  $Header:  $
*/
#ifndef EnginePrescaleCounter_h
#define EnginePrescaleCounter_h

#include "configData/gem/TrgConfig.h"

class EnginePrescaleCounter{
 public:
  /// Constructor with optional vector of prescales
  EnginePrescaleCounter(const std::vector<int>& prescales);
  /// Default destructor
  ~EnginePrescaleCounter(){}
  /// Reset the prescale counters
  void reset();
  /// decrement the counter for the engine corresponding to condsummary in the config, check for expiration
  const bool decrementAndCheck(int condsummary, const TrgConfig *tcf);
 private:
  int m_counter[16];
  const std::vector<int>& m_prescales;
  int m_useprescales;
};

#endif
