/**
*  @file TriggerAlg.cxx
*  @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /usr/local/CVS/SLAC/Trigger/src/TriggerAlg.cxx,v 1.99 2008/08/01 23:44:01 echarles Exp $
*/


#include "Trigger/ILivetimeSvc.h"

#include "ConfigSvc/IConfigSvc.h"

#include "TriggerTables.h"
#include "EnginePrescaleCounter.h"
#include "configData/fsw/FswEfcSampler.h"

#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "Event/TopLevel/DigiEvent.h"
#include "Event/Trigger/TriggerInfo.h"

#include "LdfEvent/Gem.h"
#include "LdfEvent/LsfMetaEvent.h"

#include "enums/TriggerBits.h"
#include "enums/GemState.h"

#include "idents/AcdId.h"
#include "idents/TowerId.h"

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"
#include "GaudiKernel/Property.h"

#include <cassert>
#include <map>
#include <vector>
#include <algorithm>

//------------------------------------------------------------------------------
/*! \class TriggerAlg
\brief  alg that sets trigger information

@section Attributes for job options:
@param run [0] For setting the run number
@param mask [-1] mask to apply to trigger word. -1 means any, 0 means all.

*/

class TriggerAlg : public Algorithm {

public:
    //! Constructor of this form must be provided
    TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator); 

    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();

private:

    void bitSummary(std::ostream& out, std::string label, const std::map<unsigned int,unsigned int>& table);
    /// input is a list of tiles, output is LdfEvent::GemTileList object
    void makeGemTileList(const Event::TriggerInfo::TileList& tilelist, LdfEvent::GemTileList& vetotilelist);
    /// set gem bits in trigger word, either from real condition summary, or from bits
    unsigned int gemBits(unsigned int  trigger_bits);

    //! add the FSW prescale values and MOOT Key to the meta event
    StatusCode handleMetaEvent( LsfEvent::MetaEvent& metaEvent, unsigned int triggerEngine );

    //! map the engine to the DGN prescaler
    enums::Lsf::LeakedPrescaler dgnPrescaler(unsigned engine);

    /// Check the FMX keys from MOOT against those from event data, return false if we should fail the job
    bool checkFmxKey(unsigned mootKey, unsigned evtKey, enums::Lsf::Mode, enums::Lsf::HandlerId) const;
    
    unsigned int                        m_mask;
    int                                 m_acd_hits;
    int                                 m_log_hits;
    int                                 m_event;

    BooleanProperty                     m_throttle;
    BooleanProperty                     m_applyPrescales;
    BooleanProperty                     m_applyWindowMask;
    BooleanProperty                     m_applyDeadtime;
    BooleanProperty                     m_useGltWordForData;
    BooleanProperty                     m_failOnFmxKeyMismatch;
    IntegerProperty                     m_vetobits;
    IntegerProperty                     m_vetomask;
    StringProperty                      m_table;
    IntegerArrayProperty                m_prescale;

    double                              m_lastTriggerTime; //! time of last trigger, for delta window time
    double                              m_lastWindowTime;  //! time of last trigger window, for delta window open time

    // for statistics
    unsigned int                        m_total;
    unsigned int                        m_triggered;
    unsigned int                        m_deadtime_reject;
    unsigned int                        m_window_reject;
    unsigned long long                  m_prescaled;
    unsigned long long                  m_busy;
    unsigned long long                  m_deadzone;

    std::map<unsigned int,unsigned int> m_counts;           //map of values for each bit pattern
    std::map<unsigned int,unsigned int> m_window_counts;    //map of values for each bit pattern, window mask applied
    std::map<unsigned int,unsigned int> m_prescaled_counts; //map of values for each bit pattern, after prescaling
    std::map<unsigned int,unsigned int> m_trig_counts;      //map of values for each bit pattern, triggered events

    std::map<idents::TowerId, int>      m_tower_trigger_count;

    /// use for the names of the bits
    std::map<int, std::string>          m_bitNames;

    ILivetimeSvc*                       m_LivetimeSvc;
    Trigger::TriggerTables*             m_triggerTables;
    IConfigSvc*                         m_configSvc;
    EnginePrescaleCounter*              m_pcounter;
    bool                                m_printtables;
    bool                                m_firstevent;
    double                              m_firstTriggerTime;
    unsigned                            m_mootKey;
    
    std::map<unsigned int, enums::Lsf::LeakedPrescaler> m_dgnMap;
};

