/** 
* @file TriggerAlg.cxx
* @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerAlg.cxx,v 1.4 2002/05/11 23:16:14 burnett Exp $
*/

// Include files

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"


#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/StatusCode.h"

#include "CLHEP/Geometry/Point3D.h"
#include "CLHEP/Vector/LorentzVector.h"

#include "Event/TopLevel/EventModel.h"
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
            b_CAL=       8,  //  single log above low threshold
            b_HI_CAL=   16   //  3 cal layers in a row above high threshold
            
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

    unsigned int m_mask;
    int m_acd_hits;
    int m_log_hits;
    bool m_local;
    bool m_hical;
    unsigned m_hical_bits[16];
    


    
};

//------------------------------------------------------------------------------
static const AlgFactory<TriggerAlg>  Factory;
const IAlgFactory& TriggerAlgFactory = Factory;
//------------------------------------------------------------------------------
/// 
TriggerAlg::TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator) :
Algorithm(name, pSvcLocator){
    declareProperty("mask"     ,  m_mask=0xffffffff); // trigger mask
    
    
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
    return sc;
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
    
     
    // apply filter for subsequent processing.
    if( m_mask!=0 && ( trigger_bits & m_mask) == 0) {
        setFilterPassed( false );
        log << MSG::INFO << "Event did not trigger" << endreq;
    }else {
        log << MSG::INFO << "Event triggered, masked bits = "
            << std::setbase(16) << (m_mask==0?trigger_bits:trigger_bits& m_mask) << endreq;
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
unsigned int TriggerAlg::calorimeter(const Event::CalDigiCol& logs){
    using namespace Event;
    // purpose: set calorimeter trigger bits
    MsgStream   log( msgSvc(), name() );
    log << MSG::DEBUG << logs.size() << " crystals found with hits" << endreq;

    for( CalDigiCol::const_iterator it = logs.begin(); it != logs.end(); ++it ){
    }
    return 0;

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
    
    return StatusCode::SUCCESS;
}

//------------------------------------------------------------------------------



