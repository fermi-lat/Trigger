// $Header: /nfs/slac/g/glast/ground/cvs/userAlg/src/UserAlg.cxx,v 1.4 2001/06/07 23:12:05 burnett Exp $

// Include files
// Gaudi system includes
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"

// ntuple interface
#include "ntupleWriterSvc/INTupleWriterSvc.h"


// TDS class declarations: input data, and McParticle tree
#include "GlastEvent/data/TdGlastData.h"
#include "GlastEvent/MonteCarlo/McVertex.h"
#include "GlastEvent/MonteCarlo/McParticle.h"
#include "data/IVetoData.h"
// for access to instrument.ini
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "xml/IFile.h"

// access to the id classes
#include "idents/AcdId.h"


#include <cassert>

// Define the class here instead of in a header file: not needed anywhere but here!
//------------------------------------------------------------------------------
/** 
This is Steve Ritz's userAlg code, which develops a preliminary algoritm for throttling the ACD.
Note that there is some redundancy with level1: it should be integrated.
<br> -TB

  
*/
class ACDthrottle : public Algorithm {
public:
    ACDthrottle(const std::string& name, ISvcLocator* pSvcLocator);
    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();
    
private: 
    //! routine to extract parameters from the instrument.xml file
    void getParameters();
    //! setup our ntuple
    StatusCode fillNtuple();
    //! a little routine to sum the energy deposited in the ACD tiles
    float calcTotEnergy(const IVetoData* acdDigiData);
    //! find three in a row for a given tower
    bool threeInARow(const SiData* tkrDigiData, unsigned int towerId);
    //! calculate the bit pattern in a TKR tower for XY layer coincidences
	unsigned long TKRtowerbitpattern(const SiData* tkrDigiData, unsigned int towerId);
    //! the GlastDetSvc used for access to detector info
    IGlastDetSvc*    m_detSvc; 
    //! constants from the "instrument.xml" file.
    xml::IFile * m_ini; 
    //! parameters that can be retrieved from the instrument.xml file
    int m_numTowers, m_xNumTowers, m_yNumTowers, m_xNumTopTiles, m_yNumTopTiles, m_nplanes;
	double m_veto_threshold;
    double m_mod_width;

    // elements to store in the ntuple
    float m_nhitface0,m_nhitface1, m_nhitface2, m_nhitface3, m_nhitface4;
	float m_nhitsiderow0, m_nhitsiderow1, m_nhitsiderow2, m_nhitsiderow3;
	float m_ntothit;
	float m_L1T, m_vetoword,m_bitpattern; 

    unsigned int assoctile[16][4];
    unsigned int assoctileside[16][4];

    //! access the ntupleWriter service to write out to ROOT ntuples
    INTupleWriterSvc *m_ntupleWriteSvc;
    //! parameter to store the logical name of the ROOT file to write to
    std::string m_tupleName;
};
//------------------------------------------------------------------------

// necessary to define a Factory for this algorithm
// expect that the xxx_load.cxx file contains a call     
//     DLL_DECL_ALGORITHM( ACDthrottle );

static const AlgFactory<ACDthrottle>  Factory;
const IAlgFactory& ACDthrottleFactory = Factory;