//------------------------------------------------------------------------------
static const AlgFactory<TriggerAlg>  Factory;
const IAlgFactory& TriggerAlgFactory = Factory;
//------------------------------------------------------------------------------
/// 
TriggerAlg::TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator) 
: Algorithm(name, pSvcLocator), m_event(0)
, m_lastTriggerTime(0)
, m_lastWindowTime(0)
, m_total(0)
, m_triggered(0)
, m_deadtime_reject(0)
, m_window_reject(0)
, m_prescaled(0)
, m_busy(0)
, m_deadzone(0)
, m_triggerTables(0)
, m_configSvc(0)
, m_pcounter(0)
, m_firstevent(true)
, m_firstTriggerTime(0)
, m_mootKey(0)
{
    declareProperty("mask"    ,              m_mask=0xffffffff);             // trigger mask
    declareProperty("throttle",              m_throttle=false);              // if set, veto when throttle bit is on
    declareProperty("vetomask",              m_vetomask=1+2+4);              // if thottle it set, veto if trigger masked with these ...
    declareProperty("vetobits",              m_vetobits=1+2);                // equals these bits

    declareProperty("engine",                m_table = "ConfigSvc");         // set to "default"  to use default engine table
    declareProperty("prescale",              m_prescale=std::vector<int>()); // vector of prescale factors
    declareProperty("applyPrescales",        m_applyPrescales=false);        // if using ConfigSvc, do we want to prescale events
    declareProperty("useGltWordForData",     m_useGltWordForData=false);     // even if a GEM word exists use the Glt word
    declareProperty("applyWindowMask",       m_applyWindowMask=false);       // Do we want to use a window open mask?
    declareProperty("applyDeadtime",         m_applyDeadtime=false);         // Do we want to apply deadtime?
    declareProperty("failOnFmxKeyMismatch",  m_failOnFmxKeyMismatch=true);   // Do we want to fail if the FMX key doesn't match?

    return;
}
//------------------------------------------------------------------------------
/*! 
*/
StatusCode TriggerAlg::initialize() 
{
    StatusCode sc = StatusCode::SUCCESS;

    MsgStream log(msgSvc(), name());

    // Use the Job options service to set the Algorithm's parameters
    setProperties();

    log << MSG::INFO;
    if(log.isActive()) {
        if (m_mask==0xffffffff) log.stream() << "No trigger requirement";
        else            log.stream() << "Applying trigger mask: " <<  std::setbase(16) <<m_mask <<std::setbase(10);

        if( m_throttle) log.stream() <<", throttled by rejecting  the value "<< m_vetobits;
    }
    log << endreq;

    sc = service("LivetimeSvc", m_LivetimeSvc);
    if( sc.isFailure() ) {
        log << MSG::ERROR << "failed to get the LivetimeSvc" << endreq;
        return sc;
    }
    
    if(! m_table.value().empty())
    {
        if (m_table.value()=="ConfigSvc")
        {
            log<<MSG::INFO<<"Using ConfigSvc for Trigger Config"<<endreq;

            sc = service("ConfigSvc", m_configSvc, true);
            if( sc.isFailure() ) 
            {
                log << MSG::ERROR << "failed to get the ConfigSvc" << endreq;
                return sc;
            }

            m_pcounter    = new EnginePrescaleCounter(m_prescale.value());
            m_printtables = true;
        }else{
            // selected trigger tables and engines for event selection
            m_triggerTables = new Trigger::TriggerTables(m_table.value(), m_prescale.value());

            log << MSG::INFO << "Trigger tables: \n";
            m_triggerTables->print(log.stream());
            log << endreq;
        }
    }
    
    // Initialize the map for outputting the bit names
    for( int i=0; i<8; ++i) 
    { 
        std::stringstream t; t<< "bit "<< i;
        m_bitNames[1<<i] = t.str();
    }
    m_bitNames[enums::b_LO_CAL] = "CALLO";
    m_bitNames[enums::b_HI_CAL] = "CALHI";
    m_bitNames[enums::b_ROI]    = "ROI";
    m_bitNames[enums::b_ACDH]   = "CNO";
    m_bitNames[enums::b_Track]  = "TKR";
#if 0 // do not use now
    m_bitNames[enums::b_ACDL]   = "ACDL";
#endif

    // Initialize the dgn map
    m_dgnMap[0]  = enums::Lsf::COND30;
    m_dgnMap[1]  = enums::Lsf::COND29;
    m_dgnMap[2]  = enums::Lsf::COND28;
    m_dgnMap[3]  = enums::Lsf::COND27;
    m_dgnMap[4]  = enums::Lsf::COND26;
    m_dgnMap[5]  = enums::Lsf::COND25;
    m_dgnMap[6]  = enums::Lsf::COND24;
    m_dgnMap[7]  = enums::Lsf::COND23;
    m_dgnMap[8]  = enums::Lsf::COND22;
    m_dgnMap[9]  = enums::Lsf::COND21;
    m_dgnMap[10] = enums::Lsf::COND20;
    m_dgnMap[11] = enums::Lsf::COND19;    

    return sc;
}




