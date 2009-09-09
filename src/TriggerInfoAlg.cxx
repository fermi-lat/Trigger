/**
*  @file TriggerInfoAlg.cxx
*  @brief Declaration and definition of the algorithm TriggerInfoAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerInfoAlg.cxx,v 1.2 2009/02/12 16:54:58 usher Exp $
*/

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"
#include "GaudiKernel/Property.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/StatusCode.h"

#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "Event/TopLevel/DigiEvent.h"
#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/AcdDigi.h"

#include "Event/Trigger/TriggerInfo.h"

#include "enums/TriggerBits.h"

#include "TriggerTables.h"
#include "EnginePrescaleCounter.h"
#include "ConfigSvc/IConfigSvc.h"

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
/*! \class TriggerInfoAlg
\brief  alg that sets trigger information

@section Attributes for job options:
@param run [0] For setting the run number

*/

class TriggerInfoAlg : public Algorithm {

public:
    //! Constructor of this form must be provided
    TriggerInfoAlg(const std::string& name, ISvcLocator* pSvcLocator); 

    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();

private:
    //! determine tracker trigger bits
    unsigned int  tracker(unsigned short &tkrVector);

    /// determine cal trigger bits
    /// \param calLoVector destination for callo triggers (1 bit per
    /// tower
    /// \param calLoVector destination for calhi triggers (1 bit per
    /// tower
    StatusCode calorimeter(unsigned short &calLoVector, unsigned short &calHiVector);

    //! calculate ACD trigger bits
    /// @return the bits
    unsigned int  anticoincidence(unsigned short &cnoVector, std::vector<unsigned int>& vetotilelist);

    void roiDefaultSetup();
    unsigned int throttle(unsigned int tkrVector, std::vector<unsigned int>& tilelist, unsigned short &roiVector); 

    void makeTileListMap(std::vector<unsigned int>&tileList, Event::TriggerInfo::TileList& tileListMap);

    int                  m_event;
    StringProperty       m_table;
    IntegerArrayProperty m_prescale;

    IConfigSvc*          m_configSvc;

    /// use to calculate cal trigger response from digis if 
    /// cal trig response has not already been calculated
    ICalTrigTool*        m_calTrigTool;

    /// use for the names of the bits
    std::map<int, std::string> m_bitNames;


    Trigger::TriggerTables* m_triggerTables;
    EnginePrescaleCounter* m_pcounter;
    TrgRoi *m_roi;

};

//------------------------------------------------------------------------------
static const AlgFactory<TriggerInfoAlg>  Factory;
const IAlgFactory& TriggerInfoAlgFactory = Factory;
//------------------------------------------------------------------------------
/// 
TriggerInfoAlg::TriggerInfoAlg(const std::string& name, ISvcLocator* pSvcLocator) 
  : Algorithm(name, pSvcLocator), m_configSvc(0), m_calTrigTool(0), m_pcounter(0)
{
    declareProperty("engine",         m_table = "ConfigSvc");     // set to "default"  to use default engine table
    declareProperty("prescale",       m_prescale=std::vector<int>());        // vector of prescale factors

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
}
//------------------------------------------------------------------------------
/*! 
*/
StatusCode TriggerInfoAlg::initialize() 
{
    StatusCode sc = StatusCode::SUCCESS;

    MsgStream log(msgSvc(), name());

    // Use the Job options service to set the Algorithm's parameters
    setProperties();

    sc = toolSvc()->retrieveTool("CalTrigTool", 
                                 "CalTrigTool",
                                 m_calTrigTool,
                                 0); /// could be shared
    if (sc.isFailure() ) 
    {
        log << MSG::ERROR << "  Unable to create CalTrigTool" << endreq;
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
            m_pcounter=new EnginePrescaleCounter(m_prescale.value());
        }
        else
        {
            // selected trigger tables and engines for event selection
            m_triggerTables = new Trigger::TriggerTables(m_table.value(), m_prescale.value());

            log << MSG::INFO << "Trigger tables: \n";
            m_triggerTables->print(log.stream());
            log << endreq;
        }
    }

    if (!m_pcounter) // set up default ROI config
    {
        m_roi = new TrgRoi;
        roiDefaultSetup();
    }

    return sc;
}