//------------------------------------------------------------------------
//! ctor
ACDthrottle::ACDthrottle(const std::string& name, ISvcLocator* pSvcLocator)
:Algorithm(name, pSvcLocator)
, m_detSvc(0), m_ini(0)
{
    // declare properties with setProperties calls
    declareProperty("tupleName",  m_tupleName="");
    
}
//------------------------------------------------------------------------
//! set parameters and attach to various perhaps useful services.
StatusCode ACDthrottle::initialize(){
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "initialize" << endreq;
    
    // Use the Job options service to set the Algorithm's parameters
    setProperties();

    if( m_tupleName.empty()) {log << MSG::ERROR << "tupleName property not set!"<<endreq;
        return StatusCode::FAILURE;}
    // now try to find the GlastDevSvc service
    if (service("GlastDetSvc", m_detSvc).isFailure()){
        log << MSG::ERROR << "Couldn't find the GlastDetSvc!" << endreq;
        return StatusCode::FAILURE;
    }
    // get the ini file
    m_ini = const_cast<xml::IFile*>(m_detSvc->iniFile()); //OOPS!
    assert(4==m_ini->getInt("glast", "xNum"));  // simple check

    // Call our routine to extract parameters from the instrument.xml file
    getParameters();

    // setup our ntuple...
//    setupNtuple();


    // set up the array of associated acd front tiles
	assoctile[0][0] = 0x0000;
	assoctile[0][1] = 0x0001;
	assoctile[0][2] = 0x0010;
	assoctile[0][3] = 0x0011;
    int itow, itil;
	for (itow=1;itow<=3;itow++){
		for (itil=0;itil<=3;itil++){
          assoctile[itow][itil]=assoctile[itow-1][itil]+0x0001;
		}
	}
	for (itow=4;itow<=15;itow++){
		for (itil=0;itil<=3;itil++){
          assoctile[itow][itil]=assoctile[itow-4][itil]+0x0010;
		}
	}
//	for (itow=8;itow<=11;itow++){
//		for (itil=0;itil<=3;itil++){
//         assoctile[itow][itil]=assoctile[itow-4][itil]+0x0010;
//		}
//	}
//	for (itow=12;itow<=15;itow++){
//		for (itil=0;itil<=3;itil++){
//        assoctile[itow][itil]=assoctile[itow-4][itil]+0x0010;
//		}
//	}
    // set up the array of associated acd side tiles
	for (itow=0;itow<=15;itow++){
		for (itil=0;itil<=3;itil++){
          assoctileside[itow][itil]=0x0999;
		}
	}
	assoctileside[0][0] = 0x0100;
	assoctileside[0][1] = 0x0101;
	assoctileside[0][2] = 0x0200;
	assoctileside[0][3] = 0x0201;
      assoctileside[1][0] = 0x0201;
      assoctileside[1][1] = 0x0202;
      assoctileside[2][0] = 0x0202;
      assoctileside[2][1] = 0x0203;
      assoctileside[3][0] = 0x0203;
      assoctileside[3][1] = 0x0204;
      assoctileside[3][2] = 0x0300;
      assoctileside[3][3] = 0x0301;
      assoctileside[7][0] = 0x0301;
      assoctileside[7][1] = 0x0302;
      assoctileside[11][0] = 0x0302;
      assoctileside[11][1] = 0x0303;
      assoctileside[15][0] = 0x0303;
      assoctileside[15][1] = 0x0304;
      assoctileside[15][2] = 0x0404;
      assoctileside[15][3] = 0x0403;
      assoctileside[14][0] = 0x0403;
      assoctileside[14][1] = 0x0402;
      assoctileside[13][0] = 0x0402;
      assoctileside[13][1] = 0x0401;
      assoctileside[12][0] = 0x0401;
      assoctileside[12][1] = 0x0400;
      assoctileside[12][2] = 0x0104;
      assoctileside[12][3] = 0x0103;
      assoctileside[8][0] = 0x0103;
      assoctileside[8][1] = 0x0102;
      assoctileside[4][0] = 0x0102;
      assoctileside[4][1] = 0x0101;
//
//    now let's whack a tile to study increase in rate
//
//    unsigned int iwhack = 0x0000;
//	for (itow=0;itow<=15;itow++){
//		for (itil=0;itil<=3;itil++){
//          if (assoctile[itow][itil]== iwhack) assoctile[itow][itil]=0x0999;
//		}
//	}
//
    // get a pointer to our ntupleWriterSvc
    if (service("ntupleWriterSvc", m_ntupleWriteSvc).isFailure()) {
        log << MSG::ERROR << "writeJunkAlg failed to get the ntupleWriterSvc" << endreq;
        return StatusCode::FAILURE;
    }

    //setupNtuple();
    return sc;
}