//------------------------------------------------------------------------------
StatusCode TriggerAlg::execute() 
{
    // purpose: find digi collections in the TDS, pass them to functions to calculate the individual trigger bits
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );

    // GET the MOOT key and check to see if the configuration has changed
    bool             configChanged = false;
    const TrgConfig* tcf(0);

    if (m_pcounter)     // non-zero means ConfigSvc is being used
    {
        unsigned mKey = m_configSvc->getMootKey();
        tcf           = m_configSvc->getTrgConfig();

        // Successful at recovering Trigger Configuration?
        if ( tcf == 0 ) 
        {
            log << MSG::ERROR << "Failed to get trigger config from ConfigSvc." << endreq;
            return StatusCode::FAILURE;
        }

        configChanged = mKey != m_mootKey;
        m_mootKey     = mKey;
    }

    // Is this Monte Carlo?
    SmartDataPtr<Event::DigiEvent> de(eventSvc(),   EventModel::Digi::Event);
    if( de==0 ) log << MSG::DEBUG << "No digi event found" << endreq;

    bool isMc = de ? de->fromMc() : false;
    
    // Start by recovering the trigger primitives from the TriggerInfo object which
    // we can find in the TDS (created by TriggerInfoAlg)
    SmartDataPtr<Event::TriggerInfo> triggerInfo(eventSvc(), "/Event/TriggerInfo");
    if( triggerInfo == 0) log << MSG::DEBUG << "No TriggerInfo found" << endreq;

    // set bits in the trigger word
    unsigned short tkrvector    = triggerInfo->getTkrVector();
    unsigned short cnovector    = triggerInfo->getCnoVector();
    unsigned short roivector    = triggerInfo->getRoiVector();
    unsigned short callovector  = triggerInfo->getCalLeVector();
    unsigned short calhivector  = triggerInfo->getCalHeVector();
    unsigned int   trigger_bits = triggerInfo->getTriggerBits();

    // List of struck tiles
    const Event::TriggerInfo::TileList& tilelist = triggerInfo->getTileList();

    // Accumulate some status
    m_total++;
    m_counts[trigger_bits] +=1;

    // Apply window mask. Only proceed if the window was opened 
    // or any trigger bit was set if window open mask was not available.
    if (m_applyWindowMask)
    {
        unsigned windowOpen=0;

        if (m_pcounter) // using ConfigSvc
        {
            windowOpen = trigger_bits & tcf->windowParams()->windowMask();
        }else{
            windowOpen = trigger_bits & m_mask;
        }
      
        if ( !windowOpen)
        {
            m_window_reject++;
            setFilterPassed(false);
            return sc;
        }
    }
    m_window_counts[trigger_bits] +=1;
  
    // Retrieve the EventHeader from the TDS (which, by definition of the TDS, must exist)
    SmartDataPtr<Event::EventHeader> header(eventSvc(), EventModel::EventHeader);

    // record window open time
    double         now = header->time();
    unsigned short deltawotime;

    if(m_lastWindowTime!=0)
    {
        deltawotime = m_LivetimeSvc->ticks(now-m_lastWindowTime) < 0xffff ? m_LivetimeSvc->ticks(now-m_lastWindowTime): 0xffff;
    }else{
        deltawotime = 0xffff;
    }
    m_lastWindowTime = now;

    // Retrieve GEM from the TDS
    SmartDataPtr<LdfEvent::Gem> gem(eventSvc(), "/Event/Gem"); 
    if( gem==0 ) log << MSG::DEBUG << "No GEM found" << endreq;

    // GEM information 
    int          engine(16); // default engine number
    int          gemengine(16);
    int          gltengine(16);

    // IF Gem is present (data?) then we use it to determine the trigger, otherwise, use the calculated trigger_bits
    unsigned int gemword = gem ? gem->conditionSummary() : gemBits(trigger_bits);

    // apply filter for subsequent processing.
    if( m_triggerTables!=0 )
    {
        engine = (*m_triggerTables)(trigger_bits & 0x001F); // note only apply to low 5 bits for now
        log << MSG::DEBUG << "Engine is " << engine << endreq;

        if( engine<=0 ) 
        {
            setFilterPassed(false);
            m_prescaled++;
            log << MSG::DEBUG << "Event did not trigger, according to engine selected by trigger table" << endreq;
            return sc;
        }
    } else if (m_pcounter!=0){        
        if(!m_printtables && configChanged )
        {
            log<<MSG::INFO<<"Trigger configuration changed.";
            if ( tcf != 0 ) tcf->printContrigurator(log.stream());
            log<<endreq;
            m_pcounter->reset();
        }

        if (m_printtables)
        {
            log << MSG::INFO << "Trigger tables: \n";
            if ( tcf != 0 ) tcf->printContrigurator(log.stream());
            log<<endreq;
            m_printtables=false;
            if(! m_applyPrescales ) 
            {
                log << MSG::INFO 
                    << "ConfigSvc selected, but prescale factors and inhibits are not active:\n"
                    << "\t\tset 'applyPrescales' to activate them. "
                    << endreq;
            }
        }
    
        bool passed = true;
        if (isMc || m_useGltWordForData)   // this is either MC or user wants glt word used for prescaling
        {
            passed = m_pcounter->decrementAndCheck(gemBits(trigger_bits),tcf);
        } else {                           //this is data and we want to use the GEM summary word
            assert(gem!=0);
            passed = m_pcounter->decrementAndCheck(gem->conditionSummary(),tcf); 
        }
    
        header->setPrescaleExpired(passed);
        
        // Check prescales
        if(!passed && m_applyPrescales)
        {
            setFilterPassed(false);
            m_prescaled++;
            log << MSG::DEBUG << "Event did not trigger, according to engine selected by ConfigSvc" << endreq;
            return sc;
        }

        // Retrieve the engine numbers for both the GEM and GLT
        gemengine = tcf->lut()->engineNumber(gemword);
        header->setGemPrescale(tcf->trgEngine()->prescale(gemengine));
        gltengine = tcf->lut()->engineNumber(gemBits(trigger_bits));
        header->setGltPrescale(tcf->trgEngine()->prescale(gltengine));
    }else {
        // apply throttle filter if requested
        if( m_throttle && ( (trigger_bits & m_vetomask) == (unsigned)m_vetobits ) ) 
        {
            setFilterPassed( false );
            m_prescaled++;
            log << MSG::DEBUG << "Event did not trigger" << endreq;
            return sc;
        }
    }

    // passed trigger: continue processing
    m_prescaled_counts[trigger_bits] +=1;

    // check for deadtime: set flag only if applying deadtime
    if(m_applyDeadtime)
    {
        enums::GemState gemstate=m_LivetimeSvc->checkState(now);
        if (gemstate==enums::DEADZONE)m_deadzone++;
        else if (gemstate==enums::BUSY)m_busy++;
        if( !m_LivetimeSvc->isLive(now)) 
        { 
            m_deadtime_reject ++;
            setFilterPassed(false);
            return sc;
        }
        bool longdeadtime=false;
        if(m_pcounter!=0 && gltengine!=-1) // using ConfigSvc
            longdeadtime= tcf->trgEngine()->fourRangeReadout(gltengine);
        m_LivetimeSvc->tryToRegisterEvent(now,longdeadtime);
    } 
    
    m_triggered++;
    m_trig_counts[trigger_bits] +=1;

    unsigned short deltaevtime;
    if(m_lastTriggerTime!=0)
    {
        deltaevtime = m_LivetimeSvc->ticks(now-m_lastTriggerTime)<0xffff ? m_LivetimeSvc->ticks(now-m_lastTriggerTime) : 0xffff;
    }else{
        deltaevtime = 0xffff;
    }

    m_lastTriggerTime=now;
    if(m_firstevent)
    {
        m_firstTriggerTime = now;
        m_firstevent       = false;
    }
    
    if (m_applyDeadtime) 
        header->setLivetime(m_LivetimeSvc->livetime());
    else
        header->setLivetime(header->time()-m_firstTriggerTime);

    log << MSG::DEBUG 
        << "Processing run/event " << header->run() << "/" << header->event() << " trigger & mask = "
        << std::setbase(16) << (m_mask==0 ? trigger_bits : trigger_bits & m_mask)
        << endreq;

