/** 
* @file TriggerAlg.cxx
* @brief Declaration and definition of the algorithm TriggerAlg.
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TriggerAlg.cxx,v 1.21 2003/08/29 13:36:34 burnett Exp $
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

#include "Event/Digi/TkrDigi.h"
#include "Event/Digi/CalDigi.h"
#include "Event/Digi/AcdDigi.h"

#include <map>
#include <cassert>

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

  @Section Attributes for job options:
  @param run [0] For setting the run number
  @param mask [-1] mask to apply to trigger word. -1 means any, 0 means all.

*/

class TriggerAlg : public Algorithm {
    
public:
    enum  {
        //! definition of  trigger bits
        
        b_ACDL =     1,  ///  set if cover or side veto, low threshold
            b_ACDH =     2,   ///  cover or side veto, high threshold
            b_Track=     4,   //  3 consecutive x-y layers hit
            b_LO_CAL=    8,  ///  single log above low threshold
            b_HI_CAL=   16,   /// single log above high threshold
			b_THROTTLE = 32,  /// throttle bit

         number_of_trigger_bits = 6 ///> for size of table
    };
    
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
    //! calculate ACD trigger bits
    /// @return the bits
    unsigned int  anticoincidence(const Event::AcdDigiCol&  planes);
    void bitSummary(std::ostream& out);

	//! sets the list(s) of hit ACD tiles
	void setACDTileList(const Event::AcdDigiCol& digiCol);
    //! gets the id of the next tower to look at 
    /// @return the tower id
	int getTowerID();
    //! gets the correct masks for the triggered tower
	void getMask(int towerID);
    //! remove the tower from the list, becuase it has been examined
	void removeTower(int twr);
    //! gets the tower ID and mask, then calls the functions that do the checking
	/// @return a loop control variable
	bool compare();

	StatusCode caltrigsetup();
    
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
    
    int m_run;
    int m_event;

    // for statistics
    int m_total;
    int m_triggered;
    std::map<int,int> m_counts; //map of values for each bit pattern

    std::map<idents::TowerId, int> m_tower_trigger_count;

   /// access to the Glast Detector Service to read in geometry constants from XML files
    IGlastDetSvc *m_glastDetSvc;

	bool m_throttle;
    unsigned short m_triggered_towers, m_number_triggered;
	unsigned int m_trigger_word, m_acdtop, m_acdX, m_acdY,
		         m_maskTop, m_maskX, m_maskY;  
    
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
StatusCode TriggerAlg::initialize() 
{
    

    StatusCode sc = StatusCode::SUCCESS;
    
    MsgStream log(msgSvc(), name());
    
    // Use the Job options service to set the Algorithm's parameters
    setProperties();
    
    log << MSG::INFO;
    if(log.isActive()) { log.stream() <<"Applying trigger mask: " <<  std::setbase(16) <<m_mask 
        << "initializing run number " << m_run;
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

	// initialize some variables
    m_number_triggered = 0;
	m_triggered_towers = 0;
	m_throttle = false;
       
    sc = caltrigsetup();

    return sc;
}

//------------------------------------------------------------------------------
StatusCode TriggerAlg::caltrigsetup()
{
    // purpose and method: extracting double constants for calorimeter trigger

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
    
    return StatusCode::SUCCESS;
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
    
    // set bits in the trigger word
    
    unsigned int trigger_bits = 
        (tkr? tracker(tkr) : 0 )
        | (cal? calorimeter(cal) : 0 )
        | (acd? anticoincidence(acd) : 0);

	//check the list of triggered towers against hit acd tiles.
	//add the throttle bit to trigger_bits
	bool done = false;
	while (!done){
		done = compare();
	}
	if (m_throttle==true){
        log << MSG::DEBUG << "Set the throttle bit" << endreq;	
		trigger_bits |= b_THROTTLE;
		m_throttle=false;//reset the throttle
	}
    
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

            if( h.run() < 0 || h.event() <0) {

                // event header info not set: create a new event  here
                h.setRun(m_run);
                h.setEvent(++m_event);
                h.setTrigger(trigger_bits);
                log << MSG::INFO; 
                if(log.isActive() ) log.stream() << "Creating run/event " << m_run <<"/"<<m_event  << " trigger & mask "  
                    << std::setbase(16) << (m_mask==0?trigger_bits:trigger_bits& m_mask);
                log << endreq;
            }else {
                // assume set by reading digiRoot file
                log << MSG::DEBUG ;
                if(log.isActive()) log.stream() << "Read run/event " << h.run() << "/" << h.event() << " trigger & mask "
                    << std::setbase(16) << (m_mask==0 ? trigger_bits : trigger_bits & m_mask);
                log << endreq;
                
                if(h.trigger()==0){
                    h.setTrigger(trigger_bits);
                }else  if (h.trigger() != 0xbaadf00d && trigger_bits != h.trigger() ) {
                    log << MSG::WARNING;
                    if(log.isActive()) log.stream() << "Trigger bits read back do not agree with recalculation! " 
                        << std::setbase(16) <<trigger_bits << " vs. " << h.trigger();
                    log << endreq;
                }
            }
            
        } else { 
            log << MSG::ERROR << " could not find the event header" << endreq;
            return StatusCode::FAILURE;
        }

    }
    