//------------------------------------------------------------------------
//! process an event
StatusCode ACDthrottle::execute()
{
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );
    
    /*! Causes the TDS to be searched, if the data is unavailable, the appropriate
    converter is called to retrieve the data from some persistent store, in this
    case from an IRF. 
    This call asks for the TdGlastData pointer to be given the variable name glastData.  
    We are asking the event data service, eventSvc(), to provide the data located at 
    "/Event/TdGlastData" which denotes the location of the glast detector data in 
    our Gaudi Event Model.
    */

    // Get the pointer to the event
    SmartDataPtr<TdGlastData> glastData(eventSvc(),"/Event/TdGlastData");
    m_L1T = 0.;

    // retrieve TKR data pointer for the event
    const SiData *tkrDigiData = glastData->getSiData();
    if (tkrDigiData == 0) {
        log << MSG::INFO << "No TKR Data available" << endreq;
    } else {
        // find a Tower with three in a row
        bool anyTower = false;
        unsigned int iTower;
        for (iTower = 0; iTower < m_numTowers-1; iTower++) {
            if (threeInARow(tkrDigiData, iTower) == true) {
                anyTower = true;
                break;
            }
        }
        if (anyTower) m_L1T=1.;
    }


    // retrieve CAL data pointer
    const CsIData *calDigiData = glastData->getCsIData();
    if (calDigiData == 0) {   
        log << MSG::INFO << "No CAL Data available" << endreq;
    } else {
		// check for CAL-Hi and CAL-Lo
		bool CALHI = false;
		bool CALLO = false;
	    // skip this for now.  Wait until L1T properly implemented.  Don't need it for L2 studies yet.
	}

    // retrieve the ACD data pointer
    const IVetoData *acdDigiData = glastData->getVetoData();
    // Check to see if there is any ACD data for this event - if not provide a message
    if (acdDigiData == 0) log << MSG::INFO << "No ACD Data available" << endreq;

    // if we have ACD data, calculate the number of hit tiles by face and row
          // initialize counters number of tiles on each face
         int nhitface[5] = {0.,0.,0.,0.,0.};
		 // initialize counters number of tiles in each row on the sides
		 int nhitsiderow[5] = {0.,0.,0.,0.,0.};
         unsigned long bitpattern;
         unsigned long bitpatternall = 0;
         int ntothit =0.; // this is a check on recon only
		 if (acdDigiData != 0) {
	     enum {
        _layermask = 0x0800,
        _facemask  = 0x0700,
        _rowmask   = 0x00F0,
        _colmask   = 0x000F
		 };
         for( IVetoData::const_iterator it= acdDigiData->begin(); it != acdDigiData->end(); ++it) {
		  if ((*it).energy() > m_veto_threshold){
              // add up tiles by face and by row on the side
			ntothit += 1;
			unsigned int face = ((*it).type() & _facemask)>>8;
			nhitface[face] += 1;
			if (face != 0) {
				unsigned int row = ((*it).type() & _rowmask)>>4;
				nhitsiderow[row] += 1;
			}
		  }
		}
 
	// now let's examine those L1Ts that easily match ACD tiles
//  get the bit pattern by tower
    unsigned int vetoword = 0;
	if (tkrDigiData == 0) {
        log << MSG::INFO << "No TKR Data available" << endreq;
    } else {
        unsigned int iTower;
        for (iTower = 0; iTower < m_numTowers-1; iTower++) {
          bitpattern = TKRtowerbitpattern(tkrDigiData,iTower);
          bitpatternall = bitpatternall | bitpattern;
          if (bitpattern > 0) {
          if ((bitpattern & 7) == 7) {
	     // it's a top-layer conversion trigger
           // loop over hit acd tiles and check if one is associated with this tower
           // just the top guys for now			  
            for( IVetoData::const_iterator it= acdDigiData->begin(); it != acdDigiData->end(); ++it) {
				if ((*it).energy() > m_veto_threshold){
					unsigned int tileno = (*it).type();
					int itile;
					for (itile = 0;itile<=3;itile++){
						if (tileno == assoctile[iTower][itile]) vetoword |= 1;
//				  if (assoctile[iTower][itile]==0x0999) {
//   		            m_guiSvc->guiMgr()->setState(gui::GuiMgr::PAUSED);
//				  }
					}
				}
			}
		  } else {
			  // not a top conversion.  let's check the side tiles for this tower
			  unsigned int iPlane;
			  for (iPlane = 1; iPlane < m_nplanes-2; iPlane++) {
				  unsigned long mask = (7 << iPlane);
				  if ((bitpattern & mask) == mask) {
					  // we found the first XY coincidence for the 3-in-a-row.  
					  // Look at side ACD tiles in the appropriate row.
					  for( IVetoData::const_iterator it= acdDigiData->begin(); it != acdDigiData->end(); ++it) {
						  if ((*it).energy() > m_veto_threshold){
							  unsigned int tileno = (*it).type();
							  int itile;
							  for (itile = 0;itile<=3;itile++){
								  int rowassoc = 0;
								  if (iPlane > 3) rowassoc = 1;
                                  if (iPlane > 10) rowassoc = 2;
                                  if (iPlane >14) rowassoc = 3;
                                  if (tileno == (assoctileside[iTower][itile]+rowassoc*0x0010)) {
                                      vetoword |= 0x0010*(1<<rowassoc);  // record in vetoword which row in ACD vetos
                                      iPlane = m_nplanes+1;  //break out of this big loop.  CHECK THIS.
                                      break; //  break out of this inner loop
                                  }
                                  // check wider angles if nothing has been found yet
                                  rowassoc = 0;
                                  if (iPlane > 10) rowassoc = 1;
                                  if (iPlane > 14) rowassoc = 2;
                                  if (tileno == (assoctileside[iTower][itile]+rowassoc*0x0010)) {
                                      vetoword |= 0x0010*(1<<rowassoc);  // record in vetoword which row in ACD vetos
                                      iPlane = m_nplanes+1;  //break out of this big loop.  CHECK THIS.
                                      break; //  break out of this inner loop
                                  }
                              }
                          }
                          if (iPlane > m_nplanes) break; // check to see if we're done with loop
                      }
                  } 
              }
          }
          } 
        }
    }

    // At the end - we fill the tuple with the data from this event
    m_vetoword = vetoword;
//    if (vetoword>0&&vetoword<128&&m_L1T>0.) {   // just a place to break conveniently
//		m_guiSvc->guiMgr()->setState(gui::GuiMgr::PAUSED);
//	}
	m_nhitface0 = nhitface[0];
	m_nhitface1 = nhitface[1];
	m_nhitface2 = nhitface[2];
	m_nhitface3 = nhitface[3];
	m_nhitface4 = nhitface[4];
	m_nhitsiderow0 = nhitsiderow[0];
	m_nhitsiderow1 = nhitsiderow[1];
	m_nhitsiderow2 = nhitsiderow[2];
	m_nhitsiderow3 = nhitsiderow[3];
    m_ntothit = ntothit;
    m_bitpattern = bitpatternall;
    //	
   }

	sc = fillNtuple();
    return sc;
}

