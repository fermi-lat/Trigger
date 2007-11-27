//$Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/mainpage.h,v 1.12 2007/09/05 16:43:38 heather Exp $
// (Special "header" just for doxygen)

/*! @mainpage  package Trigger

Implements 
- TriggerAlg 
- TrgConfigSvc
- LivetimeSvc

\section s1 TriggerAlg properties
TriggerAlg analyzes the digis for trigger conditions, and optionally 
sets a flag to abort processing of subsequent algorithms in the same sequence. 

@param mask [0xffffffff]  Mask to apply to the trigger word to decide if event is accepted; if 0, accept all; default accept any set bit
@param throttle if set, veto when throttle bit is on
@param vetomask [1+2+4]  if thottle it set, veto if trigger masked with these ...
@param vetobits [1+2]    equals these bits
@param engine [""]   specify data source for engine data. "default" and "TrgConfigSvc are other options
@param prescales []  allow to override the prescales. Should be alist of 12 integers
@param applyPrescales [false] if using TrgConfigSvc, do we want to prescale events?
@param useGltWordForData [false]   Even if a GEM word exists use the Glt word
@param applyWindowMask [false] Do we want to filter events using the window open mask? Needs to be true for proper GEM simulation
@param applyDeadtime [false] Filter events based on simulated GEM deadtime supplied by LivetimeSvc



The trigger word, defined below, is copied to the event header.


    \section s3 New Triggerword bit definitions: this is copied from enums/TriggerBits.h  

@verbatim
        b_ROI =      1,  ///<  throttle
        b_Track=     2,  ///>  3 consecutive x-y layers hit
        b_LO_CAL=    4,  ///>  single log above low threshold
        b_HI_CAL=    8,  ///> single log above high threshold
        b_ACDH =    16,  ///>  cover or side veto, high threshold ("CNO")

        number_of_trigger_bits = 5, ///> for size of table

        GEM_offset = 8  ///> offset to the GEM bits
@endverbatim
This new version has the GEM bits coped to the upper 8 bits of the trigger word.

\section s4 LivetimeSvc properties

@param Deadtime [26.45e-6]       deadtime to apply to trigger for 1-range readout, in sec.
@param DeadtimeLong [65.4e-6]       deadtime to apply to trigger for 4-range readout, in sec.
@param clockrate [20e6]    Number of GEM clock ticks in one second.
@param TriggerRate  [2000.]     effective total trigger rate.
@param InterleaveMode [true]    Apply efficiency correction for interleave mode.

\section s5 TrgConfigSvc properties

To use the TrgConfigSvc, we first must set up TriggerAlg as follows:
TriggerAlg.engine="TrgConfigSvc";

Note that there are three mutually exclusive ways to use the TrgConfigSvc:  Using the default configuration, 
an input XML file, or MOOT

@param configureFrom ["Default"]  Where do we find the configuration? Options are "Default", "File", "Moot".
@param xmlFile path to the config XML file if configuring from a file.

@param useKeyFromData [true] if set to true use the DB key stored in the event data to find configuration.
@param fixedKey [1852] if useKeyFromData is false, then use this fixed key for all events.

  <hr>
  \section requirements requirements
  \include requirements
*/