    return sc;
}

//------------------------------------------------------------------------------
unsigned int TriggerAlg::tracker(const Event::TkrDigiCol&  planes)
{
    // purpose and method: determine if there is tracker trigger, 3-in-a-row

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
    
    // now look for a three in a row in x-y coincidence
    for( Map::iterator itr = layer_bits.begin(); itr !=layer_bits.end(); ++ itr){

        Key theKey=(*itr).first;
        idents::TowerId tower = theKey.first;
        if( theKey.second==idents::GlastAxis::X) { 
            // found an x: and with corresponding Y (if exists)
            unsigned int 
                xbits = (*itr).second,
                ybits = layer_bits[std::make_pair(tower, idents::GlastAxis::Y)];


            if( three_in_a_row( xbits & ybits) ){
                // OK: tag the tower for stats
				m_number_triggered++;//counts the number of towers triggered
				m_triggered_towers |= int(pow(2,tower.id()));//adds the tower to the list
                m_tower_trigger_count[tower]++;
            }
        }
    }
	if (m_number_triggered > 0){ 
		m_number_triggered=0;//reset m_number_triggered;
		return b_Track;
	}
	return 0;
    
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
        if(eneP> m_HICALthreshold || eneM > m_HICALthreshold) m_hical = true; 
        
    }
        
    
    return (m_local ? b_LO_CAL:0) | (m_hical ? b_HI_CAL:0);
    
}
//------------------------------------------------------------------------------
unsigned int TriggerAlg::anticoincidence(const Event::AcdDigiCol& tiles)
{
    // purpose and method: calculate ACD trigger bits from the list of hit tiles
    
    using namespace Event;
    // purpose: set ACD trigger bits
    MsgStream   log( msgSvc(), name() );

	StatusCode  sc = StatusCode::SUCCESS;

	//initialize the acd tile lists
	m_acdtop = 0;
	m_acdX = 0;
	m_acdY = 0;

	//get the veto threshold and declare some local variables
    static double s_vetoThresholdMeV;
	sc = m_glastDetSvc->getNumericConstByName("acd.vetoThreshold", &s_vetoThresholdMeV);
    short s_face;
	short s_tile;
	short s_row;
	short s_column;
	if (sc.isFailure()) {
        log << MSG::INFO << "Unable to retrieve threshold, setting the value to 0.4 MeV" << endreq;
        s_vetoThresholdMeV = 0.4;
    }
    
    Event::AcdDigiCol::const_iterator acdDigiIt;
	
	log << MSG::DEBUG << tiles.size() << " tiles found with hits" << endreq;
    unsigned int ret=0;

	for (acdDigiIt = tiles.begin(); acdDigiIt != tiles.end(); ++acdDigiIt) {
       
		idents::AcdId id = (*acdDigiIt)->getId();

        // if it is a tile...
		if (id.tile()==true) {
        // toss out hits below threshold
        if ((*acdDigiIt)->getEnergy() < s_vetoThresholdMeV) continue;
           //get the face, row, and column numbers
		   s_face = id.face();
		   s_row  = id.row();
		   s_column = id.column();
           //set the tile number (depending on the face)
		   if (s_face == 0) 
			   s_tile = (4-s_column)*5 + s_row;
		   else if ((s_face == 2) || (s_face == 3)){
			   if (s_row < 3) s_tile = s_row*5 + s_column;
		       else           s_tile = 15;
		   }
		   else if ((s_face == 1) || (s_face == 4)){
			   if (s_row < 3) s_tile = s_row*5 + (4-s_column);
		       else           s_tile = 15;
		   }
		   //set the appropriate bit for the tile number
		   switch (s_face) {
		      case 0: //top
				 m_acdtop |= int(pow(2,s_tile));
                 break;
              case 1: //-X
                 m_acdX |= int(pow(2,s_tile+16));
                 break;
              case 2: //-Y
                 m_acdY |= int(pow(2,s_tile+16));
			     break;
              case 3: //+X
                 m_acdX |= int(pow(2,s_tile));
                 break;
              case 4: //+Y
                 m_acdY |= int(pow(2,s_tile));
                 break;
           }//switch
		}//if (id.tile())

		ret |= b_ACDL; 
        //TODO: check threshold, set high bit
    }

    return ret;
}
 	