//------------------------------------------------------------------------
//! clean up, summarize
StatusCode ACDthrottle::finalize(){
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream log(msgSvc(), name());
    
    return sc;
}


/// Retrieve parameters from the instrument.xml file
void ACDthrottle::getParameters ()
{

    MsgStream   log( msgSvc(), name() );

    m_yNumTowers = m_ini->getInt("glast", "yNum");
    m_xNumTowers = m_ini->getInt("glast", "xNum");

    m_numTowers = (m_yNumTowers * m_xNumTowers);

    m_nplanes = m_ini->getInt("tracker", "num_trays");
    --m_nplanes;  // # of planes is one less than # of trays

    m_xNumTopTiles = m_ini->getInt("veto", "numXtiles");
    m_yNumTopTiles = m_ini->getInt("veto", "numYtiles");
	m_veto_threshold = m_ini->getDouble("veto", "threshold");

    m_mod_width = m_ini->getDouble("glast", "mod_width");

    log << MSG::INFO << "Retrieved data from the instrument.xml file" << endreq;
    log << MSG::INFO << "The number of towers in X = " << m_xNumTowers << endreq;
    
}

StatusCode ACDthrottle::fillNtuple() {
    StatusCode  sc = StatusCode::SUCCESS;
    if (m_tupleName.empty()) return sc;
 
	   // Here we are adding to our ROOT ntuple
   
    // setup the entries to our tuple
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "vetoword", m_vetoword)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitface0", m_nhitface0)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitface1", m_nhitface1)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitface2", m_nhitface2)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitface3", m_nhitface3)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitface4", m_nhitface4)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitsiderow0", m_nhitsiderow0)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitsiderow1", m_nhitsiderow1)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitsiderow2", m_nhitsiderow2)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "nhitsiderow3", m_nhitsiderow3)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "ntothit", m_ntothit)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "L1T", m_L1T)).isFailure()) return sc;
    if ((sc = m_ntupleWriteSvc->addItem(m_tupleName.c_str(), "TKRpattern", m_bitpattern)).isFailure()) return sc;

    return sc;

}