#if 0 // this is masked off and was never used
    // or in the gem trigger bits, either from hardware, or derived from trigger
    trigger_bits |= gemBits(trigger_bits) << enums::GEM_offset;
    trigger_bits |= engine << (2*enums::GEM_offset); // also the engine number (if set)
#endif

    // Set the Trigger Words
    unsigned int triggerWordTwo = gltengine    | gemengine << enums::ENGINE_offset;
    unsigned int triggerword    = trigger_bits | gemword   << enums::GEM_offset;

    if( static_cast<int>(header->trigger())==-1 
        || header->trigger()==0  // this seems to happen when reading back from incoming??
        )
    {
        // expect it to be zero if not set.
        header->setTrigger(triggerword);
        header->setTriggerWordTwo(triggerWordTwo);
    }else  if (header->trigger() != 0xbaadf00d && 
               (trigger_bits&0xff) != (header->trigger()&0xff) ) 
    {
        // trigger bits already set: reading digiRoot file.
        log << MSG::WARNING;
        if(log.isActive()) log.stream() << "Trigger bits read back do not agree with recalculation! " 
                                        << std::setbase(16) 
                                        << (trigger_bits&0xff )
                                        << " vs. " << (header->trigger()&0xff )
                                        << " event " << std::setbase(10) << header->event();
        log << endreq;
    }

    // fill GEM structure for MC
    if (isMc && gem == 0)
    {
        //make vetotilelist object
        LdfEvent::GemTileList vetoTileList;
        vetoTileList.clear();

        makeGemTileList(tilelist,vetoTileList);

        LdfEvent::Gem *gemTds = new LdfEvent::Gem();
        gemTds->initTrigger(tkrvector,roivector,
                            callovector,calhivector,
                            cnovector,gemword,
                            m_deadzone&0xff,vetoTileList); 
      
        LdfEvent::GemOnePpsTime ppsTimeTds(m_LivetimeSvc->ticks((unsigned int)now) & 0x1ffffff, ((unsigned int) now) & 0x7f);

        LdfEvent::GemDataCondArrivalTime gemCondTimeTds;
        gemCondTimeTds.init(0);// no conditions arrival time in Monte Carlo

        unsigned long long livetime=0;
        if(m_applyDeadtime)
            livetime = m_LivetimeSvc->ticks(m_LivetimeSvc->livetime());
        else
            livetime = m_LivetimeSvc->ticks(now-m_firstTriggerTime);

        gemTds->initSummary(livetime & 0xffffff,
                            m_prescaled & 0xffffff,
                            m_busy & 0xffffff,
                            gemCondTimeTds,
                            m_LivetimeSvc->ticks(now) & 0x1ffffff, 
                            ppsTimeTds, 
                            deltaevtime,
                            deltawotime);
      
        sc = eventSvc()->registerObject("/Event/Gem", gemTds);
        if( sc.isFailure() ) 
        {
            log << MSG::ERROR << "could not register /Event/Gem " << endreq;
            return sc;
        }

        // Update pointer to gem object
        gem = gemTds;
    }

    // Recover MetaEvent from TDS
    SmartDataPtr<LsfEvent::MetaEvent> metaTds(eventSvc(), "/Event/MetaEvent");
    if( metaTds==0) log<< MSG::DEBUG <<"No Meta event found."<<endreq;

    LsfEvent::MetaEvent* meta = metaTds;

    // Fill MetaEvent for MC
    if (isMc && !metaTds)
    {
        // Meta event GEM scalers     
        if (meta == 0)
        {
            meta=new LsfEvent::MetaEvent;
            sc = eventSvc()->registerObject("/Event/MetaEvent", meta);
            if (sc.isFailure()) 
            {
                log << MSG::INFO << "Failed to register MetaEvent" << endreq;
                return sc;
            }
        }

        unsigned long long elapsed = 0;
        if(m_applyDeadtime)
            elapsed=m_LivetimeSvc->ticks(m_LivetimeSvc->elapsed());
        else
            elapsed=m_LivetimeSvc->ticks(now-m_firstTriggerTime);

        lsfData::GemScalers gs(elapsed,
                               gem->liveTime(),
                               m_prescaled, 
                               m_busy,
                               header->event(),
                               m_deadzone);
        meta->setScalers(gs);
    }

    sc = handleMetaEvent(*meta, gemengine);    

    return sc;
}
      