//------------------------------------------------------------------------------
StatusCode TriggerInfoAlg::execute() 
{
    // purpose: find digi collections in the TDS, pass them to functions to calculate the individual trigger bits

    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );

    // set bits in the trigger word
    unsigned short tkrVector           = 0;
    unsigned short cnoVector           = 0;
    unsigned short roiVector           = 0;
    unsigned short calLoVector         = 0;
    unsigned short calHiVector         = 0;
    unsigned short deltaEventTime      = 0xffff;
    unsigned short deltaWindowOpenTime = 0xffff;

    std::vector<unsigned int> tileList;

    unsigned int trigger_bits = tracker(tkrVector) | anticoincidence(cnoVector,tileList);

    /// process calorimeter trigger bits
    if (calorimeter(calLoVector, calHiVector).isFailure())
        return StatusCode::FAILURE;

    trigger_bits |= (calLoVector ? enums::b_LO_CAL:0) |(calHiVector ? enums::b_HI_CAL:0);

    if (m_pcounter) //using ConfigSvc
    {
        const TrgConfig* tcf = m_configSvc->getTrgConfig();

        m_roi = const_cast<TrgRoi*>(tcf->roi());
        
        if (m_roi == 0) 
        {
            log << MSG::ERROR << "Failed to get ROI mapping from MOOT" << endreq;
            return StatusCode::FAILURE;
        } 
    }

    if (tkrVector!=0 && !tileList.empty())
    {
        trigger_bits |= throttle(tkrVector,tileList,roiVector);
    }

    // Define the mask map for struck tiles
    Event::TriggerInfo::TileList tileListMap;

    // Translate from tile list vector to tile list map
    makeTileListMap(tileList, tileListMap);

    // Create and fill the TriggerInfo TDS object
    Event::TriggerInfo* triggerInfo = new Event::TriggerInfo();

    triggerInfo->initTriggerInfo(trigger_bits, 
                                 tkrVector, 
                                 roiVector, 
                                 calLoVector, 
                                 calHiVector, 
                                 cnoVector, 
                                 deltaEventTime,
                                 deltaWindowOpenTime, 
                                 tileListMap);

    sc = eventSvc()->registerObject("/Event/TriggerInfo", triggerInfo);


    return sc;
}

//------------------------------------------------------------------------------
StatusCode TriggerInfoAlg::finalize() {

    // purpose and method: make a summary

    StatusCode  sc = StatusCode::SUCCESS;

    MsgStream log(msgSvc(), name());

    return sc;
}

//------------------------------------------------------------------------------
unsigned int TriggerInfoAlg::tracker(unsigned short &tkrVector)
{
    // purpose and method: determine if there is tracker trigger, any 3-in-a-row, 
    //and fill out the TDS class TriRowBits, with the complete 3-in-a-row information
    MsgStream   log( msgSvc(), name() );

    // Set default return value
    tkrVector = 0;

    // Find the TkrDigi collection first

    SmartDataPtr<Event::TkrDigiCol> planes(eventSvc(), EventModel::Digi::TkrDigiCol);
    if( planes == 0 )
    {
        log << MSG::DEBUG << "No tkr digis found" << endreq;
        return 0;
    }

    log << MSG::DEBUG << planes->size() << " tracker planes found with hits" << endreq;

    // define a map to sort hit planes according to tower and plane axis
    typedef std::pair<idents::TowerId, idents::GlastAxis::axis> Key;
    typedef std::map<Key, unsigned int> Map;
    Map layer_bits;

    // this loop sorts the hits by setting appropriate bits in the tower-plane hit map
    for( Event::TkrDigiCol::const_iterator it = planes->begin(); it != planes->end(); ++it){
        const Event::TkrDigi& t = **it;
        if( t.getNumHits()== 0) continue; // this can happen if there are dead strips 
        layer_bits[std::make_pair(t.getTower(), t.getView())] |= layer_bit(t.getBilayer());
    }


    bool tkr_trig_flag = false;
    tkrVector=0;
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
                tkrVector|=1<<tower.id();
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
unsigned int TriggerInfoAlg::anticoincidence(unsigned short& cnoVector, std::vector<unsigned int> &vetotilelist)
{
    // purpose and method: calculate ACD trigger bits from the list of hit tiles

    // purpose: set ACD trigger bits
    MsgStream   log( msgSvc(), name() );

    unsigned int ret=0;
    cnoVector=0;

    // Look up the ACD digi collection
    SmartDataPtr<Event::AcdDigiCol> tiles(eventSvc(), EventModel::Digi::AcdDigiCol);
    if( tiles == 0 ) log << MSG::DEBUG << "No acd digis found" << endreq;

    log << MSG::DEBUG << tiles->size() << " tiles found with hits" << endreq;

    for( Event::AcdDigiCol::const_iterator it = tiles->begin(); it !=tiles->end(); ++it){
        // check if hitMapBit is set (veto) which will correspond to 0.3 MIP.
        // 20060109 Agreed at Analysis Meeting that onboard threshold is 0.3 MIP
        const Event::AcdDigi& digi = **it;

        // If the digi is purely overlay, then skip it for the trigger
        if (digi.getStatus() == (unsigned int)Event::AcdDigi::DIGI_OVERLAY) continue;
        
        // Digi id
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
                cnoVector|=1<<garc;
            }
            if (digi.getCno(Event::AcdDigi::B)){
                unsigned int pmt= Event::AcdDigi::B;
                idents::AcdId::convertToGarcGafe(id,pmt,garc,gafe);
                cnoVector|=1<<garc;
            }
        }
    } 
    return ret;
}

