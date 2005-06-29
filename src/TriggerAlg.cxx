/**
* @file TriggerAlg.cxx
* @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerAlg.cxx,v 1.45 2005/06/21 02:39:38 burnett Exp $
*/


#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"
#include "GaudiKernel/Property.h"

#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/StatusCode.h"

#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"

#include "CLHEP/Geometry/Point3D.h"
#include "CLHEP/Vector/LorentzVector.h"

#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"

#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/CalDigi.h"
#include "Event/Digi/AcdDigi.h"
#include "Event/Digi/GltDigi.h"

#include "LdfEvent/DiagnosticData.h"
#include "LdfEvent/Gem.h"
#include "enums/TriggerBits.h"


#include "ThrottleAlg.h"
#include "Trigger/TriRowBits.h"

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
    unsigned int  calorimeter(const Event::CalDigiCol&  planes);
    unsigned int  calorimeter(const Event::GltDigi&  glt);

    //! calculate ACD trigger bits
    /// @return the bits
    unsigned int  anticoincidence(const Event::AcdDigiCol&  planes);
    void bitSummary(std::ostream& out);

    StatusCode caltrigsetup();

    //// are we alive?
    bool alive(double current_time);

    /// set gem bits in trigger word, either from real condition summary, or from bits
    unsigned int gemBits(unsigned int  trigger_bits);
  
  void computeTrgReqTriRowBits(TriRowBitsTds::TriRowBits&);

    unsigned int m_mask;
    int m_acd_hits;
    int m_log_hits;
    bool m_local;
    bool m_hical;

    double m_pedestal;
    double m_maxAdc;
    double m_maxEnergy[4];
    double m_LOCALthreshold;
    double m_HICALthreshold;

    int m_event;
    DoubleProperty m_deadtime;

    double m_lastTriggerTime; //! time of last trigger, for calculated live time
    double m_liveTime; //! cumulative live time

    // for statistics
    int m_total;
    int m_triggered;
    int m_deadtimeLoss; //!< lost due to deadtime
    std::map<int,int> m_counts; //map of values for each bit pattern

    std::map<idents::TowerId, int> m_tower_trigger_count;

    /// access to the Glast Detector Service to read in geometry constants from XML files
    IGlastDetSvc *m_glastDetSvc;
};

//------------------------------------------------------------------------------
static const AlgFactory<TriggerAlg>  Factory;
const IAlgFactory& TriggerAlgFactory = Factory;
//------------------------------------------------------------------------------
/// 
TriggerAlg::TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator) :
Algorithm(name, pSvcLocator), m_event(0),
m_lastTriggerTime(0), m_liveTime(0), m_total(0), 
m_triggered(0), m_deadtimeLoss(0)
{
    declareProperty("mask"     ,  m_mask=0xffffffff); // trigger mask
    declareProperty("deadtime" ,  m_deadtime=0. );    // deadtime to apply to trigger, in sec.

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

        if( m_deadtime>0) log.stream() <<", applying deadtime of " << m_deadtime*1E6 << "u sec ";
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

    sc = caltrigsetup();

    return sc;
}

//------------------------------------------------------------------------------
StatusCode TriggerAlg::caltrigsetup()
{
    // purpose and method: extracting double constants for calorimeter trigger

    MsgStream   log( msgSvc(), name() );

    typedef std::map<double*,std::string> DPARAMAP;
    DPARAMAP dparam;
    dparam[m_maxEnergy]=std::string("cal.maxResponse0");
    dparam[m_maxEnergy+1]=std::string("cal.maxResponse1");
    dparam[m_maxEnergy+2]=std::string("cal.maxResponse2");
    dparam[m_maxEnergy+3]=std::string("cal.maxResponse3");
    dparam[&m_LOCALthreshold]=std::string("trigger.LOCALthreshold");
    dparam[&m_HICALthreshold]=std::string("trigger.HICALthreshold");
    dparam[&m_pedestal]=std::string("cal.pedestal");
    dparam[&m_maxAdc]=std::string("cal.maxAdcValue");

    for(DPARAMAP::iterator dit=dparam.begin(); dit!=dparam.end();dit++){
        if(!m_glastDetSvc->getNumericConstByName((*dit).second,(*dit).first)) {
            log << MSG::ERROR << " constant " <<(*dit).second << " not defined" << endreq;
            return StatusCode::FAILURE;
        } 
    }

    return StatusCode::SUCCESS;
}