int TriggerAlg::getTowerID()
{
   //looks at the list of triggered towers, and gets the first tower
   //to look at.  
    if (m_triggered_towers & 0x1) return 0;
	if (m_triggered_towers & 0x2) return 1;
    if (m_triggered_towers & 0x4) return 2;
	if (m_triggered_towers & 0x8) return 3;
	if (m_triggered_towers & 0x10) return 4;
	if (m_triggered_towers & 0x20) return 5;
	if (m_triggered_towers & 0x40) return 6;
	if (m_triggered_towers & 0x80) return 7;
	if (m_triggered_towers & 0x100) return 8;
	if (m_triggered_towers & 0x200) return 9;
	if (m_triggered_towers & 0x400) return 10;
	if (m_triggered_towers & 0x800) return 11;
	if (m_triggered_towers & 0x1000) return 12;
	if (m_triggered_towers & 0x2000) return 13;
	if (m_triggered_towers & 0x4000) return 14;
	if (m_triggered_towers & 0x8000) return 15;
	
	return -1; //if there are no matches
}

void TriggerAlg::getMask(int towerID)
{
	m_maskTop = 0;
	m_maskX = 0;
	m_maskY = 0;

	//If using the acd top and the top 2 side rows of the acd, use up 
	//to 12 tiles for each tower
	switch (towerID)
	{
	case 0://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x318000;//0000 0000 0011 0001 1000 0000 0000 0000
		m_maskX = 0x03180000;//0000 0011 0001 1000 0000 0000 0000 0000
		m_maskY = 0x630000;  //0000 0000 0110 0011 0000 0000 0000 0000
		return;
    case 1://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C00; //0000 0000 0000 0001 1000 1100 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0xC60000;  //0000 0000 1100 0110 0000 0000 0000 0000
		return;
	case 2://                     28   24   20   16   12   8    4    0
		m_maskTop = 0xC60;   //0000 0000 0000 0000 0000 1100 0110 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C0000; //0000 0001 1000 1100 0000 0000 0000 0000
		return;
	case 3://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x63;    //0000 0000 0000 0000 0000 0000 0110 0011
		m_maskX = 0x63;      //0000 0000 0000 0000 0000 0000 0110 0011
		m_maskY = 0x3180000; //0000 0011 0001 1000 0000 0000 0000 0000
		return;
	case 4://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x630000;//0000 0000 0110 0011 0000 0000 0000 0000
		m_maskX = 0x18C0000; //0000 0001 1000 1100 0000 0000 0000 0000
		m_maskY = 0;         //0
		return;
	case 5://                      28   24   20   16   12   8    4    0
		m_maskTop = 0x31800;  //0000 0000 0000 0011 0001 1000 0000 0000
		m_maskX = 0;          //0
		m_maskY = 0;          //0
	    return;
	case 6://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C0;  //0000 0000 0000 0000 0001 1000 1100 0000
		m_maskX = 0;         //0
		m_maskY = 0;         //0
		return;
	case 7://                     28   24   20   16   12   8    4    0
		m_maskTop = 0xC6;    //0000 0000 0000 0000 0000 0000 1100 0110
		m_maskX = 0xC6;      //0000 0000 0000 0000 0000 0000 1100 0110
		m_maskY = 0;         //0
		return;
	case 8://                      28   24   20   16   12   8    4    0
		m_maskTop = 0xC60000; //0000 0000 1100 0110 0000 0000 0000 0000
		m_maskX = 0xC60000;   //0000 0000 1100 0110 0000 0000 0000 0000
		m_maskY = 0;          //0
		return;
	case 9://                      28   24   20   16   12   8    4    0
		m_maskTop = 0x63000;  //0000 0000 0000 0110 0011 0000 0000 0000
		m_maskX = 0;          //0
		m_maskY = 0;          //0
		return;
	case 10://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x3180;  //0000 0000 0000 0000 0011 0001 1000 0000
		m_maskX = 0;         //0 
		m_maskY = 0;         //0 
		return;
	case 11://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x18C;   //0000 0000 0000 0000 0000 0001 1000 1100
		m_maskX = 0x18C;     //0000 0000 0000 0000 0000 0001 1000 1100
		m_maskY = 0;         //0
		return;
	case 12://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C0000;//0000 0001 1000 1100 0000 0000 0000 0000
		m_maskX = 0x630000;   //0000 0000 0110 0011 0000 0000 0000 0000
		m_maskY = 0x318;      //0000 0000 0000 0000 0000 0011 0001 1000
		return;
	case 13://                    28   24   20   16   12   8    4    0
		m_maskTop = 0xC6000; //0000 0000 0000 1100 0110 0000 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C;     //0000 0000 0000 0000 0000 0001 1000 1100
		return;
    case 14://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x6300;  //0000 0000 0000 0000 0110 0011 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0xC6;      //0000 0000 0000 0000 0000 0000 1100 0110
		return;
    case 15://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x318;   //0000 0000 0000 0000 0000 0011 0001 1000
		m_maskX = 0x318;     //0000 0000 0000 0000 0000 0011 0001 1000
		m_maskY = 0x63;      //0000 0000 0000 0000 0000 0000 0110 0011
		return;
	}
	return;
}

