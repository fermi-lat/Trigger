/**
*  @file TriggerAlg.cxx
*  @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerAlg.cxx,v 1.79 2007/09/11 21:15:12 kocian Exp $
*/

#include "ThrottleAlg.h"

#include "TriggerTables.h"

#include "Trigger/ILivetimeSvc.h"

#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "Trigger/ITrgConfigSvc.h"
#include "EnginePrescaleCounter.h"


#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"

#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/CalDigi.h"
#include "Event/Digi/AcdDigi.h"
#include "Event/Digi/GltDigi.h"

#include "LdfEvent/DiagnosticData.h"
#include "LdfEvent/Gem.h"
#include "enums/TriggerBits.h"

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"
#include "GaudiKernel/Property.h"

#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/StatusCode.h"


#include "CalXtalResponse/ICalTrigTool.h"

#include <map>
#include <vector>

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
    unsigned int  tracker(const Event::TkrDigiCol&  planes);
    //! determine calormiter trigger bits
    /// @return the bits
    unsigned int  calorimeter(const Event::GltDigi&  glt);

    //! calculate ACD trigger bits
    /// @return the bits
    unsigned int  anticoincidence(const Event::AcdDigiCol&  planes);
    void bitSummary(std::ostream& out, std::string label, const std::map<int,int>& table);

    /// set gem bits in trigger word, either from real condition summary, or from bits
    unsigned int gemBits(unsigned int  trigger_bits);

    unsigned int m_mask;
    int m_acd_hits;
    int m_log_hits;

    int m_event;
    BooleanProperty  m_throttle;
    BooleanProperty  m_applyPrescales;
    BooleanProperty  m_useGltWordForData;
    IntegerProperty m_vetobits;
    IntegerProperty m_vetomask;
    StringProperty m_table;
    IntegerArrayProperty m_prescale;

    double m_lastTriggerTime; //! time of last trigger, for calculated live time

    // for statistics
    int m_total;
    int m_triggered;
    int m_deadtime_reject;
    std::map<int,int> m_counts; //map of values for each bit pattern
    std::map<int,int> m_trig_counts; //map of values for each bit pattern, triggered events

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

};

