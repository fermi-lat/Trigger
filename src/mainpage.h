//$Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/mainpage.h,v 1.6 2005/03/15 04:09:57 burnett Exp $
// (Special "header" just for doxygen)

/*! @mainpage  package Trigger

Implements 
- TriggerAlg 
- ThrottleAlg

\section s1 TriggerAlg properties
TriggerAlg analyzes the digis for trigger conditions, and optionally 
sets a flag to abort processing of subsequent algorithms in the same sequence. 

@param mask [0xffffffff]  Mask to apply to the trigger word to decide if event is accepted; if 0, accept all; default accept any set bit
@param deadtime [0.]  Livetime threshold (s). Ignore if zero. 
    
If the deadtime is enabled, this much time is subtracted from the accumulated livetime associated with the given 
event, which is copied to the event header.


    THe trigger word, defined below, is copied to the event header.

\section s2 Triggerword bit definitions (old)
    @verbatim
b_ACDL =     1,  ///  set if cover or side veto, low threshold
b_ACDH =     2,  ///  cover or side veto, high threshold
b_Track=     4,  ///  3 consecutive x-y layers hit
b_LO_CAL=    8,  ///  single log above low threshold
b_HI_CAL=   16,  /// single log above high threshold
b_THROTTLE= 32,  /// Ritz throttle
b_LIVETIME= 64,  /// happened after livetime threshold (and it is >0)
    @endverbatim

    \section s3 New Triggerword bit definitions: this is copied from enums/TriggerBits.h  

@verbatim
        b_ACDL =     1,  ///>  set if cover or side veto, low threshold
        b_Track=     2,  ///>  3 consecutive x-y layers hit
        b_LO_CAL=    4,  ///>  single log above low threshold
        b_HI_CAL=    8,  ///> single log above high threshold
        b_ACDH =    16,  ///>  cover or side veto, high threshold ("CNO")
        b_THROTTLE= 32,  ///> Ritz throttle

        number_of_trigger_bits = 6, ///> for size of table

        GEM_offset = 8  ///> offset to the GEM bits
@endverbatim
This new version has the GEM bits coped to the upper 8 bits of the trigger word.

  <hr>
  \section requirements requirements
  \include requirements
*/

