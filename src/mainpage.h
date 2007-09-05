//$Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/mainpage.h,v 1.11 2007/04/11 01:32:11 burnett Exp $
// (Special "header" just for doxygen)

/*! @mainpage  package Trigger

Implements 
- TriggerAlg 
- ThrottleAlg
- TrgConfigSvc

\section s1 TriggerAlg properties
TriggerAlg analyzes the digis for trigger conditions, and optionally 
sets a flag to abort processing of subsequent algorithms in the same sequence. 

@param mask [0xffffffff]  Mask to apply to the trigger word to decide if event is accepted; if 0, accept all; default accept any set bit
@param deadtime [0.]  Livetime threshold (s). Ignore if zero. 

@param throttle if set, veto when throttle bit is on
@param vetomask [1+2+4]  if thottle it set, veto if trigger masked with these ...
@param vetobits[ 1+2]    equals these bits
@param engine [""]   specify data source for engine data. "default" and "TrgConfigSvc are other options
@param prescales []  allow to override the prescales. Should be alist of 12 integers

If the deadtime is enabled, this much time is subtracted from the accumulated livetime associated with the given 
event, which is copied to the event header. (But note that deadtime is now managed by LivetimeSvc.)




The trigger word, defined below, is copied to the event header.


    \section s3 New Triggerword bit definitions: this is copied from enums/TriggerBits.h  

@verbatim
        b_ROI =      1,  ///<  throttle
        b_Track=     2,  ///>  3 consecutive x-y layers hit
        b_LO_CAL=    4,  ///>  single log above low threshold
        b_HI_CAL=    8,  ///> single log above high threshold
        b_ACDH =    16,  ///>  cover or side veto, high threshold ("CNO")
        b_THROTTLE= 32,  ///>  throttle
        b_ACDL =     64,  ///>  set if cover or side veto, low threshold

        number_of_trigger_bits = 7, ///> for size of table

        GEM_offset = 8  ///> offset to the GEM bits
@endverbatim
This new version has the GEM bits coped to the upper 8 bits of the trigger word.

\section s5 LivetimeSvc properties

@param Deadtime [25.0e-6]       deadtime to apply to trigger, in sec.
@param TriggerRate  [2000.]     effective total trigger rate

\section s5 TrgConfigSvc properties
To use the TrgConfigSvc, we first must set up TriggerAlg as follows:
TriggerAlg.engine="TrgConfigSvc";

Note that there are three mutually exclusive ways to use the TrgConfigSvc:  Using the default configuration, 
an input XML file, or MOOT

@param useDefaultConfiguration [false]  if set to true use the default config file stored in /src/defaultTrgConfig.xml

@param configureFromFile [false] if set to true, we configure using an input XML file
@param xmlFile path to the config XML file

@param configureFromMoot [false] if set to true, use the MOOT DB to configure
@param useKeyFromData [false] if set to true use the DB key stored in the event data
@param fixedKey [1852] if useKeyFromData is false, then use this fixed key for all events

  <hr>
  \section requirements requirements
  \include requirements
*/