bool TriggerAlg::alive(double current_time)
{ 
    // this is the case when reading back.
    if (m_deadtime<=0) return true;
    // ok we actually want to apply a deadtime: in this case *assume* time is increasing!

    return (current_time-m_lastTriggerTime >=m_deadtime);
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

    SmartDataPtr<Event::GltDigi> glt(eventSvc(), "Event/digi/GltDigi");
    if( glt==0 ) log << MSG::DEBUG << "No digi bits found" << endreq;
    // set bits in the trigger word

    unsigned int trigger_bits = 
        (  tkr!=0? tracker(tkr) : 0 )
        | (acd!=0? anticoincidence(acd) : 0);


    // calorimter is either the new glt, or old cal
    if( glt!=0 ){ trigger_bits |= calorimeter(glt) ;}
    else if( cal!=0) {trigger_bits |= calorimeter(cal); }


    SmartDataPtr<Event::EventHeader> header(eventSvc(), EventModel::EventHeader);

    //call the throttle alg and add the bit to the trigger word
    ThrottleAlg Throttle;
    Throttle.setup();
    if (header){
        StatusCode temp_sc;
        double s_vetoThresholdMeV;
        temp_sc = m_glastDetSvc->getNumericConstByName("acd.vetoThreshold", &s_vetoThresholdMeV);
        if( tkr!=0 && acd !=0 ) {
            trigger_bits |= Throttle.calculate(tkr,acd, s_vetoThresholdMeV);
        }
    }
    else{
        log << MSG::ERROR << " could not find the event header" << endreq;
        return StatusCode::FAILURE;
    }

    // check for deadtime: set flag only if applying deadtime
    bool killedByDeadtime =  ! alive(header->time()); 
    if( killedByDeadtime) {
        ++ m_deadtimeLoss; // keep track of these for output summary
    }else {

        m_total++;
        m_counts[trigger_bits] = m_counts[trigger_bits]+1;
    }

    // apply filter for subsequent processing.
    if( m_mask!=0 && ( trigger_bits & m_mask) == 0  ) {
        setFilterPassed( false );
        log << MSG::DEBUG << "Event did not trigger" << endreq;
    }else if( killedByDeadtime ){
        setFilterPassed( false );
        log << MSG::DEBUG << "Event killed by deadtime limit" << endreq;
    }else {
        m_triggered++;
        double now = header->time();
        // this is where we lose the deadtime.
        m_liveTime +=  now-m_lastTriggerTime - m_deadtime;

        header->setLivetime(m_liveTime);
        m_lastTriggerTime = now;

        Event::EventHeader& h = header;

        log << MSG::DEBUG ;
        if(log.isActive()) log.stream() << "Processing run/event " << h.run() << "/" << h.event() << " trigger & mask = "
            << std::setbase(16) << (m_mask==0 ? trigger_bits : trigger_bits & m_mask);
        log << endreq;

        // or in the gem trigger bits, either from hardware, or derived from trigger
        trigger_bits |= gemBits(trigger_bits) << 8;

        if( static_cast<int>(h.trigger())==-1 
                    || h.trigger()==0  // this seems to happen when reading back from incoming??
                    )
        {
            // expect it to be zero if not set.
            h.setTrigger(trigger_bits);
        }else  if (h.trigger() != 0xbaadf00d && trigger_bits != h.trigger() ) {
            // trigger bits already set: reading digiRoot file.
            log << MSG::INFO;
            if(log.isActive()) log.stream() << "Trigger bits read back do not agree with recalculation! " 
                << std::setbase(16) <<trigger_bits << " vs. " << h.trigger();
            log << endreq;
        }
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
        (( trigger_bits & enums::b_Track) !=0 ? LdfEvent::Gem::TKR   : 0)
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
        layer_bits[std::make_pair(t.getTower(), t.getView())] |= layer_bit(t.getBilayer());
    }

    //!Creating Object in TDS.
    //!Documentation of three_in_a_row_bits available in Trigger/TriRowBits.h
    TriRowBitsTds::TriRowBits *rowbits= new TriRowBitsTds::TriRowBits;
    eventSvc()->registerObject("/Event/TriRowBits", rowbits);

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

            //!Calculating the TriRowBits - 16 possible 3-in-a-row signals for 18 layers
            unsigned int bitword=three_in_a_row(xbits & ybits);
	    rowbits->setDigiTriRowBits(tower.id(), bitword);

	    if(bitword) {
	        // OK: tag the tower for stats, but just one tower per event
                if(!tkr_trig_flag) m_tower_trigger_count[tower]++;
		//remember there was a 3-in-a-row but keep looking in other towers
		tkr_trig_flag=true;
             }

        }
    }

    //Now we compute the 3 in a row combinations based on the trigger requests
    computeTrgReqTriRowBits(*rowbits);
    
    //    SmartDataPtr<TriRowBitsTds::TriRowBits> newrowbits(eventSvc(), "/Event/TriRowBits");

    
    log << MSG::DEBUG;
    if(log.isActive()) log.stream() << *rowbits;
    log <<endreq;

    //returns the digi base word, for consistency with the cal and acd.
       if(tkr_trig_flag) return enums::b_Track;
            else return 0;
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::calorimeter(const Event::GltDigi& glt)
{
    // purpose and method: calculate CAL trigger bits from the list of bits
    bool local=false, hical=false;
    const std::vector<bool>& cal_lo = glt.getCAL_LO();
    const std::vector<bool>& cal_hi = glt.getCAL_HI();
    for( std::vector<bool>::const_iterator bit = cal_lo.begin(); 
        bit!=cal_lo.end(); ++ bit){
        if( *bit) { local = true; break;}
    }
    for( std::vector<bool>::const_iterator bit = cal_hi.begin(); 
        bit!=cal_hi.end(); ++ bit){
        if( *bit) { hical = true; break;}
    }
    return (local ? enums::b_LO_CAL:0) 
        |  (hical ? enums::b_HI_CAL:0);

}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::calorimeter(const Event::CalDigiCol& calDigi)
{
    // purpose and method: calculate CAL trigger bits from the list of digis


    using namespace Event;
    // purpose: set calorimeter trigger bits
    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << calDigi.size() << " crystals found with hits" << endreq;

    m_local = false;
    m_hical = false;

    for( CalDigiCol::const_iterator it = calDigi.begin(); it != calDigi.end(); ++it ){

        idents::CalXtalId xtalId = (*it)->getPackedId();
#if 0 // for debug only: not used, so comment out to avoid warnings
        int lyr = xtalId.getLayer();
        int towid = xtalId.getTower();
        int icol  = xtalId.getColumn();
#endif

        const Event::CalDigi::CalXtalReadoutCol& readoutCol = (*it)->getReadoutCol();

        Event::CalDigi::CalXtalReadoutCol::const_iterator itr = readoutCol.begin();

        int rangeP = itr->getRange(idents::CalXtalId::POS); 
        int rangeM = itr->getRange(idents::CalXtalId::NEG); 

        double adcP = itr->getAdc(idents::CalXtalId::POS);	
        double adcM = itr->getAdc(idents::CalXtalId::NEG);	

        double eneP = m_maxEnergy[rangeP]*(adcP-m_pedestal)/(m_maxAdc-m_pedestal);
        double eneM = m_maxEnergy[rangeM]*(adcM-m_pedestal)/(m_maxAdc-m_pedestal);


        if(eneP> m_LOCALthreshold || eneM > m_LOCALthreshold) m_local = true;
        if(eneP> m_HICALthreshold || eneM > m_HICALthreshold) m_hical = true; 

    }


    return (m_local ? enums::b_LO_CAL:0) 
        | (m_hical ? enums::b_HI_CAL:0);

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
        // if it is here, assume it has a bit.
        ret |= enums::b_ACDL; 
        // now trigger high if either PMT is above threshold
        const AcdDigi& digi = **it;
        if (   digi.getHighDiscrim(Event::AcdDigi::A) 
            || digi.getHighDiscrim(Event::AcdDigi::B) ) ret |= enums::b_ACDH;
    } 
    return ret;
}