StatusCode TriggerInfoAlg::calorimeter(unsigned short &calLoVector, unsigned short &calHiVector) 
{
    if(m_calTrigTool->getCALTriggerVector(idents::CalXtalId::LARGE, calLoVector).isFailure())
        return StatusCode::FAILURE;
    if(m_calTrigTool->getCALTriggerVector(idents::CalXtalId::SMALL, calHiVector).isFailure())
        return StatusCode::FAILURE;

    return StatusCode::SUCCESS;
}

//------------------------------------------------------------------------------
unsigned int TriggerInfoAlg::throttle(unsigned int tkrVector, std::vector<unsigned int>& tilelist, unsigned short &roiVector){
    unsigned int triggered =0;
    roiVector=0;
    for (unsigned int i=0;i<tilelist.size();i++){
        std::vector<unsigned long> rr=m_roi->roiFromName(tilelist[i]);
        for (unsigned int j=0;j<rr.size();j++){
            if (tkrVector & (1<<rr[j])){
                triggered|=enums::b_ROI;
            }
            roiVector|=1<<rr[j];
        }
    }
    return triggered;
}

//------------------------------------------------------------------------------
void TriggerInfoAlg::roiDefaultSetup()
{
    m_roi->setRoiRegister(  0 , 0x30001 );
    m_roi->setRoiRegister(  1 , 0xc0006 );
    m_roi->setRoiRegister(  2 , 0x110008 );
    m_roi->setRoiRegister(  3 , 0x660033 );
    m_roi->setRoiRegister(  4 , 0x8800cc );
    m_roi->setRoiRegister(  5 , 0x3300110 );
    m_roi->setRoiRegister(  6 , 0xcc00660 );
    m_roi->setRoiRegister(  7 , 0x11000880 );
    m_roi->setRoiRegister(  8 , 0x66003300 );
    m_roi->setRoiRegister(  9 , 0x8800cc00 );
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
void TriggerInfoAlg::makeTileListMap(std::vector<unsigned int>&tileList, Event::TriggerInfo::TileList& tileListMap)
{
    MsgStream log(msgSvc(), name());

    // Clear the map and zero out the values
    tileListMap.clear();
    tileListMap["xzm"] = 0;
    tileListMap["xzp"] = 0;
    tileListMap["yzm"] = 0;
    tileListMap["yzp"] = 0;
    tileListMap["xy"]  = 0;
    tileListMap["rbn"] = 0;
    tileListMap["na"]  = 0;

    // Loop through the input vector and translate to bit map
    for (std::vector<unsigned int>::iterator tileItr = tileList.begin(); tileItr != tileList.end(); tileItr++)
    {
        unsigned int id       = *tileItr;
        unsigned int gemIndex = idents::AcdId::gemIndexFromTile(id);

        // Now do a look up...
        if      (gemIndex <  16) tileListMap["xzm"] |= 1 <<  gemIndex;
        else if (gemIndex <  32) tileListMap["xzp"] |= 1 << (gemIndex - 16);
        else if (gemIndex <  48) tileListMap["yzm"] |= 1 << (gemIndex - 32);
        else if (gemIndex <  64) tileListMap["yzp"] |= 1 << (gemIndex - 48);
        else if (gemIndex <  89) tileListMap["xy"]  |= 1 << (gemIndex - 64);
        else if (gemIndex < 104) tileListMap["rbn"] |= 1 << (gemIndex - 96);
        else if (gemIndex < 123) tileListMap["na"]  |= 1 << (gemIndex - 112);
        else log << MSG::ERROR << "Bad tile gem index: " << gemIndex << endreq;
    }

    return;
}
