// $Header: /nfs/slac/g/glast/ground/cvs/trigger/src/TriggerAlgAlg.cxx,v 1.1.1.1 2001/05/23 14:56:24 burnett Exp $

// Include files
// Gaudi system includes
#include "GaudiKernel/MsgStream.h"
#include "GaudiKernel/AlgFactory.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/Algorithm.h"

// Gaudi ntuple interface
#include "GaudiKernel/INTupleSvc.h"
#include "GaudiKernel/INTuple.h"
#include "GaudiKernel/NTuple.h"


// TDS class declarations: input data,
#include "GlastEvent/data/TdGlastData.h"

// for access to instrument.ini
#include "GlastSvc/GlastDetSvc/IGlastDetSvc.h"
#include "xml/IFile.h"

#include <cassert>

// Define the class here instead of in a header file: not needed anywhere but here!
//------------------------------------------------------------------------------
/** 

  
*/
class TriggerAlg : public Algorithm {
public:
    TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator);
    StatusCode initialize();
    StatusCode execute();
    StatusCode finalize();
    
private: 
    //! the GlastDetSvc used for access to detector info
    IGlastDetSvc*    m_detSvc; 
    //! constants from the "instrument.xml" file.
    xml::IFile * m_ini; 
    
};
//------------------------------------------------------------------------

// necessary to define a Factory for this algorithm
// expect that the xxx_load.cxx file contains a call     
//     DLL_DECL_ALGORITHM( TriggerAlg );

static const AlgFactory<TriggerAlg>  Factory;
const IAlgFactory& TriggerAlgFactory = Factory;

//------------------------------------------------------------------------
//! ctor
TriggerAlg::TriggerAlg(const std::string& name, ISvcLocator* pSvcLocator)
:Algorithm(name, pSvcLocator)
, m_detSvc(0), m_ini(0)
{
    // declare properties with setProperties calls
    
}
//------------------------------------------------------------------------
//! set parameters and attach to various perhaps useful services.
StatusCode TriggerAlg::initialize(){
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream log(msgSvc(), name());
    log << MSG::INFO << "initialize" << endreq;
    
    // Use the Job options service to set the Algorithm's parameters
    setProperties();

    // now try to find the GlastDevSvc service
    if (service("GlastDetSvc", m_detSvc).isFailure()){
        log << MSG::ERROR << "Couldn't find the GlastDetSvc!" << endreq;
        return StatusCode::FAILURE;
    }
    // get the ini file
    m_ini = const_cast<xml::IFile*>(m_detSvc->iniFile()); //OOPS!
    assert(4==m_ini->getInt("glast", "xNum"));  // simple check


    return sc;
}

//------------------------------------------------------------------------
//! process an event
StatusCode TriggerAlg::execute()
{
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream   log( msgSvc(), name() );
    
    return sc;
}

//------------------------------------------------------------------------
//! clean up, summarize
StatusCode TriggerAlg::finalize(){
    StatusCode  sc = StatusCode::SUCCESS;
    MsgStream log(msgSvc(), name());
    
    return sc;
}