//------------------------------------------------------------------------------
StatusCode TriggerAlg::finalize() {

    // purpose and method: make a summary

    using namespace std;
    StatusCode  sc = StatusCode::SUCCESS;

    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "Totals triggered/ processed: " << m_triggered << "/" << m_total ; 

    if( m_deadtime>0 )  log << "\n\t\tLost to deadtime: " << m_deadtimeLoss ;

    if(log.isActive() ) bitSummary(log.stream());

    log << endreq;

    //TODO: format this nicely, as a 4x4 table
    log << MSG::INFO ;
    if(log.isActive() ){
        log.stream() << "Tower trigger summary\n" << setw(30) << "Tower    count " << std::endl;;
        for( std::map<idents::TowerId, int>::const_iterator it = m_tower_trigger_count.begin();
            it != m_tower_trigger_count.end(); ++ it ){
                log.stream() << setw(30) << (*it).first.id() << setw(10) << (*it).second << std::endl;
            }
    }
    log << endreq;
    return sc;
}
//------------------------------------------------------------------------------
void TriggerAlg::bitSummary(std::ostream& out){

    // purpose and method: make a summary of the bit frequencies to the stream

    using namespace std;
    int size= enums::number_of_trigger_bits;  // bits to expect
    static int col1=16; // width of first column
    out << endl << "             bit frequency table";
    out << endl << setw(col1) << "value"<< setw(6) << "count" ;
    int j, grand_total=0;
    for(j=0; j<size; ++j) out << setw(6) << (1<<j);
    out << endl << setw(col1) <<" "<< setw(6) << "------"; 
    for( j=0; j<size; ++j) out << setw(6) << "-----";
    vector<int>total(size);
    for( map<int,int>::const_iterator it = m_counts.begin(); it != m_counts.end(); ++it){
        int i = (*it).first, n = (*it).second;
        grand_total += n;
        out << endl << setw(col1)<< i << setw(6)<< n ;
        for(j=0; j<size; ++j){
            int m = ((i&(1<<j))!=0)? n :0;
            total[j] += m;
            out << setw(6) << m ;
        }
    }
    out << endl << setw(col1) <<" "<< setw(6) << "------"; 
    for( j=0; j<size; ++j) out << setw(6) << "-----";
    out << endl << setw(col1) << "tot:" << setw(6)<< grand_total;
    for( j=0; j<size; ++j) out << setw(6) << total[j];


}


