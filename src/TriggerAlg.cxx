/** 
* @file TriggerAlg.cxx
* @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerAlg.cxx,v 1.8 2002/05/25 19:59:32 burnett Exp $
*/

// Include files

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"


#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/StatusCode.h"

#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"

#include "CLHEP/Geometry/Point3D.h"
#include "CLHEP/Vector/LorentzVector.h"

#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "Event/TopLevel/Event.h"

#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/CalDigi.h"
#include "Event/Digi/AcdDigi.h"

#include <map>
namespace {
    inline bool three_in_a_row(unsigned bits)
    {
        while( bits ) {
            if( (bits & 7) ==7) return true;
            bits >>= 1;
        }
        return false;
    }
    inline unsigned layer_bit(int layer){ return 1 << layer;}
}

//------------------------------------------------------------------------------
/*! \class TriggerAlg
\brief  alg that sets trigger information

*/

class TriggerAlg : public Algorithm {
    
public:
    enum  {
        // definition of  trigger bits
        
        b_ACDL =     1,  //  set if cover or side veto, low threshold
            b_ACDH =     2,   //  cover or side veto, high threshold
            b_Track=     4,   //  3 consecutive x-y layers hit
            b_LO_CAL=    8,  //  single log above low threshold
            b_HI_CAL=   16,   //  3 cal layers in a row above high threshold

         number_of_trigger_bits = 5
            
    };
    
    //! Constructor of this form must be provided
    TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator); 
    
    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();
    
private:
    unsigned int  tracker(const Event::TkrDigiCol&  planes);
    unsigned int  calorimeter(const Event::CalDigiCol&  planes);
    unsigned int  anticoincidence(const Event::AcdDigiCol&  planes);
    void bitSummary(std::ostream& out);

    StatusCode caltrigsetup();
    
    unsigned int m_mask;
    int m_acd_hits;
    int m_log_hits;
    bool m_local;
    bool m_hical;
    unsigned m_hical_bits[16];
    
    double m_pedestal;
    double m_maxAdc;
    double m_maxEnergy[4];
    double m_LOCALthreshold;
    double m_HICALthreshold;
    
    int m_run;
    int m_event;

    // for statistics
    int m_total;
    int m_triggered;
    std::map<int,int> m_counts; //map of values for each bit pattern

    
    
};

//------------------------------------------------------------------------------
static const AlgFactory<TriggerAlg>  Factory;
const IAlgFactory& TriggerAlgFactory = Factory;
//------------------------------------------------------------------------------
/// 
TriggerAlg::TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator) :
Algorithm(name, pSvcLocator), m_event(0) ,m_total(0), m_triggered(0)
{
    declareProperty("mask"     ,  m_mask=0xffffffff); // trigger mask
    declareProperty("run"      ,  m_run =0 );  
   
}

//------------------------------------------------------------------------------
/*! 
*/
StatusCode TriggerAlg::initialize() {
    
    StatusCode sc = StatusCode::SUCCESS;
    
    MsgStream log(msgSvc(), name());
    
    // Use the Job options service to set the Algorithm's parameters
    setProperties();
    
    log << MSG::INFO <<"Applying mask: " <<  std::setbase(16) <<m_mask <<  endreq;
    
    
    sc = caltrigsetup();
    log << MSG::INFO << "initializing run number " << m_run  << endreq;

    return sc;
}

//------------------------------------------------------------------------------
StatusCode TriggerAlg::caltrigsetup()
{
    // extracting double constants for calorimeter trigger
    IGlastDetSvc* detSvc;
    StatusCode sc = service("GlastDetSvc", detSvc);
    if( sc.isFailure() ) return sc;
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
        if(!detSvc->getNumericConstByName((*dit).second,(*dit).first)) {
            log << MSG::ERROR << " constant " <<(*dit).second << " not defined" << endreq;
            return StatusCode::FAILURE;
        } 
    }
    
    for (int r=0; r<4;r++) m_maxEnergy[r] *= 1000.; // from GeV to MeV
    
    m_LOCALthreshold *= 1000.;
    m_HICALthreshold *= 1000.;
    return StatusCode::SUCCESS;
}