//------------------------------------------------------------------------------
StatusCode TriggerAlg::finalize() 
{
    // purpose and method: make a summary
    StatusCode  sc = StatusCode::SUCCESS;
    static bool first(true);
    if( !first ) return sc;
    first = false;

    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "Totals triggered/ processed: " << m_triggered << "/" << m_total ; 

    if(log.isActive() )
    {
        bitSummary(log.stream(), "all events", m_counts);
        if( m_window_reject>0 )
            bitSummary(log.stream(), "events after window mask", m_window_counts);
        if( m_prescaled > 0 )
            bitSummary(log.stream(), "events after prescaling", m_prescaled_counts);
        if( m_triggered < m_total )
            bitSummary(log.stream(), "triggered events", m_trig_counts);
        if (m_window_reject>0)
        {
            log << "\n\t\tRejected " << m_window_reject << " events due to window mask";
        }
        if (m_prescaled>0)
        {
            log << "\n\t\tRejected " << m_prescaled << " events due to prescaling";
        }
        if( m_deadtime_reject>0)
        {
            log << "\n\t\tRejected " << m_deadtime_reject << " events due to deadtime";
        }
    }

    log << endreq;

    //TODO: format this nicely, as a 4x4 table

    return sc;
}

//------------------------------------------------------------------------------
unsigned int TriggerAlg::gemBits(unsigned int  trigger_bits)
{
    // set corresponding gem bits from glt word, should be a 1 to 1 translation

    return 
        ((trigger_bits & enums::b_ROI)    !=0 ? LdfEvent::Gem::ROI   : 0)
        |((trigger_bits & enums::b_Track) !=0 ? LdfEvent::Gem::TKR   : 0)
        |((trigger_bits & enums::b_LO_CAL)!=0 ? LdfEvent::Gem::CALLE : 0)
        |((trigger_bits & enums::b_HI_CAL)!=0 ? LdfEvent::Gem::CALHE : 0)
        |((trigger_bits & enums::b_ACDH)  !=0 ? LdfEvent::Gem::CNO   : 0) ;

}

