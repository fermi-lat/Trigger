/**
*  @file TriggerAlg.cxx
*  @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerAlg.cxx,v 1.87 2007/11/06 22:36:56 kocian Exp $
*/

//#include "ThrottleAlg.h"

#include "TriggerTables.h"

#include "Trigger/ILivetimeSvc.h"

#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "Trigger/ITrgConfigSvc.h"
#include "EnginePrescaleCounter.h"


#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "Event/TopLevel/DigiEvent.h"

#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/AcdDigi.h"
#include "Event/Digi/GltDigi.h"

#include "LdfEvent/DiagnosticData.h"
#include "LdfEvent/EventSummaryData.h"
#include "LdfEvent/Gem.h"
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

    int numInfoMessages = 10;
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
    //! determine calormiter trigger bits
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
    unsigned int m_prescaled;
    unsigned int m_busy;
    unsigned int m_deadzone;
    std::map<unsigned int,unsigned int> m_counts; //map of values for each bit pattern
    std::map<unsigned int,unsigned int> m_window_counts; //map of values for each bit pattern, window mask applied
    std::map<unsigned int,unsigned int> m_prescaled_counts; //map of values for each bit pattern, after prescaling
    std::map<unsigned int,unsigned int> m_trig_counts; //map of values for each bit pattern, triggered events

    std::map<idents::TowerId, int> m_tower_trigger_count;

    /// access to the Glast Detector Service to read in geometry constants from XML files
    IGlastDetSvc *m_glastDetSvc;

    ILivetimeSvc * m_LivetimeSvc;


    /// use to calculate cal trigger response from digis if cal trig response has not already
    /// been calculated
    ICalTrigTool *m_calTrigTool;

    /// use for the names of the bits
    std::map<int, std::string> m_bitNames;

    Trigger::TriggerTables* m_triggerTables;

    ITrgConfigSvc *m_trgConfigSvc;
    EnginePrescaleCounter* m_pcounter;
    bool m_printtables;
    bool m_isMc;
    TrgRoi *m_roi;

    unsigned int m_triggerMask;
    int m_infoCount;

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
, m_pcounter(0)
, m_isMc(0)
{
    declareProperty("mask"    ,  m_mask=0xffffffff); // trigger mask
    declareProperty("throttle",  m_throttle=false);  // if set, veto when throttle bit is on
    declareProperty("vetomask",  m_vetomask=1+2+4);  // if thottle it set, veto if trigger masked with these ...
    declareProperty("vetobits",  m_vetobits=1+2);    // equals these bits

    declareProperty("engine",    m_table = "");     // set to "default"  to use default engine table
    declareProperty("prescale",  m_prescale=std::vector<int>());        // vector of prescale factors
    declareProperty("applyPrescales",  m_applyPrescales=false);  // if using TrgConfigSvc, do we want to prescale events
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

    m_glastDetSvc = 0;
    sc = service("GlastDetSvc", m_glastDetSvc, true);
    if (sc.isSuccess() ) {
        sc = m_glastDetSvc->queryInterface(IID_IGlastDetSvc, (void**)&m_glastDetSvc);
    }

    if( sc.isFailure() ) {
        log << MSG::ERROR << "TriggerAlg failed to get the GlastDetSvc" << endreq;
        return sc;
    }
    sc = service("LivetimeSvc", m_LivetimeSvc);
    if( sc.isFailure() ) {
        log << MSG::ERROR << "failed to get the LivetimeSvc" << endreq;
        return sc;
    }

    sc = toolSvc()->retrieveTool("CalTrigTool", 
                                 m_calTrigTool,
                                 this);
    if (sc.isFailure() ) {
      log << MSG::ERROR << "  Unable to create CalTrigTool" << endreq;
      return sc;
    }

    if(! m_table.value().empty()){
        if (m_table.value()=="TrgConfigSvc"){
            log<<MSG::INFO<<"Using TrgConfigSvc"<<endreq;
            sc = service("TrgConfigSvc", m_trgConfigSvc, true);
            if( sc.isFailure() ) {
                log << MSG::ERROR << "failed to get the TrgConfigSvc" << endreq;
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

    m_triggerMask = (1<<enums::number_of_trigger_bits)-1;
    m_infoCount = 0;
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

    SmartDataPtr<Event::GltDigi> glt(eventSvc(),   EventModel::Digi::Event+"/GltDigi");
    if( glt==0 ) log << MSG::DEBUG << "No digi bits found" << endreq;

    SmartDataPtr<Event::DigiEvent> de(eventSvc(),   EventModel::Digi::Event);
    if( de==0 ) log << MSG::DEBUG << "No digi event found" << endreq;

    SmartDataPtr<LdfEvent::Gem> gem(eventSvc(), "/Event/Gem"); 
    if( gem==0 ) log << MSG::DEBUG << "No GEM found" << endreq;

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
    // calorimeter is either the new glt, or old cal
    if( glt!=0 ) { 
      trigger_bits |= calorimeter(glt,callovector,calhivector) ;
    }
    else {
      /// try to create new glt
      Event::GltDigi *newGlt(0);
      newGlt = m_calTrigTool->setupGltDigi();
      if (!newGlt)
        log << MSG::ERROR << "Failure to create new GltDigi in TDS." << endreq;
      
      /// empty return val from calTrigTOol
      CalUtil::CalArray<CalUtil::DiodeNum, bool> calTrigBits;
      fill(calTrigBits.begin(), calTrigBits.end(), false);
      
      /// calculate cal trigger response
      sc = m_calTrigTool->calcGlobalTrig(calTrigBits, newGlt);
      if (sc.isFailure()) {
        log << MSG::ERROR << "Failure to run cal trigger code" << endreq;
        return sc;
      }
      callovector=newGlt->getCALLETriggerVector();
      calhivector=newGlt->getCALHETriggerVector();
        
      trigger_bits |= (calTrigBits[CalUtil::LRG_DIODE] ? enums::b_LO_CAL:0) 
        |  (calTrigBits[CalUtil::SM_DIODE] ? enums::b_HI_CAL:0);
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
    if(m_pcounter){ //using TrgConfigSvc
      m_roi=const_cast<TrgRoi*>(m_trgConfigSvc->getTrgConfig()->roi());
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
      if (m_pcounter){ // using TrgConfigSvc
	windowOpen=trigger_bits & m_trgConfigSvc->getTrgConfig()->windowParams()->windowMask();
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
        const TrgConfig* tcf=m_trgConfigSvc->getTrgConfig();
        if(!m_printtables && m_trgConfigSvc->configChanged()){
            log<<MSG::INFO<<"Trigger configuration changed.";
            tcf->printContrigurator(log.stream());
            log<<endreq;
            m_pcounter->reset();
        }
        if (m_printtables){
            log << MSG::INFO << "Trigger tables: \n";
            tcf->printContrigurator(log.stream());
            log<<endreq;
            m_printtables=false;
            if(! m_applyPrescales ) {
                log << MSG::INFO << 
                    "TrgConfigSvc selected, but prescale factors and inhibits are not active:\n"
                    "\t\tset 'applyPrescales' to activate them. "
                    << endreq;
            }
        }
	bool passed=true;
	if (m_applyPrescales==true){
	  if (m_isMc || m_useGltWordForData==true) {// this is either MC or user wants glt word used for prescaling
	    passed=m_pcounter->decrementAndCheck(gemBits(trigger_bits),tcf);
	  } else {//this is data and we want to use the GEM summary word
	    assert(gem!=0);
	    passed=m_pcounter->decrementAndCheck(gem->conditionSummary(),tcf); 
	  }
	}
        if(!passed){
            setFilterPassed(false);
	    m_prescaled++;
            log << MSG::DEBUG << "Event did not trigger, according to engine selected by TrgConfigSvc" << endreq;
            return sc;
        }
        // Retrieve the engine numbers for both the GEM and GLT
	gemengine = tcf->lut()->engineNumber(gemword);
        gltengine= tcf->lut()->engineNumber(gemBits(trigger_bits));

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
      if(m_pcounter!=0 && gltengine!=-1) // using TrgConfigSvc
	longdeadtime=m_trgConfigSvc->getTrgConfig()->trgEngine()->fourRangeReadout(gltengine);
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
    if (m_applyDeadtime) 
      header->setLivetime(m_LivetimeSvc->livetime());
    else
      header->setLivetime(header->time());

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
    }else  if (h.trigger() != 0xbaadf00d 
        && ((trigger_bits&m_triggerMask) != (h.trigger()&m_triggerMask) )) {
            m_infoCount++;
            if(m_infoCount<=numInfoMessages) {
                // trigger bits already set: reading digiRoot file.
                log << MSG::INFO;
                if(log.isActive()) {
                    log.stream() << "Trigger bits read back do not agree with recalculation! " 
                        << std::setbase(16) <<trigger_bits << " vs. " << h.trigger();
                    if(m_infoCount==numInfoMessages) {
                        log << endreq;
                        log << "Message suppressed after " << std::setbase(10) 
                            << numInfoMessages << " events" ;
                    }
                }
            }
            log << endreq;
        }
    // fill GEM structure for MC
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
      unsigned livetime=0;
      if(m_applyDeadtime)
	livetime=m_LivetimeSvc->ticks(m_LivetimeSvc->livetime()) & 0xffffff;
      else
	livetime=m_LivetimeSvc->ticks(now)&0xffffff;
      gemTds->initSummary(livetime,m_prescaled&0xffffff,
      			  m_busy&0xffffff,gemCondTimeTds,
      			  m_LivetimeSvc->ticks(now) & 0x1ffffff, ppsTimeTds, 
      			  deltaevtime,deltawotime);
      
      sc = eventSvc()->registerObject("/Event/Gem", gemTds);
      if( sc.isFailure() ) {
        log << MSG::ERROR << "could not register /Event/Gem " << endreq;
        return sc;
      }
    }

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
    callovector=glt.getCALLETriggerVector();
    calhivector=glt.getCALHETriggerVector();
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
	  unsigned int pmt=digi.getCno(Event::AcdDigi::A) ? Event::AcdDigi::A : Event::AcdDigi::B;
	  idents::AcdId::convertToGarcGafe(id,pmt,garc,gafe);
	  cnovector|=1<<garc;
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


