//$Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/mainpage.h,v 1.13 2007/11/27 21:04:25 kocian Exp $
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

\section s5 ConfigSvc properties

To use the ConfigSvc, we first must set up TriggerAlg as follows:
TriggerAlg.engine="ConfigSvc";

ConfigSvc uses MootSvc to access MOOT to get the configuration file names in MOOT, then
uses configData to read those xml files to get the configuration parameters

In all cases a null value means to take the value from MootSvc and any non-null value 
must be a full path to the override file.

@param GemXml [""]    Full path to GEM LATC xml.              
@param RoiXml [""]    Full path to ROI LATC xml.
@param GammaFilterXml [""]    Full path to GAMMA filter xml description.
@param DgnFilterXml   [""]    Full path to DGN filter xml description.
@param MipFilterXml   [""]    Full path to MIP filter xml description.            
@param HipFilterXml   [""]    Full path to HIP filter xml description                        


\section s6 MootSvc properties

@param MootArchive  [""] Full path to MOOT archive.  "" Means use $MOOT_ARCHIVE if defined, "/afs/slac/g/glast/moot" otherwise
@param UseEventKeys [true] Get keys from Event
@param scid [0]  Force a value for the sourceID.  0 -> take value from event, Flight == 77.
@param StartTime [0]  Force a value for the run start time. 0 -> take value from event
@param MootConfigKey [0]  Force a value for the MOOT key.  0 -> Get the value from MOOT
@param NoMoot[false]  true means don't use MOOT at all.

  <hr>
  \section requirements requirements
  \include requirements
*/