//------------------------------------------------------------------------------
void TriggerAlg::makeGemTileList(const Event::TriggerInfo::TileList& tileList, LdfEvent::GemTileList& vetotilelist)
{
    MsgStream log(msgSvc(), name());
    vetotilelist.clear();

    // If the tile list is empty this is a problem...
    if (!tileList.empty())
    {
        // By definition, the map quantities must be there...
        unsigned short xzm = tileList.find("xzm")->second;
        unsigned short xzp = tileList.find("xzp")->second;
        unsigned short yzm = tileList.find("yzm")->second;
        unsigned short yzp = tileList.find("yzp")->second;
        unsigned int   xy  = tileList.find("xy")->second;
        unsigned short rbn = tileList.find("rbn")->second;
        unsigned short na  = tileList.find("na")->second;

        vetotilelist.init(xzm,xzp,yzm,yzp,xy,rbn,na);
    }
    else
        vetotilelist.init(0,0,0,0,0,0,0);

    return;
}
//------------------------------------------------------------------------------
void TriggerAlg::bitSummary(std::ostream& out, std::string label, const std::map<unsigned int,unsigned int>& table)
{
    // purpose and method: make a summary of the bit frequencies to the stream

    using namespace std;
#if 0
    int size= enums::number_of_trigger_bits;  // bits to expect
#else
    int size(5); // this is it: no more ACDL
#endif 
    static int col1=16; // width of first column
    out << endl << "             bit frequency: "<< label;
    out << endl << setw(col1) << "value"<< setw(6) << "count" ;
    int j, grand_total=0;
    for(j=size-1; j>=0; --j) out << setw(6) << m_bitNames[1<<j];
    out << endl << setw(col1) <<" "<< setw(6) << "------"; 
    for( j=0; j<size; ++j) out << setw(6) << "-----";
    vector<int>total(size);
    for( map<unsigned int,unsigned int>::const_iterator it = table.begin(); it != table.end(); ++it)
    {
        int i = (*it).first, n = (*it).second;
        grand_total += n;
        out << endl << setw(col1)<< i << setw(6)<< n ;
        for(j=size-1; j>=0; --j)
        {
            int m = ((i&(1<<j))!=0)? n :0;
            total[j] += m;
            out << setw(6) << m ;
        }
    }
    out << endl << setw(col1) <<" "<< setw(6) << "------"; 
    for( j=0; j<size; ++j) out << setw(6) << "-----";
    out << endl << setw(col1) << "tot:" << setw(6)<< grand_total;
    for(j=size-1; j>=0; --j) out << setw(6) << total[j];

    return;
}