void TriggerAlg::removeTower(int twr)
{
	if (twr==0){m_triggered_towers ^= 0x1; return;}
	if (twr==1){m_triggered_towers ^= 0x2; return;}
	if (twr==2){m_triggered_towers ^= 0x4; return;}
	if (twr==3){m_triggered_towers ^= 0x8; return;}
	if (twr==4){m_triggered_towers ^= 0x10; return;}
	if (twr==5){m_triggered_towers ^= 0x20; return;}
	if (twr==6){m_triggered_towers ^= 0x40; return;}
	if (twr==7){m_triggered_towers ^= 0x80; return;}
	if (twr==8){m_triggered_towers ^= 0x100; return;}
	if (twr==9){m_triggered_towers ^= 0x200; return;}
	if (twr==10){m_triggered_towers ^= 0x400; return;}
	if (twr==11){m_triggered_towers ^= 0x800; return;}
	if (twr==12){m_triggered_towers ^= 0x1000; return;}
	if (twr==13){m_triggered_towers ^= 0x2000; return;}
	if (twr==14){m_triggered_towers ^= 0x4000; return;}
	if (twr==15){m_triggered_towers ^= 0x8000; return;}
	
	return;
}

bool TriggerAlg::compare()
{
   int  tower = 0;
   bool done = true;
   bool notdone = false;
   
   MsgStream log(msgSvc(), name());

   if ((tower = getTowerID()) == -1) return done;//no more towers
   getMask(tower);
   removeTower(tower);
   if ( (m_acdtop & m_maskTop)||(m_acdX & m_maskX)||(m_acdY & m_maskY) )
   {
      m_throttle = true;
	  return done;
   }
   if (m_triggered_towers==0) return done;
   return notdone;
}

