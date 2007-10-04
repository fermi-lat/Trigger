/*
@file TrgConfigSvc.cxx

@brief keeps track of the GEM trigger configuration
@author Martin Kocian

$Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/TrgConfigSvc.cxx,v 1.2 2007/08/14 16:01:36 heather Exp $

*/

#include "TrgConfigSvc.h"

#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/SvcFactory.h"

#include "GaudiKernel/SmartDataPtr.h"
#include "LdfEvent/LsfMetaEvent.h"
//#include "configData/db/LatcDBImplOld.h"
#include "configData/db/LatcDBImpl.h"
#include "configData/db/LatcDBImplFile.h"
#include "Event/TopLevel/EventModel.h"
#include "Event/TopLevel/Event.h"
#include "GaudiKernel/IIncidentSvc.h"
#include <stdlib.h>
#include <assert.h>


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

    declareProperty("configureFrom",       m_configureFrom="Default");// options are Default, File, Moot
    declareProperty("xmlFile", m_xmlFile="");
    declareProperty("useKeyFromData",  m_useKeyFromData=true);
    declareProperty("fixedKey",  m_fixedKey=1852);
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
    if(m_configureFrom.value()!="Default"&&m_configureFrom.value()!="Moot"&&m_configureFrom.value()!="File"){
      log << MSG::ERROR << "Pick exactly one configuration source out of Moot, File, and Default" << endreq;
      return StatusCode::FAILURE;
    }
    // print a message about the configuration
    log<<MSG::INFO<<"Configuration taken from "<<m_configureFrom.value()<<endreq;
    if (m_configureFrom.value()=="Moot"){
      if( m_useKeyFromData)log<<MSG::INFO<<"Using MOOT key from data"<<endreq;
      else log<<MSG::INFO<<"Using fixed MOOT key "<<m_fixedKey<<endreq;
    }

    if (m_configureFrom.value()=="Moot" && m_useKeyFromData){
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


    if (m_configureFrom.value()=="Moot"){
      m_trgconfig=new TrgConfigDB(new LatcDBImpl);
      if(m_useKeyFromData==false)m_trgconfig->updateKey(m_fixedKey);
    }else{
      LatcDBImplFile* fc=new LatcDBImplFile;
      if (m_configureFrom.value()=="Default"){
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
  if(m_configureFrom.value()=="Moot" && m_useKeyFromData && !m_eventread){
    MsgStream log( msgSvc(), name() );
    m_eventread=true;
    
    // update the configuration 
    SmartDataPtr<LsfEvent::MetaEvent> meta(eventSvc(), "/Event/MetaEvent");
    unsigned int latckey=meta->keys()->LATC_master();
    //std::cout<<"Latc key "<<latckey<<std::endl;
    m_configchanged=m_trgconfig->updateKey(latckey);
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