//------------------------------------------------------------------------------
StatusCode TriggerAlg::execute() {
    
    
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );
    
    SmartDataPtr<Event::TkrDigiCol> tkr(eventSvc(), EventModel::Digi::TkrDigiCol);
    if( tkr==0 ) log << MSG::DEBUG << "No tkr digis found" << endreq;
    
    SmartDataPtr<Event::CalDigiCol> cal(eventSvc(), EventModel::Digi::CalDigiCol);
    if( cal==0 ) log << MSG::DEBUG << "No cal digis found" << endreq;
    
    SmartDataPtr<Event::AcdDigiCol> acd(eventSvc(), EventModel::Digi::AcdDigiCol);
    if( acd==0 ) log << MSG::DEBUG << "No acd digis found" << endreq;
    
    // set bits in the trigger word
    
    unsigned int trigger_bits = 
        (tkr? tracker(tkr) : 0 )
        | (cal? calorimeter(cal) : 0 )
        | (acd? anticoincidence(acd) : 0);
    
    
    m_total++;
    m_counts[trigger_bits] = m_counts[trigger_bits]+1;

    // apply filter for subsequent processing.
    if( m_mask!=0 && ( trigger_bits & m_mask) == 0) {
        setFilterPassed( false );
        log << MSG::DEBUG << "Event did not trigger" << endreq;
    }else {
        m_triggered++;

        
        SmartDataPtr<Event::EventHeader> header(eventSvc(), EventModel::EventHeader);
        
       
        if(header) {
            Event::EventHeader& h = header;
            h.setRun(m_run);
            h.setEvent(++m_event);
            h.setTrigger(trigger_bits);
            
            log << MSG::INFO << "Begin event " << m_event  << " trigger bits "  
                << std::setbase(16) << (m_mask==0?trigger_bits:trigger_bits& m_mask) << endreq;
            
        } else { 
            log << MSG::WARNING << " could not find the event header" << endreq;
            return StatusCode::FAILURE;
        }

    }
    
    return sc;
}

//------------------------------------------------------------------------------
unsigned int TriggerAlg::tracker(const Event::TkrDigiCol&  planes){
    using namespace Event;
    // purpose: determine if there is tracker trigger, 3-in-a-row
    
    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << planes.size() << " tracker planes found with hits" << endreq;
    
    // define a map to sort hit planes according to tower and plane axis
    typedef std::pair<idents::TowerId, idents::GlastAxis::axis> Key;
    typedef std::map<Key, unsigned int> Map;
    Map layer_bits;
    
    for( Event::TkrDigiCol::const_iterator it = planes.begin(); it != planes.end(); ++it){
        const TkrDigi& t = **it;
        layer_bits[std::make_pair(t.getTower(), t.getView())] |= layer_bit(t.getBilayer());
    }
    
    // now look for a three in a row
    for( Map::iterator itr = layer_bits.begin(); itr !=layer_bits.end(); ++ itr){
        if( three_in_a_row((*itr).second) ) return b_Track;
    }
    return 0;
    
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::calorimeter(const Event::CalDigiCol& calDigi){
    using namespace Event;
    // purpose: set calorimeter trigger bits
    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << calDigi.size() << " crystals found with hits" << endreq;
    
    m_local = false;
    m_hical = false;
    for( int j=0; j<16; ++j) m_hical_bits[j] = 0;
    
    for( CalDigiCol::const_iterator it = calDigi.begin(); it != calDigi.end(); ++it ){
        
        idents::CalXtalId xtalId = (*it)->getPackedId();
        int lyr = xtalId.getLayer();
        int towid = xtalId.getTower();
        int icol  = xtalId.getColumn();
        
        
        const Event::CalDigi::CalXtalReadoutCol& readoutCol = (*it)->getReadoutCol();
        
        Event::CalDigi::CalXtalReadoutCol::const_iterator itr = readoutCol.begin();
        
        int rangeP = itr->getRange(idents::CalXtalId::POS); 
        int rangeM = itr->getRange(idents::CalXtalId::NEG); 
        
        double adcP = itr->getAdc(idents::CalXtalId::POS);	
        double adcM = itr->getAdc(idents::CalXtalId::NEG);	
        
        double eneP = m_maxEnergy[rangeP]*(adcP-m_pedestal)/(m_maxAdc-m_pedestal);
        double eneM = m_maxEnergy[rangeM]*(adcM-m_pedestal)/(m_maxAdc-m_pedestal);
        
        
        if(eneP> m_LOCALthreshold || eneM > m_LOCALthreshold) m_local = true;
        if(eneP> m_HICALthreshold || eneM > m_HICALthreshold) m_hical_bits[towid] |= layer_bit(lyr); 
        
    }
    
    for(j=0; j<16; ++j) m_hical = m_hical || three_in_a_row(m_hical_bits[j]);
    
    
    return (m_local ? b_LO_CAL:0) | (m_hical ? b_HI_CAL:0);
    
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::anticoincidence(const Event::AcdDigiCol& tiles){
    using namespace Event;
    // purpose: set ACD trigger bits
    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << tiles.size() << " tiles found with hits" << endreq;
    unsigned int ret=0;
    for( AcdDigiCol::const_iterator it = tiles.begin(); it !=tiles.end(); ++it){
        ret |= b_ACDL; 
    } 
    return ret;
}


//------------------------------------------------------------------------------
StatusCode TriggerAlg::finalize() {
    StatusCode  sc = StatusCode::SUCCESS;
    
    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "Totals triggered/ processed: " << m_total << "/" << m_triggered ; 
    
    bitSummary(log.stream());
    log << endreq;

    return StatusCode::SUCCESS;
}
//------------------------------------------------------------------------------
void TriggerAlg::bitSummary(std::ostream& out){

    // purpose and method: make a summary of the bit frequencies to the stream

    using namespace std;
    int size= number_of_trigger_bits;  // bits to expect
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