//------------------------------------------------------------------------------
StatusCode TriggerAlg::finalize() {

    // purpose and method: make a summary

    using namespace std;
    StatusCode  sc = StatusCode::SUCCESS;
    
    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "Totals triggered/ processed: " << m_triggered << "/" << m_total ; 
    
    if(log.isActive()) bitSummary(log.stream());
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
/*
Throttle Documentation

author: David Wren, dnwren@milkyway.gsfc.nasa.gov
date: 9 March 2004

The throttle is designed to activate if a triggered tower is next to an ACD tile
that is above its veto threshold (it is hit).  We only consider the top face of
the ACD, and the top two rows on the sides.  This can easily be changed if we 
want to consider the top 3 rows of the ACD.  I've included the appropriate bit
masks for this at the end of this documentation.

First, we read in the list of tiles that are hit from AcdDigi, which gives a 
face, a row, and a column.  The format of the ACD digi Id is described in 
AcdId.h:
  __ __       __ __ __     __ __ __ __      __ __ __ __
  LAYER         FACE           ROW            COLUMN
                 OR                             OR
           RIBBON ORIENTATION               RIBBON NUMBER

The face codes are:
Face 0 == top  (hat)
Face 1 == -X side 
Face 2 == -Y side
Face 3 == +X side
Face 4 == +Y side
Face 5, 6 == ribbon

For the top, the tile-tower association looks like this (where lines indicate
the tower-tile associations we are using):

Tile     24    19    14    9	 4    /|\ +Y 
           \  /  \  /  \  /  \  /      |  
Tower	    12    13    14    15       | 
           /  \  /  \  /  \  /  \      |
Tile	 23    18    13	   8	 3     |________\
           \  /  \  /  \  /  \  /               /
Tower       8     9     10    11               +X
           /  \  /  \  /  \  /  \
Tile     22    17    12    7	 2
           \  /  \  /  \  /  \  /
Tower       4     5     6     7
           /  \  /  \  /  \  /  \
Tile     21    16    11    6     1
           \  /  \  /  \  /  \  /
Tower       0     1     2     3
           /  \  /  \  /  \  /  \
Tile     20    15    10    5     0


For face 1 (-X side), the tile tower association looks like this:

ACD Column    4      3     2      1     0
                
                tower tower tower tower
                 12     8     4     0 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---

So tower 12, for example, is shadowed by tiles 0, 1, 5, 6, 10, 11, and 15.
The column and row designations come from the AcdId designation.


For face 2 (-Y side), the tile tower association looks like this:

ACD Column    0      1     2      3     4
                
                tower tower tower tower
                  0     1     2     3 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---


For face 3 (+X side), the tile tower association looks like this:

ACD Column    0      1     2      3     4
                
                tower tower tower tower
                  3     7     11     15 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---


For face 4 (+Y side), the tile tower association looks like this:

ACD Column    4      3     2      1     0
                
                tower tower tower tower
                 15     14    13     12 
             
             tile   tile  tile  tile  tile  
                   
ACD Row  0    0      1     2      3     4     
                  
                 
         1    5      6     7      8     9  
                 
                 
         2    10     11    12     13    14
         
         
         3             --- 15 ---

So depending on which ACD rows we are considering for the throttle, we have 
different numbers of tiles associated with a triggered tower:

Tower location           Number of Rows Considered       Number of Tiles

Center                             2                           4
                                   3                           4 

Edge                               2                           8
                                   3                           10

Corner                             2                           12
                                   3                           16


In the code, the formula for going from the AcdId.h info to a tile number is this:

		   if (s_face == 0) 
			   s_tile = (4-s_column)*5 + s_row;
		   else if ((s_face == 2) || (s_face == 3)){
			   if (s_row < 3) s_tile = s_row*5 + s_column;
		       else           s_tile = 15;
		   }
		   else if ((s_face == 1) || (s_face == 4)){
			   if (s_row < 3) s_tile = s_row*5 + (4-s_column);
		       else           s_tile = 15;
		   }

The algorithm first creates the list of ACD tiles that are hit,
and a list of towers that are triggered.  From the list of triggered
towers, it looks up bit masks of tiles to check for that tower.  
These masks are hard coded according to the following scheme:

The top ACD tiles are represented by a 32 bit words: m_maskTop or 
m_acdtop, where "mask" indicates that the word is the mask of bits
that corresponds to a given tower, and "acd" indicates the list of 
hit ACD tiles.
               1111 1111 1111 1111 
bit position   fedc ba98 7654 3210 fedc ba98 7654 3210
               ---------------------------------------
                       1 1111 1111 
ACD tile (hex)         8 7654 3210 fedc ba98 7654 3210                    

The side ACD tiles are represented by one of two 32 bit words.  The
two words each contain information about ACD tiles that are in one
plane (X or Y).  The 16 least significant bits hold information on
the "+" faces (X+ or Y+), while the 16 most significant bits hold
information on the "-" faces (X- or Y-).

m_acdX and m_maskX are arranged like this:
               1111 1111 1111 1111 
bit position   fedc ba98 7654 3210 fedc ba98 7654 3210
               ---------------------------------------                        
ACD tile (hex)  edc ba98 7654 3210  edc ba98 7654 3210
                     X- face             X+ face

m_acdY and m_maskY are arranged similarly:
               1111 1111 1111 1111 
bit position   fedc ba98 7654 3210 fedc ba98 7654 3210
               ---------------------------------------                        
ACD tile (hex)  edc ba98 7654 3210  edc ba98 7654 3210
                     Y- face             Y+ face

ANDing the appropriate masks with the tile lists allows us to
tell if a hit tile corresponds to a triggered tower:

if ( (m_acdtop & m_maskTop)||(m_acdX & m_maskX)||(m_acdY & m_maskY) )

If this condition is met, the throttle is activated.
----------------------------------------------------------------------

If the user wants to use the top of the acd and the top 3 rows
of the side tiles (instead of the top 2 rows), the masks in
TriggerAlg::getMask(int towerID) can be replaced with the set of masks
below.

    //If using the acd top and top 3 side rows of the acd, use up 
    //to 16 tiles for each tower:
	switch (towerID)
	{
	case 0://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x318000;//0000 0000 0011 0001 1000 0000 0000 0000
		m_maskX = 0x63180000;//0110 0011 0001 1000 0000 0000 0000 0000
		m_maskY = 0x0C630000;//0000 1100 0110 0011 0000 0000 0000 0000
		return;
    case 1://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C00; //0000 0000 0000 0001 1000 1100 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C60000;//0001 1000 1100 0110 0000 0000 0000 0000
		return;
	case 2://                     28   24   20   16   12   8    4    0
		m_maskTop = 0xC60;   //0000 0000 0000 0000 0000 1100 0110 0000
		m_maskX = 0;         //0
		m_maskY = 0x318C0000;//0011 0001 1000 1100 0000 0000 0000 0000
		return;
	case 3://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x63;    //0000 0000 0000 0000 0000 0000 0110 0011
		m_maskX = 0xC63;     //0000 0000 0000 0000 0000 1100 0110 0011
		m_maskY = 0x63180000;//0110 0011 0001 1000 0000 0000 0000 0000
		return;
	case 4://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x630000;//0000 0000 0110 0011 0000 0000 0000 0000
		m_maskX = 0x318C0000;//0011 0001 1000 1100 0000 0000 0000 0000
		m_maskY = 0;         //0
		return;
	case 5://                      28   24   20   16   12   8    4    0
		m_maskTop = 0x31800;  //0000 0000 0000 0011 0001 1000 0000 0000
		m_maskX = 0;          //0
		m_maskY = 0;          //0
	    return;
	case 6://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C0;  //0000 0000 0000 0000 0001 1000 1100 0000
		m_maskX = 0;         //0
		m_maskY = 0;         //0
		return;
	case 7://                     28   24   20   16   12   8    4    0
		m_maskTop = 0xC6;    //0000 0000 0000 0000 0000 0000 1100 0110
		m_maskX = 0x18C6;    //0000 0000 0000 0000 0001 1000 1100 0110
		m_maskY = 0;         //0
		return;
	case 8://                      28   24   20   16   12   8    4    0
		m_maskTop = 0xC60000; //0000 0000 1100 0110 0000 0000 0000 0000
		m_maskX = 0x18C60000; //0001 1000 1100 0110 0000 0000 0000 0000
		m_maskY = 0;          //0
		return;
	case 9://                      28   24   20   16   12   8    4    0
		m_maskTop = 0x63000;  //0000 0000 0000 0110 0011 0000 0000 0000
		m_maskX = 0;          //0
		m_maskY = 0;          //0
		return;
	case 10://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x3180;  //0000 0000 0000 0000 0011 0001 1000 0000
		m_maskX = 0;         //0 
		m_maskY = 0;         //0 
		return;
	case 11://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x18C;   //0000 0000 0000 0000 0000 0001 1000 1100
		m_maskX = 0x318C;    //0000 0000 0000 0000 0011 0001 1000 1100
		m_maskY = 0;         //0
		return;
	case 12://                     28   24   20   16   12   8    4    0
		m_maskTop = 0x18C0000;//0000 0001 1000 1100 0000 0000 0000 0000
		m_maskX = 0xC630000;  //0000 1100 0110 0011 0000 0000 0000 0000
		m_maskY = 0xC318;     //0000 0000 0000 0000 0110 0011 0001 1000
		return;
	case 13://                    28   24   20   16   12   8    4    0
		m_maskTop = 0xC6000; //0000 0000 0000 1100 0110 0000 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x318C;    //0000 0000 0000 0000 0011 0001 1000 1100
		return;
    case 14://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x6300;  //0000 0000 0000 0000 0110 0011 0000 0000
		m_maskX = 0;         //0
		m_maskY = 0x18C6;    //0000 0000 0000 0000 0001 1000 1100 0110
		return;
    case 15://                    28   24   20   16   12   8    4    0
		m_maskTop = 0x318;   //0000 0000 0000 0000 0000 0011 0001 1000
		m_maskX = 0x6318;    //0000 0000 0000 0000 0110 0011 0001 1000
		m_maskY = 0x0C63;    //0000 0000 0000 0000 0000 1100 0110 0011
		return;
	}
*/

