/**
*  @file TriRowBitsAlg.cxx
*  @brief Declaration and definition of the algorithm TriRowBitsAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/GlastRelease-scons/Trigger/src/TriRowBitsAlg.cxx,v 1.2.22.1 2010/10/08 16:46:57 heather Exp $
*/


// set the following define to compile Johann's code for special trigger bit diagnostics
// or better, move it to its own algorithm, as it is not involved in computing trigger bits itself.
#include "Trigger/TriRowBits.h"

#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"

#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"

#include "Event/Digi/TkrDigi.h"
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
/*! \class TriRowBitsAlg
\brief  alg that calculates the TriRowBits from TKR digis and diagnostics

@section Attributes for job options:

*/

class TriRowBitsAlg : public Algorithm {

public:
    //! Constructor of this form must be provided
    TriRowBitsAlg(const std::string& name, ISvcLocator* pSvcLocator); 

    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();

private:

    void computeTrgReqTriRowBits(TriRowBitsTds::TriRowBits&);

    /// access to the Glast Detector Service to read in geometry constants from XML files
    IGlastDetSvc *m_glastDetSvc;


};

//------------------------------------------------------------------------------
//static const AlgFactory<TriRowBitsAlg>  Factory;
//const IAlgFactory& TriRowBitsAlgFactory = Factory;
DECLARE_ALGORITHM_FACTORY(TriRowBitsAlg);
//------------------------------------------------------------------------------
/// 
TriRowBitsAlg::TriRowBitsAlg(const std::string& name, ISvcLocator* pSvcLocator) 
: Algorithm(name, pSvcLocator)
{


}
//------------------------------------------------------------------------------
/*! 
*/
StatusCode TriRowBitsAlg::initialize() 
{

    StatusCode sc = StatusCode::SUCCESS;

    MsgStream log(msgSvc(), name());

    // Use the Job options service to set the Algorithm's parameters
    setProperties();

    m_glastDetSvc = 0;
    sc = service("GlastDetSvc", m_glastDetSvc, true);
    if (sc.isSuccess() ) {
        sc = m_glastDetSvc->queryInterface(IID_IGlastDetSvc, (void**)&m_glastDetSvc);
    } else {
        log << MSG::INFO << "TriRowBitsAlg failed to get the GlastDetSvc" << endreq;
        log << MSG::INFO << "No matter, TriRowBitsAlg does not use it yet" << endreq;
        return StatusCode::SUCCESS;
    }
  
    return StatusCode::SUCCESS;
}




//------------------------------------------------------------------------------
StatusCode TriRowBitsAlg::execute() 
{

    // purpose: find digi collections in the TDS, pass them to functions to calculate the individual trigger bits

    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );

    SmartDataPtr<Event::TkrDigiCol> planes(eventSvc(), EventModel::Digi::TkrDigiCol);
    if( planes==0 ) {
        log << MSG::DEBUG << "No tkr digis found" << endreq;
        return StatusCode::SUCCESS;
    }

    // purpose and method: determine if there is tracker trigger, any 3-in-a-row, 
    //and fill out the TDS class TriRowBits, with the complete 3-in-a-row information

    using namespace Event;

    log << MSG::DEBUG << planes->size() << " tracker planes found with hits" << endreq;

    // define a map to sort hit planes according to tower and plane axis
    typedef std::pair<idents::TowerId, idents::GlastAxis::axis> Key;
    typedef std::map<Key, unsigned int> Map;
    Map layer_bits;

    // this loop sorts the hits by setting appropriate bits in the tower-plane hit map
    for( Event::TkrDigiCol::const_iterator it = planes->begin(); it != planes->end(); ++it){
        const TkrDigi& t = **it;
        if( t.getNumHits()== 0) continue; // this can happen if there are dead strips 
        layer_bits[std::make_pair(t.getTower(), t.getView())] |= layer_bit(t.getBilayer());
    }

    //!Creating Object in TDS.
    //!Documentation of three_in_a_row_bits available in Trigger/TriRowBits.h
    TriRowBitsTds::TriRowBits *rowbits= new TriRowBitsTds::TriRowBits;
    sc = eventSvc()->registerObject("/Event/TriRowBits", rowbits);
    if (sc.isFailure()) {
        log << MSG::WARNING << "Failed to register TriRowBits on the TDS" << endreq;
        log << MSG::WARNING << "Will not compute" << endreq;
        return StatusCode::SUCCESS;
    }

	//bool tkr_trig_flag = false;
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


        }
    }

    //Now we compute the 3 in a row combinations based on the trigger requests
    computeTrgReqTriRowBits(*rowbits);


    log << MSG::DEBUG;
    if(log.isActive()) log.stream() << *rowbits;
    log <<endreq;

    return StatusCode::SUCCESS;
}


StatusCode TriRowBitsAlg::finalize() {

    // purpose and method: make a summary

    StatusCode  sc = StatusCode::SUCCESS;

    return sc;
}


void TriRowBitsAlg::computeTrgReqTriRowBits(TriRowBitsTds::TriRowBits& rowbits)
{
    // Retrieve the Diagnostic data for this event
    // note: here we would also have access to the CAL diagnostic data.
    SmartDataPtr<LdfEvent::DiagnosticData> diagTds(eventSvc(), "/Event/Diagnostic");

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