//------------------------------------------------------------------------------

void TriggerAlg::computeTrgReqTriRowBits(TriRowBitsTds::TriRowBits& rowbits)
{
  // Retrieve the Diagnostic data for this event
  // note: here we would also have access to the CAL diagnostic data.
  SmartDataPtr<LdfEvent::DiagnosticData> diagTds(eventSvc(), "/Event/Diagnostic");
  //  SmartDataPtr<TriRowBitsTds::TriRowBits> rowbits(eventSvc(), "/Event/TriRowBits");
  static const unsigned int NUM_TWR = 16; //this should come from the geometry.

  //handle the case where there is no diagnostics in the TDS:
  if(!diagTds) 
    {
      return;
    }
  typedef std::pair<unsigned int, unsigned int> Key;
  typedef std::map<Key, unsigned int> Map;
  Map trgReq_bits;
  
  int numTkrDiag = diagTds->getNumTkrDiagnostic();

  for (int ind = 0; ind < numTkrDiag; ind++) 
    {
      LdfEvent::TkrDiagnosticData tkrDiagTds = diagTds->getTkrDiagnosticByIndex(ind);
      unsigned int tower = tkrDiagTds.tower();
      unsigned int gtcc  = tkrDiagTds.gtcc();
      if(gtcc==2||gtcc==3) //X view even
	{
	  trgReq_bits[std::make_pair(tower,0)] |= tkrDiagTds.dataWord();
	}
      if(gtcc==0||gtcc==1) //Y view even
	{
	  trgReq_bits[std::make_pair(tower,1)] |= tkrDiagTds.dataWord();
	}
      if(gtcc==6||gtcc==7) //X view odd
	{
	  trgReq_bits[std::make_pair(tower,2)] |= tkrDiagTds.dataWord();
	}
      if(gtcc==4||gtcc==5) //Y view odd
	{
	  trgReq_bits[std::make_pair(tower,3)] |= tkrDiagTds.dataWord();
	}
    }
  
  unsigned int trgReq_bits_evenbilayers[NUM_TWR];
  unsigned int trgReq_bits_oddbilayers[NUM_TWR];
  for(unsigned int twr=0;twr<NUM_TWR;twr++)
    {
      unsigned bitword=0;
      trgReq_bits_evenbilayers[twr]=0;
      trgReq_bits_oddbilayers[twr]=0;
      trgReq_bits_evenbilayers[twr] = trgReq_bits[std::make_pair(twr,0)] & trgReq_bits[std::make_pair(twr,1)];
      trgReq_bits_oddbilayers[twr] = trgReq_bits[std::make_pair(twr,2)] & trgReq_bits[std::make_pair(twr,3)];
      //and now compute the 3 in a row combinations:
      int comb = 0;
      while(comb<16)
	{  
	  bitword |=(((trgReq_bits_evenbilayers[twr]&3)==3) & ((trgReq_bits_oddbilayers[twr]&1)==1))<<comb; 
	  bitword |=(((trgReq_bits_oddbilayers[twr]&3)==3) & ((trgReq_bits_evenbilayers[twr]&2)==2))<<comb+1; 
	  
	  trgReq_bits_evenbilayers[twr] >>= 1;
	  trgReq_bits_oddbilayers[twr] >>= 1;
	  comb = comb+2;
	}
      rowbits.setTrgReqTriRowBits(twr, bitword);
    }
  
}