// just a little routine to demonstrate manipulating the ACD digi data.
float ACDthrottle::calcTotEnergy(const IVetoData *acdDigiData) 
{
    double totEnergy = 0.0;
    for( IVetoData::const_iterator it= acdDigiData->begin(); it != acdDigiData->end(); ++it) {
        totEnergy += (*it).energy();
    }
    return totEnergy;
}

// check to see if there is three In a Row in a particular Tower
bool ACDthrottle::threeInARow(const SiData *tkrDigiData, unsigned int towerId) 
{
    unsigned int iPlane;
    unsigned long bitString = TKRtowerbitpattern(tkrDigiData,towerId);
    for (iPlane = 0; iPlane < m_nplanes; iPlane++) {
        unsigned long mask = (7 << iPlane);
        if ((bitString & mask) == mask) return true;
    }
    return false;
}
// produce the bit pattern of XY coincidences by plane
unsigned long ACDthrottle::TKRtowerbitpattern(const SiData *tkrDigiData, unsigned int towerId) 
{
    unsigned long bitString = 0;
    unsigned int iPlane;
    for (iPlane = 0; iPlane < m_nplanes; iPlane++) {
        int nx = tkrDigiData->nHits(SiData::X, iPlane);
        int ny = tkrDigiData->nHits(SiData::Y, iPlane);

        if ( (nx > 0) && (ny > 0) ) {  // we have hits in both x & y

            bool foundXinTower = false;
            unsigned iXhit;
            for (iXhit = 0; iXhit < nx; iXhit++) {
                if (towerId == tkrDigiData->moduleId(SiData::X, iPlane, iXhit)) {
                    foundXinTower = true;
                    break;
                }
            }

            bool foundYinTower = false;
            unsigned iYhit;
            for (iYhit = 0; iYhit < ny; iYhit++) {
                if (towerId == tkrDigiData->moduleId(SiData::Y, iPlane, iYhit)) {
                    foundYinTower = true;
                    break;
                }
            }

            if (foundXinTower && foundYinTower) bitString |= (1 << iPlane);

        } else {  // do not have hits in both x & y..continue to the next plane
            continue;
        }
    }
    return bitString;
}