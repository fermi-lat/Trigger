//$Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/mainpage.h,v 1.4 2004/07/25 21:13:59 burnett Exp $
// (Special "header" just for doxygen)

/*! @mainpage  package Trigger

Implements 
- TriggerAlg 
- ThrottleAlg

\section s1 TriggerAlg properties
TriggerAlg analyzes the digis for trigger conditions, and optionally 
sets a flag to abort processing of subsequent algorithms in the same sequence.

@param mask [0xffffffff]  Mask to apply to the trigger word to decide if event is accepted; if 0, accept all; default accept any set bit
@param  run  [0]      Run number to apply if MC run
@param deadtime [0.]  Livetime threshold to use to set b_LIVETIME in trigger mask   
    
\section s2 Triggerword bit definitions
    @verbatim
b_ACDL =     1,  ///  set if cover or side veto, low threshold
b_ACDH =     2,  ///  cover or side veto, high threshold
b_Track=     4,  ///  3 consecutive x-y layers hit
b_LO_CAL=    8,  ///  single log above low threshold
b_HI_CAL=   16,  /// single log above high threshold
b_THROTTLE= 32,  /// Ritz throttle
b_LIVETIME= 64,  /// happened after livetime threshold (and it is >0)
    @endverbatim

  <hr>
  \section requirements requirements
  \include requirements
*/

