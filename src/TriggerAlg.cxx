/**
*  @file TriggerAlg.cxx
*  @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerAlg.cxx,v 1.94 2008/06/16 20:26:29 echarles Exp $
*/

//#include "ThrottleAlg.h"

#include "TriggerTables.h"

#include "Trigger/ILivetimeSvc.h"

#include "ConfigSvc/IConfigSvc.h"
#include "EnginePrescaleCounter.h"
#include "configData/fsw/FswEfcSampler.h"

#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "Event/TopLevel/DigiEvent.h"

#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/AcdDigi.h"
#include "Event/Digi/CalDigi.h"
#include "Event/Digi/GltDigi.h"

#include "LdfEvent/DiagnosticData.h"
#include "LdfEvent/EventSummaryData.h"
#include "LdfEvent/Gem.h"
#include "LdfEvent/LsfMetaEvent.h"
#include "enums/TriggerBits.h"
#include "enums/GemState.h"


#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"
#include "GaudiKernel/Property.h"

#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/StatusCode.h"


#include "CalXtalResponse/ICalTrigTool.h"

#include <cassert>
#include <map>
#include <vector>
#include <algorithm>

namespace { // local definitions of convenient inline functions
    inline unsigned three_in_a_row(unsigned bits)
    {
        unsigned bitword = 0;
        for(int i=0; i<16; i++){
            if((bits&7)==7)  bitword |= 1 << i;
            bits >>= 1;
        }
        return bitword;
    }
    inline unsigned layer_bit(int layer){ return 1 << layer;}
}
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
    //! determine tracker trigger bits
    unsigned int  tracker(const Event::TkrDigiCol&  planes, unsigned short &tkrvector);
    //! determine calormiter trigger bits from existing GltDigi
    /// @return the bits
    unsigned int  calorimeter(const Event::GltDigi&  glt, unsigned short &callowvector, unsigned short &calhivector);

    //! calculate ACD trigger bits
    /// @return the bits
    unsigned int  anticoincidence(const Event::AcdDigiCol&  planes, unsigned short &cnovector, std::vector<unsigned int>& vetotilelist);
    void bitSummary(std::ostream& out, std::string label, const std::map<unsigned int,unsigned int>& table);
    /// input is a list of tiles, output is LdfEvent::GemTileList object
    void makeGemTileList(std::vector<unsigned int>&tilelist,LdfEvent::GemTileList& vetotilelist);
    /// set gem bits in trigger word, either from real condition summary, or from bits
    unsigned int gemBits(unsigned int  trigger_bits);
    void roiDefaultSetup();
    unsigned int throttle(unsigned int tkrvector, std::vector<unsigned int>& tilelist, unsigned short &roivector); 

    //! add the FSW prescale values and MOOT Key to the meta event
    StatusCode handleMetaEvent( LsfEvent::MetaEvent& metaEvent, unsigned int triggerEngine );

    //! map the engine to the DGN prescaler
    enums::Lsf::LeakedPrescaler dgnPrescaler(unsigned engine);

    /// create new GltDigi object & populate it w/ known
    /// fields
    static Event::GltDigi *createGltDigi(const Event::GltDigi::CalTriggerVec calLOVec,
                                         const Event::GltDigi::CalTriggerVec calHIVec);
    /// register GltDigi object in TDS.
  StatusCode registerGltDigi(Event::GltDigi *gltDigi);
    unsigned int m_mask;
    int m_acd_hits;
    int m_log_hits;

    int m_event;
    BooleanProperty  m_throttle;
    BooleanProperty  m_applyPrescales;
    BooleanProperty  m_applyWindowMask;
    BooleanProperty  m_applyDeadtime;
    BooleanProperty  m_useGltWordForData;
    IntegerProperty m_vetobits;
    IntegerProperty m_vetomask;
    StringProperty m_table;
    IntegerArrayProperty m_prescale;

    double m_lastTriggerTime; //! time of last trigger, for delta window time
    double m_lastWindowTime; //! time of last trigger window, for delta window open time

    // for statistics
    unsigned int m_total;
    unsigned int m_triggered;
    unsigned int m_deadtime_reject;
    unsigned int m_window_reject;
    unsigned long long m_prescaled;
    unsigned long long m_busy;
    unsigned long long m_deadzone;
    std::map<unsigned int,unsigned int> m_counts; //map of values for each bit pattern
    std::map<unsigned int,unsigned int> m_window_counts; //map of values for each bit pattern, window mask applied
    std::map<unsigned int,unsigned int> m_prescaled_counts; //map of values for each bit pattern, after prescaling
    std::map<unsigned int,unsigned int> m_trig_counts; //map of values for each bit pattern, triggered events

    std::map<idents::TowerId, int> m_tower_trigger_count;

    ILivetimeSvc * m_LivetimeSvc;


    /// use to calculate cal trigger response from digis if cal trig response has not already
    /// been calculated
    ICalTrigTool *m_calTrigTool;

    /// use for the names of the bits
    std::map<int, std::string> m_bitNames;

    Trigger::TriggerTables* m_triggerTables;

    IConfigSvc *m_configSvc;
    EnginePrescaleCounter* m_pcounter;
    bool m_printtables;
    bool m_isMc;
    TrgRoi *m_roi;
    bool m_firstevent;
    double m_firstTriggerTime;
    unsigned m_mootKey;

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
, m_triggered(0), m_deadtime_reject(0), m_window_reject(0), m_prescaled(0)
, m_busy(0), m_deadzone(0)
, m_triggerTables(0)
, m_configSvc(0)
, m_pcounter(0)
, m_isMc(0)
, m_roi(0)
, m_firstevent(true)
, m_firstTriggerTime(0)
, m_mootKey(0)
{
    declareProperty("mask"    ,  m_mask=0xffffffff); // trigger mask
    declareProperty("throttle",  m_throttle=false);  // if set, veto when throttle bit is on
    declareProperty("vetomask",  m_vetomask=1+2+4);  // if thottle it set, veto if trigger masked with these ...
    declareProperty("vetobits",  m_vetobits=1+2);    // equals these bits

    declareProperty("engine",    m_table = "ConfigSvc");     // set to "default"  to use default engine table
    declareProperty("prescale",  m_prescale=std::vector<int>());        // vector of prescale factors
    declareProperty("applyPrescales",  m_applyPrescales=false);  // if using ConfigSvc, do we want to prescale events
    declareProperty("useGltWordForData",  m_useGltWordForData=false);  // even if a GEM word exists use the Glt word
    declareProperty("applyWindowMask",  m_applyWindowMask=false);  // Do we want to use a window open mask?
    declareProperty("applyDeadtime",  m_applyDeadtime=false);  // Do we want to apply deadtime?

    for( int i=0; i<8; ++i) { 
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

    sc = toolSvc()->retrieveTool("CalTrigTool", 
                                 "CalTrigTool",
                                 m_calTrigTool,
                                 0); /// could be shared
    if (sc.isFailure() ) {
      log << MSG::ERROR << "  Unable to create CalTrigTool" << endreq;
      return sc;
    }
    
    if(! m_table.value().empty()){
        if (m_table.value()=="ConfigSvc"){
            log<<MSG::INFO<<"Using ConfigSvc for Trigger Config"<<endreq;
            sc = service("ConfigSvc", m_configSvc, true);
            if( sc.isFailure() ) {
                log << MSG::ERROR << "failed to get the ConfigSvc" << endreq;
                return sc;
            }
            m_pcounter=new EnginePrescaleCounter(m_prescale.value());
            m_printtables=true;
        }else{

            // selected trigger tables and engines for event selection

            m_triggerTables = new Trigger::TriggerTables(m_table.value(), m_prescale.value());

            log << MSG::INFO << "Trigger tables: \n";
            m_triggerTables->print(log.stream());
            log << endreq;
        }
    }
    if (!m_pcounter){ // set up default ROI config
      m_roi=new TrgRoi;
      roiDefaultSetup();
    }
    return sc;
}




//------------------------------------------------------------------------------
StatusCode TriggerAlg::execute() 
{

    // purpose: find digi collections in the TDS, pass them to functions to calculate the individual trigger bits

    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );

    SmartDataPtr<Event::TkrDigiCol> tkr(eventSvc(), EventModel::Digi::TkrDigiCol);
    if( tkr==0 ) log << MSG::DEBUG << "No tkr digis found" << endreq;

    SmartDataPtr<Event::AcdDigiCol> acd(eventSvc(), EventModel::Digi::AcdDigiCol);
    if( acd==0 ) log << MSG::DEBUG << "No acd digis found" << endreq;

    SmartDataPtr<Event::CalDigiCol> cal(eventSvc(), EventModel::Digi::CalDigiCol);
    if( cal==0 ) log << MSG::DEBUG << "No cal digis found" << endreq;

    SmartDataPtr<Event::GltDigi> glt(eventSvc(),   EventModel::Digi::Event+"/GltDigi");
    if( glt==0 ) log << MSG::DEBUG << "No digi bits found" << endreq;

    SmartDataPtr<Event::DigiEvent> de(eventSvc(),   EventModel::Digi::Event);
    if( de==0 ) log << MSG::DEBUG << "No digi event found" << endreq;

    SmartDataPtr<LdfEvent::Gem> gem(eventSvc(), "/Event/Gem"); 
    if( gem==0 ) log << MSG::DEBUG << "No GEM found" << endreq;

    SmartDataPtr<LsfEvent::MetaEvent> metaTds(eventSvc(), "/Event/MetaEvent");
    if( metaTds==0) log<< MSG::DEBUG <<"No Meta event found."<<endreq;

    // GET the MOOT key
    unsigned mKey = m_configSvc->getMootKey();
    bool configChanged = mKey != m_mootKey;
    m_mootKey = mKey;
    const TrgConfig* tcf(0);

    //    SmartDataPtr<LdfEvent::EventSummaryData> evsum(eventSvc(), "/Event/EventSummary"); 
        //if( evsum==0 ) log << MSG::DEBUG << "No event summary found" << endreq;

    if (de)m_isMc=de->fromMc();
    
    // set bits in the trigger word

    unsigned short tkrvector=0;
    unsigned short cnovector=0;
    unsigned short roivector=0;
    unsigned short callovector=0;
    unsigned short calhivector=0;
    LdfEvent::GemTileList vetoTileList;
    vetoTileList.clear();
    std::vector<unsigned int> tilelist;
    unsigned int trigger_bits = 
        (  tkr!=0? tracker(tkr,tkrvector) : 0 )
        | (acd!=0? anticoincidence(acd, cnovector,tilelist) : 0);
    // CAL CASE 1: Get bits from existing GltDigi class
    if( glt!=0 ) { 
      trigger_bits |= calorimeter(glt,callovector,calhivector) ;
    }

    // CAL CASE 2: Populate new GltDigi class with info from CalTrigTool
    else if(cal!=0){
      //-- Retrieve Trigger Vectors from CalTrigTool --//
      sc = m_calTrigTool->getCALTriggerVector(idents::CalXtalId::LARGE, callovector);
      if (sc.isFailure())
        return sc;
      sc = m_calTrigTool->getCALTriggerVector(idents::CalXtalId::SMALL, calhivector);
      if (sc.isFailure())
               return sc;
         
      // set cal lo trigger bit
      trigger_bits |= (callovector != 0) ? enums::b_LO_CAL : 0;
      // set cal hii trigger bit
      trigger_bits |= (calhivector != 0) ? enums::b_HI_CAL : 0;
 
      // create new GltDigi
      // object
      std::auto_ptr<Event::GltDigi> newGltDigi(createGltDigi(callovector, calhivector));
      if (!newGltDigi.get()) {
        log << MSG::ERROR << "Unable to create new GltDigi object" << endreq;
        return StatusCode::FAILURE;
      } 
     
      // register the new GltDigi object (we already know that
      // TDS GltDigi is currently blank.
      sc = registerGltDigi(newGltDigi.get());
      if (sc.isFailure()) {
        log << MSG::ERROR << "Unable to register GltDigi object" << endreq;
        return StatusCode::FAILURE;
      }

      // release ownership if object was sucessfully
      // registered.
      newGltDigi.release();
    }

    SmartDataPtr<Event::EventHeader> header(eventSvc(), EventModel::EventHeader);

    /*
    //call the throttle alg and add the bit to the trigger word
    ThrottleAlg Throttle;
    Throttle.setup();
    if (header){
        StatusCode temp_sc;
        static double vetoThresholdMeV(-99);
        if( vetoThresholdMeV<0){
            temp_sc = m_glastDetSvc->getNumericConstByName("acd.vetoThreshold", &vetoThresholdMeV);
        }
        if( tkr!=0 && acd !=0 ) {
            int roi = Throttle.calculate(tkr,acd, vetoThresholdMeV);
            if( roi!=0){
                trigger_bits |= enums::b_ROI;
            }
        }
    }
    else{
        log << MSG::ERROR << " could not find the event header" << endreq;
        return StatusCode::FAILURE;
    }
    */
    if(m_pcounter){ //using ConfigSvc
      tcf = m_configSvc->getTrgConfig();
      m_roi=const_cast<TrgRoi*>(tcf->roi());
      if ( m_roi == 0 ) {
	log << MSG::ERROR << "Failed to get ROI mapping from MOOT" << endreq;
	return StatusCode::FAILURE;
      } 
    }
    if (tkrvector!=0 && tilelist.size()!=0){
      trigger_bits |= throttle(tkrvector,tilelist,roivector);
    }

    m_total++;
    m_counts[trigger_bits] +=1;

    // Apply window mask. Only proceed if the window was opened 
    // or any trigger bit was set if window open mask was not available.
    if (m_applyWindowMask){
      unsigned windowOpen=0;
      if (m_pcounter){ // using ConfigSvc
	windowOpen=trigger_bits & tcf->windowParams()->windowMask();
      }else{
	windowOpen=trigger_bits&m_mask;
      }
      if ( !windowOpen){
	m_window_reject++;
	setFilterPassed(false);
	return sc;
      }
    }
    m_window_counts[trigger_bits] +=1;

    // record window open time
    double now = header->time();
    unsigned short deltawotime;
    if(m_lastWindowTime!=0){
      deltawotime=m_LivetimeSvc->ticks(now-m_lastWindowTime) < 0xffff ? m_LivetimeSvc->ticks(now-m_lastWindowTime): 0xffff;
    }else{
      deltawotime=0xffff;
    }
    m_lastWindowTime=now;
    

    int engine(16); // default engine number
    int gemengine(16),gltengine(16);
    unsigned int gemword= gem ? gem->conditionSummary() : gemBits(trigger_bits);

    // apply filter for subsequent processing.
    if( m_triggerTables!=0 ){
        engine = (*m_triggerTables)(trigger_bits & 31); // note only apply to low 5 bits for now
        log << MSG::DEBUG << "Engine is " << engine << endreq;

        if( engine<=0 ) {
            setFilterPassed(false);
	    m_prescaled++;
            log << MSG::DEBUG << "Event did not trigger, according to engine selected by trigger table" << endreq;
            return sc;
        }
    } else if (m_pcounter!=0){        
	if ( tcf == 0 ) {
	  log << MSG::ERROR << "Failed to get trigger config from ConfigSvc." << endreq;
	  return StatusCode::FAILURE;
	}
        if(!m_printtables && configChanged ){
            log<<MSG::INFO<<"Trigger configuration changed.";
            if ( tcf != 0 ) tcf->printContrigurator(log.stream());
            log<<endreq;
            m_pcounter->reset();
        }
        if (m_printtables){
            log << MSG::INFO << "Trigger tables: \n";
            if ( tcf != 0 ) tcf->printContrigurator(log.stream());
            log<<endreq;
            m_printtables=false;
            if(! m_applyPrescales ) {
                log << MSG::INFO << 
                    "ConfigSvc selected, but prescale factors and inhibits are not active:\n"
                    "\t\tset 'applyPrescales' to activate them. "
                    << endreq;
            }
        }
	bool passed=true;
	if (m_isMc || m_useGltWordForData==true) {// this is either MC or user wants glt word used for prescaling
	  passed=m_pcounter->decrementAndCheck(gemBits(trigger_bits),tcf);
	} else {//this is data and we want to use the GEM summary word
	  assert(gem!=0);
	  passed=m_pcounter->decrementAndCheck(gem->conditionSummary(),tcf); 
	}
	header->setPrescaleExpired(passed);
        if(!passed && m_applyPrescales==true){
            setFilterPassed(false);
	    m_prescaled++;
            log << MSG::DEBUG << "Event did not trigger, according to engine selected by ConfigSvc" << endreq;
            return sc;
        }
        // Retrieve the engine numbers for both the GEM and GLT
	gemengine = tcf->lut()->engineNumber(gemword);
	header->setGemPrescale(tcf->trgEngine()->prescale(gemengine));
        gltengine= tcf->lut()->engineNumber(gemBits(trigger_bits));
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
    
    if(m_applyDeadtime){
      enums::GemState gemstate=m_LivetimeSvc->checkState(now);
      if (gemstate==enums::DEADZONE)m_deadzone++;
      else if (gemstate==enums::BUSY)m_busy++;
      if( !m_LivetimeSvc->isLive(now)) { 
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
    if(m_lastTriggerTime!=0){
      deltaevtime=m_LivetimeSvc->ticks(now-m_lastTriggerTime)<0xffff ? m_LivetimeSvc->ticks(now-m_lastTriggerTime) : 0xffff;
    }else{
      deltaevtime=0xffff;
    }
    m_lastTriggerTime=now;
    if(m_firstevent){
      m_firstTriggerTime=now;
      m_firstevent=false;
    }
    if (m_applyDeadtime) 
      header->setLivetime(m_LivetimeSvc->livetime());
    else
      header->setLivetime(header->time()-m_firstTriggerTime);

    Event::EventHeader& h = header;

    log << MSG::DEBUG ;
    if(log.isActive()) log.stream() << "Processing run/event " << h.run() << "/" << h.event() << " trigger & mask = "
        << std::setbase(16) << (m_mask==0 ? trigger_bits : trigger_bits & m_mask);
    log << endreq;
#if 0 // this is masked off and was never used
    // or in the gem trigger bits, either from hardware, or derived from trigger
    trigger_bits |= gemBits(trigger_bits) << enums::GEM_offset;
    trigger_bits |= engine << (2*enums::GEM_offset); // also the engine number (if set)
#endif
    unsigned int triggerWordTwo = 0;
    triggerWordTwo |= gltengine;
    triggerWordTwo |= gemengine << enums::ENGINE_offset; // also the GEM engine number (if set)
    unsigned int triggerword=trigger_bits;
    triggerword |= gemword << enums::GEM_offset;

    if( static_cast<int>(h.trigger())==-1 
        || h.trigger()==0  // this seems to happen when reading back from incoming??
        )
    {
        // expect it to be zero if not set.
        h.setTrigger(triggerword);
        h.setTriggerWordTwo(triggerWordTwo);
    }else  if (h.trigger() != 0xbaadf00d && trigger_bits != h.trigger() ) {
        // trigger bits already set: reading digiRoot file.
        log << MSG::INFO;
        if(log.isActive()) log.stream() << "Trigger bits read back do not agree with recalculation! " 
            << std::setbase(16) <<trigger_bits << " vs. " << h.trigger();
        log << endreq;
    }
    // fill GEM structure for MC
    LsfEvent::MetaEvent *meta = metaTds;

    if (m_isMc && gem == 0){
      //make vetotilelist object
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
	livetime=m_LivetimeSvc->ticks(m_LivetimeSvc->livetime());
      else
	livetime=m_LivetimeSvc->ticks(now-m_firstTriggerTime);
      gemTds->initSummary(livetime & 0xffffff,m_prescaled&0xffffff,
      			  m_busy&0xffffff,gemCondTimeTds,
      			  m_LivetimeSvc->ticks(now) & 0x1ffffff, ppsTimeTds, 
      			  deltaevtime,deltawotime);
      
      sc = eventSvc()->registerObject("/Event/Gem", gemTds);
      if( sc.isFailure() ) {
        log << MSG::ERROR << "could not register /Event/Gem " << endreq;
        return sc;
      }
      // Meta event GEM scalers     
      if (meta==0){
	meta=new LsfEvent::MetaEvent;
	sc = eventSvc()->registerObject("/Event/MetaEvent", meta);
	if (sc.isFailure()) {
	  log << MSG::INFO << "Failed to register MetaEvent" << endreq;
	  return sc;
	}
      }
      unsigned long long elapsed=0;
      if(m_applyDeadtime)
	elapsed=m_LivetimeSvc->ticks(m_LivetimeSvc->elapsed());
      else
	elapsed=m_LivetimeSvc->ticks(now-m_firstTriggerTime);
      lsfData::GemScalers gs(elapsed,livetime,
			     m_prescaled, m_busy,
			     header->event(),m_deadzone);
      meta->setScalers(gs);
      
    }


    sc = handleMetaEvent(*meta,gemengine);    

    return sc;
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::gemBits(unsigned int  trigger_bits)
{
    // set corresponding gem bits from glt word, should be a 1 to 1 translation

    return 
        ((trigger_bits & enums::b_ROI)   !=0 ? LdfEvent::Gem::ROI   : 0)
        |((trigger_bits & enums::b_Track) !=0 ? LdfEvent::Gem::TKR   : 0)
        |((trigger_bits & enums::b_LO_CAL)!=0 ? LdfEvent::Gem::CALLE : 0)
        |((trigger_bits & enums::b_HI_CAL)!=0 ? LdfEvent::Gem::CALHE : 0)
        |((trigger_bits & enums::b_ACDH)  !=0 ? LdfEvent::Gem::CNO   : 0) ;

}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::tracker(const Event::TkrDigiCol&  planes, unsigned short &tkrvector)
{
    // purpose and method: determine if there is tracker trigger, any 3-in-a-row, 
    //and fill out the TDS class TriRowBits, with the complete 3-in-a-row information

    using namespace Event;

    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << planes.size() << " tracker planes found with hits" << endreq;

    // define a map to sort hit planes according to tower and plane axis
    typedef std::pair<idents::TowerId, idents::GlastAxis::axis> Key;
    typedef std::map<Key, unsigned int> Map;
    Map layer_bits;

    // this loop sorts the hits by setting appropriate bits in the tower-plane hit map
    for( Event::TkrDigiCol::const_iterator it = planes.begin(); it != planes.end(); ++it){
        const TkrDigi& t = **it;
        if( t.getNumHits()== 0) continue; // this can happen if there are dead strips 
        layer_bits[std::make_pair(t.getTower(), t.getView())] |= layer_bit(t.getBilayer());
    }


    bool tkr_trig_flag = false;
    tkrvector=0;
    // now look for a three in a row in x-y coincidence
    for( Map::iterator itr = layer_bits.begin(); itr !=layer_bits.end(); ++ itr){

        Key theKey=(*itr).first;
        idents::TowerId tower = theKey.first;
        if( theKey.second==idents::GlastAxis::X) {
            // found an x: and with corresponding Y (if exists)
            unsigned int
                xbits = (*itr).second,
                ybits = layer_bits[std::make_pair(tower, idents::GlastAxis::Y)];

            unsigned int bitword = three_in_a_row(xbits & ybits);
            if(bitword) {
	        // set bit in the tkr vector
	        tkrvector|=1<<tower.id();
                // OK: tag the tower for stats, but just one tower per event
                if(!tkr_trig_flag) m_tower_trigger_count[tower]++;
                //remember there was a 3-in-a-row but keep looking in other towers
                tkr_trig_flag=true;
            }

        }
    }

    //returns the digi base word, for consistency with the cal and acd.
    if(tkr_trig_flag) return enums::b_Track;
    else return 0;
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::calorimeter(const Event::GltDigi& glt,unsigned short &callovector, unsigned short &calhivector)
{
    // purpose and method: calculate CAL trigger bits from the list of bits
    bool local=glt.getCALLOtrigger(), 
        hical = glt.getCALHItrigger();
    callovector=glt.getCALLOTriggerVec();
    calhivector=glt.getCALHITriggerVec();
    return (local ? enums::b_LO_CAL:0) 
        |  (hical ? enums::b_HI_CAL:0);

}

//------------------------------------------------------------------------------
unsigned int TriggerAlg::anticoincidence(const Event::AcdDigiCol& tiles, unsigned short& cnovector, std::vector<unsigned int> &vetotilelist)
{
    // purpose and method: calculate ACD trigger bits from the list of hit tiles


    using namespace Event;
    // purpose: set ACD trigger bits
    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << tiles.size() << " tiles found with hits" << endreq;
    unsigned int ret=0;
    cnovector=0;
    for( AcdDigiCol::const_iterator it = tiles.begin(); it !=tiles.end(); ++it){
        // check if hitMapBit is set (veto) which will correspond to 0.3 MIP.
        // 20060109 Agreed at Analysis Meeting that onboard threshold is 0.3 MIP
        const AcdDigi& digi = **it;
	unsigned int id=digi.getId().id();
	if (id==899)id=1000; // NA tiles are 899 sometimes 
	
	// veto tile list
        if ( (digi.getHitMapBit(Event::AcdDigi::A) || digi.getHitMapBit(Event::AcdDigi::B))  ){
	  vetotilelist.push_back(id);
	  //ret |= enums::b_ACDL; 
	}

        // now trigger high if either PMT is above threshold
        if (   digi.getCno(Event::AcdDigi::A) || digi.getCno(Event::AcdDigi::B) ){
	  ret |= enums::b_ACDH;
	  // cno vector
	  unsigned int garc,gafe;
	  garc=gafe=0xff;
          if (digi.getCno(Event::AcdDigi::A)){
	    unsigned int pmt= Event::AcdDigi::A;
	    idents::AcdId::convertToGarcGafe(id,pmt,garc,gafe);
	    cnovector|=1<<garc;
          }
          if (digi.getCno(Event::AcdDigi::B)){
	    unsigned int pmt= Event::AcdDigi::B;
	    idents::AcdId::convertToGarcGafe(id,pmt,garc,gafe);
	    cnovector|=1<<garc;
          }
	}
    } 
    return ret;
}

//------------------------------------------------------------------------------
void TriggerAlg::makeGemTileList(std::vector<unsigned int>&tilelist,LdfEvent::GemTileList& vetotilelist){
    MsgStream log(msgSvc(), name());
    vetotilelist.clear();
    unsigned short xzm,xzp,yzm,yzp,rbn,na;
    xzm=xzp=yzm=yzp=rbn=na=0;
    unsigned int xy=0;
    for (unsigned int i=0;i<tilelist.size();i++){
      unsigned id=tilelist[i];
      unsigned int gemindex=idents::AcdId::gemIndexFromTile(id);
      if (gemindex<16) xzm|=1<<gemindex;
      else if (gemindex<32) xzp|=1<<(gemindex-16);
      else if (gemindex<48) yzm|=1<<(gemindex-32);
      else if (gemindex<64) yzp|=1<<(gemindex-48);
      else if (gemindex<89) xy|=1<<(gemindex-64);
      else if (gemindex<104) rbn|=1<<(gemindex-96);
      else if (gemindex<123) na|=1<<(gemindex-112);
      else log << MSG::ERROR<< "Bad tile gem index"<<endreq;
    } 
    vetotilelist.init(xzm,xzp,yzm,yzp,xy,rbn,na);
}
//------------------------------------------------------------------------------
void TriggerAlg::roiDefaultSetup(){
  m_roi->setRoiRegister( 0 , 0x30001 );
  m_roi->setRoiRegister( 1 , 0xc0006 );
  m_roi->setRoiRegister( 2 , 0x110008 );
  m_roi->setRoiRegister( 3 , 0x660033 );
  m_roi->setRoiRegister( 4 , 0x8800cc );
  m_roi->setRoiRegister( 5 , 0x3300110 );
  m_roi->setRoiRegister( 6 , 0xcc00660 );
  m_roi->setRoiRegister( 7 , 0x11000880 );
  m_roi->setRoiRegister( 8 , 0x66003300 );
  m_roi->setRoiRegister( 9 , 0x8800cc00 );
  m_roi->setRoiRegister( 10 , 0x30001000 );
  m_roi->setRoiRegister( 11 , 0xc0006000 );
  m_roi->setRoiRegister( 12 , 0x8000 );
  m_roi->setRoiRegister( 13 , 0x10000 );
  m_roi->setRoiRegister( 14 , 0x1100011 );
  m_roi->setRoiRegister( 15 , 0x10001100 );
  m_roi->setRoiRegister( 16 , 0x110001 );
  m_roi->setRoiRegister( 17 , 0x11000110 );
  m_roi->setRoiRegister( 18 , 0x1000 );
  m_roi->setRoiRegister( 22 , 0x10000 );
  m_roi->setRoiRegister( 23 , 0x60003 );
  m_roi->setRoiRegister( 24 , 0x8000c );
  m_roi->setRoiRegister( 25 , 0x30001 );
  m_roi->setRoiRegister( 26 , 0xc0006 );
  m_roi->setRoiRegister( 27 , 0x8 );
  m_roi->setRoiRegister( 31 , 0x80000 );
  m_roi->setRoiRegister( 32 , 0x8800088 );
  m_roi->setRoiRegister( 33 , 0x80008800 );
  m_roi->setRoiRegister( 34 , 0x880008 );
  m_roi->setRoiRegister( 35 , 0x88000880 );
  m_roi->setRoiRegister( 36 , 0x8000 );
  m_roi->setRoiRegister( 40 , 0x10000000 );
  m_roi->setRoiRegister( 41 , 0x60003000 );
  m_roi->setRoiRegister( 42 , 0x8000c000 );
  m_roi->setRoiRegister( 43 , 0x30001000 );
  m_roi->setRoiRegister( 44 , 0xc0006000 );
  m_roi->setRoiRegister( 45 , 0x8000 );
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::throttle(unsigned int tkrvector, std::vector<unsigned int>& tilelist, unsigned short &roivector){
  unsigned int triggered =0;
  roivector=0;
  for (unsigned int i=0;i<tilelist.size();i++){
    std::vector<unsigned long> rr=m_roi->roiFromName(tilelist[i]);
    for (unsigned int j=0;j<rr.size();j++){
      if (tkrvector & (1<<rr[j])){
	triggered|=enums::b_ROI;
      }
      roivector|=1<<rr[j];
    }
  }
  return triggered;
}
	  
//------------------------------------------------------------------------------
StatusCode TriggerAlg::finalize() {

    // purpose and method: make a summary

    StatusCode  sc = StatusCode::SUCCESS;
    static bool first(true);
    if( !first ) return sc;
    first = false;

    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "Totals triggered/ processed: " << m_triggered << "/" << m_total ; 

    if(log.isActive() ){
      bitSummary(log.stream(), "all events", m_counts);
      if( m_window_reject>0 )
	bitSummary(log.stream(), "events after window mask", m_window_counts);
      if( m_prescaled > 0 )
	bitSummary(log.stream(), "events after prescaling", m_prescaled_counts);
      if( m_triggered < m_total )
	bitSummary(log.stream(), "triggered events", m_trig_counts);
      if (m_window_reject>0){
	log << "\n\t\tRejected " << m_window_reject << " events due to window mask";
      }
      if (m_prescaled>0){
	log << "\n\t\tRejected " << m_prescaled << " events due to prescaling";
      }
      if( m_deadtime_reject>0){
	log << "\n\t\tRejected " << m_deadtime_reject << " events due to deadtime";
      }
    }


    log << endreq;

    //TODO: format this nicely, as a 4x4 table

    return sc;
}
//------------------------------------------------------------------------------
void TriggerAlg::bitSummary(std::ostream& out, std::string label, const std::map<unsigned int,unsigned int>& table){

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
    for( map<unsigned int,unsigned int>::const_iterator it = table.begin(); it != table.end(); ++it){
        int i = (*it).first, n = (*it).second;
        grand_total += n;
        out << endl << setw(col1)<< i << setw(6)<< n ;
        for(j=size-1; j>=0; --j){
            int m = ((i&(1<<j))!=0)? n :0;
            total[j] += m;
            out << setw(6) << m ;
        }
    }
    out << endl << setw(col1) <<" "<< setw(6) << "------"; 
    for( j=0; j<size; ++j) out << setw(6) << "-----";
    out << endl << setw(col1) << "tot:" << setw(6)<< grand_total;
    for(j=size-1; j>=0; --j) out << setw(6) << total[j];


}


enums::Lsf::LeakedPrescaler TriggerAlg::dgnPrescaler( unsigned triggerEngine ) {
  static std::map<unsigned int, enums::Lsf::LeakedPrescaler> dgnMap;
  if ( dgnMap.size() == 0 ) {
    dgnMap[0] = enums::Lsf::COND30;
    dgnMap[1] = enums::Lsf::COND29;
    dgnMap[2] = enums::Lsf::COND28;
    dgnMap[3] = enums::Lsf::COND27;
    dgnMap[4] = enums::Lsf::COND26;
    dgnMap[5] = enums::Lsf::COND25;
    dgnMap[6] = enums::Lsf::COND24;
    dgnMap[7] = enums::Lsf::COND23;
    dgnMap[8] = enums::Lsf::COND22;
    dgnMap[9] = enums::Lsf::COND21;
    dgnMap[10] = enums::Lsf::COND20;
    dgnMap[11] = enums::Lsf::COND19;    
  }
  std::map<unsigned int, enums::Lsf::LeakedPrescaler>::iterator itr = dgnMap.find(triggerEngine);
  return itr == dgnMap.end() ? enums::Lsf::UNSUPPORTED : itr->second;
}


StatusCode TriggerAlg::handleMetaEvent( LsfEvent::MetaEvent& metaEvent, unsigned int triggerEngine ) {

  if ( m_configSvc == 0 ) return StatusCode::FAILURE;
  
  metaEvent.setMootKey( m_configSvc->getMootKey() );

  unsigned handlerMask(0);  
  lsfData::GammaHandler* gamma = const_cast<lsfData::GammaHandler*>(metaEvent.gammaFilter());
  unsigned gammaPS = LSF_INVALID_UINT;
  enums::Lsf::LeakedPrescaler gammaSamp = enums::Lsf::UNSUPPORTED;
  enums::Lsf::RsdState gammaState = enums::Lsf::INVALID;
  if ( gamma != 0 ) {
    const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), enums::Lsf::GAMMA );    
    handlerMask |= 1;
    if ( efc != 0 ) {
      gammaSamp = gamma->prescaler() ;
      gammaState = gamma->state();
      gammaPS = efc->prescaleFactor(gammaState, gammaSamp);      
    }
    gamma->setPrescaleFactor(gammaPS);
  }
  
  lsfData::DgnHandler* dgn = const_cast<lsfData::DgnHandler*>(metaEvent.dgnFilter());
  unsigned dgnPS = LSF_INVALID_UINT;
  enums::Lsf::LeakedPrescaler dgnSamp = enums::Lsf::UNSUPPORTED;
  enums::Lsf::RsdState dgnState = enums::Lsf::INVALID;
  if ( dgn != 0 ) {
    dgnSamp = dgnPrescaler(triggerEngine);
    const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), enums::Lsf::DGN );    
    handlerMask |= 2;
    if ( efc != 0 ) {      
      dgnState = dgn->state();   
      dgnPS = efc->prescaleFactor(dgnState, dgnSamp);
    }
    dgn->setPrescaleFactor(dgnPS);
  }
  lsfData::MipHandler* mip = const_cast<lsfData::MipHandler*>(metaEvent.mipFilter());
  unsigned mipPS = LSF_INVALID_UINT;
  enums::Lsf::LeakedPrescaler mipSamp = enums::Lsf::UNSUPPORTED;
  enums::Lsf::RsdState mipState = enums::Lsf::INVALID;
  if ( mip != 0 ) {
    const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), enums::Lsf::MIP );    
    handlerMask |= 4;
    if ( efc != 0 ) {
      mipState = mip->state();
      mipSamp = mip->prescaler();
      mipPS = efc->prescaleFactor(mipState, mipSamp);      
    }
    mip->setPrescaleFactor(mipPS);
  }
  unsigned hipPS = LSF_INVALID_UINT;
  enums::Lsf::LeakedPrescaler hipSamp = enums::Lsf::UNSUPPORTED;
  enums::Lsf::RsdState hipState = enums::Lsf::INVALID;
  lsfData::HipHandler* hip = const_cast<lsfData::HipHandler*>(metaEvent.hipFilter());
  if ( hip != 0 ) {
    const FswEfcSampler* efc = m_configSvc->getFSWPrescalerInfo( metaEvent.datagram().mode(), enums::Lsf::HIP );    
    handlerMask |= 8;
    if ( efc != 0 ) {
      hipState = hip->state();
      hipSamp = hip->prescaler();
      hipPS = efc->prescaleFactor(hipState, hipSamp);
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


Event::GltDigi *TriggerAlg::createGltDigi(const Event::GltDigi::CalTriggerVec calLOVec,
                                          const Event::GltDigi::CalTriggerVec calHIVec) {
  Event::GltDigi *gltDigi(new Event::GltDigi());
  if (!gltDigi)
    return 0;

  gltDigi->setCALLOTriggerVec(calLOVec);
  gltDigi->setCALHITriggerVec(calHIVec);
   
  return gltDigi;
}
 
StatusCode TriggerAlg::registerGltDigi(Event::GltDigi *gltDigi) {
  static const std::string gltPath( EventModel::Digi::Event+"/GltDigi");
 
  return eventSvc()->registerObject(gltPath, gltDigi);
}