enums::Lsf::LeakedPrescaler TriggerAlg::dgnPrescaler( unsigned triggerEngine ) 
{
    std::map<unsigned int, enums::Lsf::LeakedPrescaler>::iterator itr = m_dgnMap.find(triggerEngine);

    return itr == m_dgnMap.end() ? enums::Lsf::UNSUPPORTED : itr->second;
}


StatusCode TriggerAlg::handleMetaEvent( LsfEvent::MetaEvent& metaEvent, unsigned int triggerEngine ) 
{
    if ( m_configSvc == 0 ) return StatusCode::FAILURE;
  
    metaEvent.setMootKey( m_configSvc->getMootKey() );

    unsigned handlerMask(0);  
    unsigned fmxKey(0);

    lsfData::GammaHandler* gamma = const_cast<lsfData::GammaHandler*>(metaEvent.gammaFilter());
    unsigned gammaPS = LSF_INVALID_UINT;
    enums::Lsf::LeakedPrescaler gammaSamp = enums::Lsf::UNSUPPORTED;
    enums::Lsf::RsdState gammaState = enums::Lsf::INVALID;
    if ( gamma != 0 ) 
    {
        const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), 
                                                                     enums::Lsf::GAMMA, fmxKey );    
        handlerMask |= 1;
        if ( efc != 0 ) 
        {
            gammaSamp = gamma->prescaler() ;
            gammaState = gamma->state();
            gammaPS = efc->prescaleFactor(gammaState, gammaSamp);      
            if ( ! checkFmxKey(fmxKey,gamma->cfgKey(),metaEvent.datagram().mode(),enums::Lsf::GAMMA) ) 
            {
                return StatusCode::FAILURE;
            }
        }
        
        gamma->setPrescaleFactor(gammaPS);
    }
  
    lsfData::DgnHandler* dgn = const_cast<lsfData::DgnHandler*>(metaEvent.dgnFilter());
    unsigned dgnPS = LSF_INVALID_UINT;
    enums::Lsf::LeakedPrescaler dgnSamp = enums::Lsf::UNSUPPORTED;
    enums::Lsf::RsdState dgnState = enums::Lsf::INVALID;
    if ( dgn != 0 ) 
    {
        dgnSamp = dgnPrescaler(triggerEngine);
        const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), 
                                                                     enums::Lsf::DGN, fmxKey  );    
        handlerMask |= 2;
        if ( efc != 0 ) 
        {      
            dgnState = dgn->state();   
            dgnPS = efc->prescaleFactor(dgnState, dgnSamp);
            if ( ! checkFmxKey(fmxKey,dgn->cfgKey(),metaEvent.datagram().mode(),enums::Lsf::DGN) ) 
            {
                return StatusCode::FAILURE;
            }
        }
    
        dgn->setPrescaleFactor(dgnPS);
    }
    lsfData::MipHandler* mip = const_cast<lsfData::MipHandler*>(metaEvent.mipFilter());
    unsigned mipPS = LSF_INVALID_UINT;
    enums::Lsf::LeakedPrescaler mipSamp = enums::Lsf::UNSUPPORTED;
    enums::Lsf::RsdState mipState = enums::Lsf::INVALID;
    if ( mip != 0 ) 
    {
        const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), 
                                                                     enums::Lsf::MIP, fmxKey  );    
        handlerMask |= 4;
        if ( efc != 0 ) 
        {
            mipState = mip->state();
            mipSamp = mip->prescaler();
            mipPS = efc->prescaleFactor(mipState, mipSamp);      
            if ( ! checkFmxKey(fmxKey,mip->cfgKey(),metaEvent.datagram().mode(),enums::Lsf::MIP) ) 
            {
                return StatusCode::FAILURE;
            }
        }
    
        mip->setPrescaleFactor(mipPS);
    }
    unsigned hipPS = LSF_INVALID_UINT;
    enums::Lsf::LeakedPrescaler hipSamp = enums::Lsf::UNSUPPORTED;
    enums::Lsf::RsdState hipState = enums::Lsf::INVALID;
    lsfData::HipHandler* hip = const_cast<lsfData::HipHandler*>(metaEvent.hipFilter());
    if ( hip != 0 ) 
    {
        const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), 
                                                                     enums::Lsf::HIP, fmxKey  );    
        handlerMask |= 8;
        if ( efc != 0 ) 
        {
            hipState = hip->state();
            hipSamp = hip->prescaler();
            hipPS = efc->prescaleFactor(hipState, hipSamp);
            if ( ! checkFmxKey(fmxKey,hip->cfgKey(),metaEvent.datagram().mode(),enums::Lsf::HIP) ) 
            {
                return StatusCode::FAILURE;
            }
        }
    
        hip->setPrescaleFactor(hipPS);
    }

  /*
  std::cout << "Prescale Factors. " << triggerEngine << ' ' << handlerMask 
        << "  GAMMA: " << gammaState << ':' << gammaSamp << ' ' << gammaPS 
        << "  DGN  : " << dgnState << ':' << dgnSamp << ' ' << dgnPS 
        << "  MIP  : " << mipState << ':' << mipSamp << ' ' << mipPS 
        << "  HIP  : " << hipState << ':' << hipSamp << ' ' << hipPS << std::endl;
  */
    return StatusCode::SUCCESS;
}