//------------------------------------------------------------------------------
static const AlgFactory<TriggerAlg>  Factory;
const IAlgFactory& TriggerAlgFactory = Factory;
//------------------------------------------------------------------------------
/// 
TriggerAlg::TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator) 
: Algorithm(name, pSvcLocator), m_event(0)
, m_total(0)
, m_triggered(0), m_deadtime_reject(0)
, m_triggerTables(0)
, m_pcounter(0)
{
    declareProperty("mask"    ,  m_mask=0xffffffff); // trigger mask
    declareProperty("throttle",  m_throttle=false);  // if set, veto when throttle bit is on
    declareProperty("vetomask",  m_vetomask=1+2+4);  // if thottle it set, veto if trigger masked with these ...
    declareProperty("vetobits",  m_vetobits=1+2);    // equals these bits

    declareProperty("engine",    m_table = "");     // set to "default"  to use default engine table
    declareProperty("prescale",  m_prescale=std::vector<int>());        // vector of prescale factors
    declareProperty("applyPrescales",  m_applyPrescales=false);  // use TrgConfigSvc to termine trg engine but don't prescale
    declareProperty("useGltWordForData",  m_useGltWordForData=false);  // even if a GEM word exists use the Glt word

    for( int i=0; i<8; ++i) { 
        std::stringstream t; t<< "bit "<< i;
        m_bitNames[1<<i] = t.str();
    }
    m_bitNames[enums::b_LO_CAL] = "CALLO";
    m_bitNames[enums::b_HI_CAL] = "CALHI";
    m_bitNames[enums::b_ROI]    = "ROI";
    m_bitNames[enums::b_ACDH]   = "CNO";
    m_bitNames[enums::b_Track]  = "TKR";
    m_bitNames[enums::b_ACDL]   = "ACDL";
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

    // this tool needs to be shared by CalDigiAlg, XtalDigiTool & TriggerAlg, so I am
    // giving it global ownership
    sc = toolSvc()->retrieveTool("CalTrigTool", m_calTrigTool);
    if (sc.isFailure() ) {
        log << MSG::ERROR << "  Unable to create CalTrigTool" << endreq;
        return sc;
    }

    if(! m_table.value().empty()){
        if (m_table.value()=="TrgConfigSvc"){
            sc = service("TrgConfigSvc", m_trgConfigSvc, true);
            if( sc.isFailure() ) {
                log << MSG::ERROR << "failed to get the TrgConfigSvc" << endreq;
                return sc;
            }
            log<<MSG::INFO<<"Using TrgConfigSvc"<<endreq;
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

    SmartDataPtr<Event::CalDigiCol> cal(eventSvc(), EventModel::Digi::CalDigiCol);
    if( cal==0 ) log << MSG::DEBUG << "No cal digis found" << endreq;

    SmartDataPtr<Event::AcdDigiCol> acd(eventSvc(), EventModel::Digi::AcdDigiCol);
    if( acd==0 ) log << MSG::DEBUG << "No acd digis found" << endreq;

    SmartDataPtr<Event::GltDigi> glt(eventSvc(),   EventModel::Digi::Event+"/GltDigi");
    if( glt==0 ) log << MSG::DEBUG << "No digi bits found" << endreq;
    // set bits in the trigger word

    unsigned int trigger_bits = 
        (  tkr!=0? tracker(tkr) : 0 )
        | (acd!=0? anticoincidence(acd) : 0);


    // calorimter is either the new glt, or old cal
    if( glt!=0 ){ trigger_bits |= calorimeter(glt) ;}
    else if( cal!=0 ) {
        /// try to create new glt
        Event::GltDigi *newGlt(0);
        newGlt = m_calTrigTool->setupGltDigi(eventSvc());
        if (!newGlt)
            log << MSG::ERROR << "Failure to create new GltDigi in TDS." << endreq;

        CalUtil::CalArray<CalUtil::DiodeNum, bool> calTrigBits;
        calTrigBits.fill(false);
        sc = m_calTrigTool->calcGlobalTrig(cal, calTrigBits, newGlt);
        if (sc.isFailure()) {
            log << MSG::ERROR << "Failure to run cal trigger code" << endreq;
            return sc;
        }

        trigger_bits |= (calTrigBits[CalUtil::LRG_DIODE] ? enums::b_LO_CAL:0) 
            |  (calTrigBits[CalUtil::SM_DIODE] ? enums::b_HI_CAL:0);
    }


    SmartDataPtr<Event::EventHeader> header(eventSvc(), EventModel::EventHeader);

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
                trigger_bits |= roi; // hit both for now
            }
        }
    }
    else{
        log << MSG::ERROR << " could not find the event header" << endreq;
        return StatusCode::FAILURE;
    }
    // check for deadtime: set flag only if applying deadtime
    bool killedByDeadtime =  ! m_LivetimeSvc->isLive(header->time()); 
    if( killedByDeadtime) {
        m_deadtime_reject ++;
        setFilterPassed(false);
        return sc;
    }

    m_total++;
    m_counts[trigger_bits] +=1;
    int engine(16); // default engine number
    int gemengine(16),gltengine(16);

    // apply filter for subsequent processing.
    if( m_triggerTables!=0 ){
        engine = (*m_triggerTables)(trigger_bits & 31); // note only apply to low 5 bits for now
        log << MSG::DEBUG << "Engine is " << engine << endreq;

        if( engine<=0 ) {
            setFilterPassed(false);
            log << MSG::DEBUG << "Event did not trigger, according to engine selected by trigger table" << endreq;
            return sc;
        }
    } else if (m_pcounter!=0){
        const TrgConfig* tcf=m_trgConfigSvc->getTrgConfig();
        if (m_printtables){
            log << MSG::INFO << "Trigger tables: \n";
            tcf->printContrigurator(log.stream());
            log<<endreq;
            m_printtables=false;
        }
        if(m_trgConfigSvc->configChanged()){
            log<<MSG::INFO<<"Trigger configuration changed.";
            tcf->printContrigurator(log.stream());
            log<<endreq;
            m_pcounter->reset();
        }
	bool passed=true;
	if (m_applyPrescales==true){
	  if (gemBits(0)==0 || m_useGltWordForData==true) {// this is either MC or user wants glt word used for prescaling
	    passed=m_pcounter->decrementAndCheck(trigger_bits&31,tcf);
	  } else {//this is data and we want to use the GEM summary word
	    passed=m_pcounter->decrementAndCheck(gemBits(0),tcf); 
	  }
	}
        if(!passed){
            setFilterPassed(false);
            log << MSG::DEBUG << "Event did not trigger, according to engine selected by TrgConfigSvc" << endreq;
            return sc;
        }
        // Retrieve the engine numbers for both the GEM and GLT
        gemengine = tcf->lut()->engineNumber(gemBits(trigger_bits));
        gltengine= tcf->lut()->engineNumber(trigger_bits&31);

    }else {
        // apply mask conditions
        if( m_mask!=0 && ( trigger_bits & m_mask) == 0 
            || m_throttle && ( (trigger_bits & m_vetomask) == m_vetobits ) ) 
        {
            setFilterPassed( false );
            log << MSG::DEBUG << "Event did not trigger" << endreq;
            return sc;
        }else if( killedByDeadtime ){
            setFilterPassed( false );
            log << MSG::DEBUG << "Event killed by deadtime limit" << endreq;
            return sc;

        }
    }
    // passed trigger: continue processing

    m_triggered++;
    m_trig_counts[trigger_bits] +=1;

    double now = header->time();

    header->setLivetime(m_LivetimeSvc->livetime());

    Event::EventHeader& h = header;

    log << MSG::DEBUG ;
    if(log.isActive()) log.stream() << "Processing run/event " << h.run() << "/" << h.event() << " trigger & mask = "
        << std::setbase(16) << (m_mask==0 ? trigger_bits : trigger_bits & m_mask);
    log << endreq;

    // or in the gem trigger bits, either from hardware, or derived from trigger
    trigger_bits |= gemBits(trigger_bits) << enums::GEM_offset;
    trigger_bits |= engine << (2*enums::GEM_offset); // also the engine number (if set)
    
    unsigned int triggerWordTwo = 0;
    triggerWordTwo |= gltengine;
    triggerWordTwo |= gemengine << enums::ENGINE_offset; // also the GEM engine number (if set)

    if( static_cast<int>(h.trigger())==-1 
        || h.trigger()==0  // this seems to happen when reading back from incoming??
        )
    {
        // expect it to be zero if not set.
        h.setTrigger(trigger_bits);
        h.setTriggerWordTwo(triggerWordTwo);
    }else  if (h.trigger() != 0xbaadf00d && trigger_bits != h.trigger() ) {
        // trigger bits already set: reading digiRoot file.
        log << MSG::INFO;
        if(log.isActive()) log.stream() << "Trigger bits read back do not agree with recalculation! " 
            << std::setbase(16) <<trigger_bits << " vs. " << h.trigger();
        log << endreq;
    }

    return sc;
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::gemBits(unsigned int  trigger_bits)
{
    SmartDataPtr<LdfEvent::Gem> gem(eventSvc(), "/Event/Gem"); 

    if ( gem!=0) {
        return gem->conditionSummary();
    }

    // no GEM: set correspnding bits from present word
    // kudos to Heather for creating a typedef

    return 
        ((trigger_bits & enums::b_ROI)   !=0 ? LdfEvent::Gem::ROI   : 0)
        |((trigger_bits & enums::b_Track) !=0 ? LdfEvent::Gem::TKR   : 0)
        |((trigger_bits & enums::b_LO_CAL)!=0 ? LdfEvent::Gem::CALLE : 0)
        |((trigger_bits & enums::b_HI_CAL)!=0 ? LdfEvent::Gem::CALHE : 0)
        |((trigger_bits & enums::b_ACDH)  !=0 ? LdfEvent::Gem::CNO   : 0) ;

}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::tracker(const Event::TkrDigiCol&  planes)
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
unsigned int TriggerAlg::calorimeter(const Event::GltDigi& glt)
{
    // purpose and method: calculate CAL trigger bits from the list of bits
    bool local=glt.getCALLOtrigger(), 
        hical = glt.getCALHItrigger();
    return (local ? enums::b_LO_CAL:0) 
        |  (hical ? enums::b_HI_CAL:0);

}

