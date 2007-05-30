/*
@file TrgConfigSvc.cxx

@brief keeps track of the GEM trigger configuration
@author Martin Kocian

$Header: $

*/

#include "TrgConfigSvc.h"

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/SvcFactory.h"

#include "GaudiKernel/SmartDataPtr.h"
#include "LdfEvent/LsfMetaEvent.h"
//#include "configData/db/LatcDBImplOld.h"
#include "configData/db/LatcDBImplFile.h"
#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "GaudiKernel/IIncidentSvc.h"
#include <stdlib.h>



// declare the service factories for the TrgConfigSvc
static SvcFactory<TrgConfigSvc> a_factory;
const ISvcFactory& TrgConfigSvcFactory = a_factory; 

TrgConfigSvc::TrgConfigSvc(const std::string& name,ISvcLocator* svc) 
  : Service(name,svc),m_eventread(true),m_configchanged(false)
{
    // Purpose and Method: Constructor - Declares and sets default properties
    //                     
    // Inputs: service name and locator 
    //         
    // Outputs: None
    // Dependencies: None
    // Restrictions and Caveats:  None

    // declare the properties

    declareProperty("configureFromFile",       m_configureFromFile=false);
    declareProperty("xmlFile", m_xmlFile="");
    declareProperty("configureFromMOOT",    m_configureFromMoot=false);
    declareProperty("useKeyFromData",  m_useKeyFromData=true);
    declareProperty("fixedKey",  m_fixedKey=1852);
    declareProperty("useDefaultConfiguration",  m_useDefaultConfiguration=true);
}

StatusCode  TrgConfigSvc::queryInterface (const InterfaceID& riid, void **ppvIF)
{
    if (IID_ITrgConfigSvc == riid) {
        *ppvIF = dynamic_cast<ITrgConfigSvc*> (this);
        return StatusCode::SUCCESS;
    } else {
        return Service::queryInterface (riid, ppvIF);
    }
}

const InterfaceID&  TrgConfigSvc::type () const {
    return IID_ITrgConfigSvc;
}

StatusCode TrgConfigSvc::initialize () 
{
    // Purpose and Method: Initialize the lists of dead units
    //                     
    // Inputs: None        
    // Outputs: None
    // Dependencies: None
    // Restrictions and Caveats:  None

    StatusCode  sc = StatusCode::SUCCESS;

    Service::initialize();

    // Open the message log
    MsgStream log( msgSvc(), name() );
    // Event service
    if ((sc = service("EventDataSvc",m_eventSvc,true)).isFailure())
      { 
        log << MSG::ERROR << "Failed to find event data service" << endreq;
	return StatusCode::FAILURE;
      }

    // Bind all of the properties for this service
    if ( (sc = setProperties()).isFailure() ) {
        log << MSG::ERROR << "Failed to set properties" << endreq;
	return StatusCode::FAILURE;
    }
    if((int)m_configureFromFile+(int)m_configureFromMoot+(int)m_useDefaultConfiguration!=1){
      log << MSG::ERROR << "Pick exactly one configuration source out of MOOT, file, and Default" << endreq;
      return StatusCode::FAILURE;
    }
    if (m_configureFromMoot && m_useKeyFromData){
      //temporary fix to disable MOOT config 
      log<<MSG::ERROR<<"Configuring from MOOT is disabled in this version for lack of mootCore"<<endreq;
      return StatusCode::FAILURE;
      // Get ready to listen for BeginEvent
      IIncidentSvc* incSvc;
      sc = service("IncidentSvc", incSvc, true);
      if (sc.isSuccess() ) {
	incSvc->addListener(this, "BeginEvent");
      } else {
	log << MSG::ERROR << "can't find IncidentSvc" << endreq;
	return sc;
      }
    }


    if (m_configureFromMoot){
      //temporary fix to disable MOOT config 
      log<<MSG::ERROR<<"Configuring from MOOT is disabled in this version for lack of mootCore"<<endreq;
      return StatusCode::FAILURE;
      //m_trgconfig=new TrgConfigDB(new LatcDBImplOld);
      //if(m_useKeyFromData==false)m_trgconfig->updateKey(m_fixedKey);
    }else{
      LatcDBImplFile* fc=new LatcDBImplFile;
      if (m_useDefaultConfiguration){
	std::string xmlfile=std::string(getenv("CONFIGDATAROOT"))+"/src/defaultTrgConfig.xml";
	log << MSG::INFO << "Using default GEM configuration from "<<xmlfile<< endreq;
	fc->setFilename(xmlfile);
      }else{
	fc->setFilename(m_xmlFile);
	// update the object only once with a non-zero key
	log << MSG::INFO << "Configuring GEM object from file "<<m_xmlFile<< endreq;
      }
      m_trgconfig=new TrgConfigDB(fc);
      m_trgconfig->updateKey(1);
    }
    return sc;
}

const TrgConfig* TrgConfigSvc::getTrgConfig() 
{
  if(m_configureFromMoot && m_useKeyFromData && !m_eventread){
    //temporary fix to disable MOOT config 
    MsgStream log( msgSvc(), name() );
    log<<MSG::ERROR<<"Configuring from MOOT is disabled in this version for lack of mootCore"<<endreq;
    assert(0);
    /*
    m_eventread=true;
    
    // update the configuration 
    SmartDataPtr<LsfEvent::MetaEvent> meta(eventSvc(), "/Event/MetaEvent");
    unsigned int latckey=meta->keys()->LATC_master();
    //    std::cout<<"Latc key "<<latckey<<std::endl;
    m_configchanged=m_trgconfig->updateKey(latckey);
    */
  }
  return m_trgconfig;
 
}

const bool TrgConfigSvc::configChanged(){
  if(!m_eventread)getTrgConfig();
  return m_configchanged;
}
		    
// handle "incidents"
void TrgConfigSvc::handle(const Incident &inc)
{
  if( inc.type()=="BeginEvent")m_eventread=false;
}


StatusCode TrgConfigSvc::finalize( ) {
    MsgStream log(msgSvc(), name());  
    return StatusCode::SUCCESS;
}