bool TriggerAlg::checkFmxKey(unsigned mootKey, 
                             unsigned evtKey, 
                             enums::Lsf::Mode mode, 
                             enums::Lsf::HandlerId handler) const 
{
    // mootKey == 0 means we didn't get a cdm from MOOT, but from a file, don't really check
    if ( mootKey == 0 ) return true;
    // if the keys match we are ok
    if ( mootKey == evtKey ) return true;
  
    // houston, we have a problem...  

    // only warn once per mode X handler
    int handlerXMode = 1000*mode + handler;
    static std::set<int> warnedSet;
    if ( warnedSet.find(handlerXMode) == warnedSet.end() ) 
    {
        warnedSet.insert(handlerXMode);
        MsgStream log(msgSvc(), name());
        log << MSG::ERROR << "FMX key from moot (" << mootKey 
            << ") doesn't match key from data (" << evtKey << " for ";
    
        switch (handler) {
        case enums::Lsf::PASS_THRU:
            log << "PASS_THRU"; break;
        case enums::Lsf::GAMMA:
            log << "GAMMA"; break;
        case enums::Lsf::ASC:
            log << "ASC"; break;
        case enums::Lsf::MIP:
            log << "MIP"; break;
        case enums::Lsf::HIP:
            log << "HIP"; break;
        case enums::Lsf::DGN:
            log << "DGN"; break;
        default:
            log << "Some weird unknown"; break;
        }
    
        log << " handler in mode " << mode << "!!!" << endreq;
    }

    // return value depends on failOnFmxKeyMismatch job option
    return m_failOnFmxKeyMismatch.value() ? false : true;
}