//------------------------------------------------------------------------------
unsigned int TriggerAlg::anticoincidence(const Event::AcdDigiCol& tiles)
{
    // purpose and method: calculate ACD trigger bits from the list of hit tiles


    using namespace Event;
    // purpose: set ACD trigger bits
    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << tiles.size() << " tiles found with hits" << endreq;
    unsigned int ret=0;
    for( AcdDigiCol::const_iterator it = tiles.begin(); it !=tiles.end(); ++it){
        // check if hitMapBit is set (veto) which will correspond to 0.3 MIP.
        // 20060109 Agreed at Analysis Meeting that onboard threshold is 0.3 MIP
        const AcdDigi& digi = **it;
        if ( (digi.getHitMapBit(Event::AcdDigi::A)
            || digi.getHitMapBit(Event::AcdDigi::B))  ){
                ret |= enums::b_ACDL; 
            }
            // now trigger high if either PMT is above threshold
            if (   digi.getCno(Event::AcdDigi::A) 
                || digi.getCno(Event::AcdDigi::B) ) ret |= enums::b_ACDH;
    } 
    return ret;
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
        if( m_triggered < m_total )
            bitSummary(log.stream(), "triggered events", m_trig_counts);
       if( m_deadtime_reject>0){
            log << "\n\t\tRejected " << m_deadtime_reject << " events due to deadtime";
       }
    }


    log << endreq;

    //TODO: format this nicely, as a 4x4 table

    return sc;
}
//------------------------------------------------------------------------------
void TriggerAlg::bitSummary(std::ostream& out, std::string label, const std::map<int,int>& table){

    // purpose and method: make a summary of the bit frequencies to the stream

    using namespace std;
    int size= enums::number_of_trigger_bits;  // bits to expect
    static int col1=16; // width of first column
    out << endl << "             bit frequency: "<< label;
    out << endl << setw(col1) << "value"<< setw(6) << "count" ;
    int j, grand_total=0;
    for(j=size-1; j>=0; --j) out << setw(6) << m_bitNames[1<<j];
    out << endl << setw(col1) <<" "<< setw(6) << "------"; 
    for( j=0; j<size; ++j) out << setw(6) << "-----";
    vector<int>total(size);
    for( map<int,int>::const_iterator it = table.begin(); it != table.end(); ++it){
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


