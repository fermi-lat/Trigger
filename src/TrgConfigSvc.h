/*
@file TrgConfigSvc.h

@brief provides the GEM configuration
@author Martin Kocian

$Header:  $

*/
#ifndef TrgConfigSvc_H
#define TrgConfigSvc_H 

// Include files
#include "Trigger/ITrgConfigSvc.h"
#include "GaudiKernel/Service.h"
#include "GaudiKernel/IDataProviderSvc.h"
#include "configData/db/TrgConfigDB.h"
#include "GaudiKernel/IIncidentListener.h"
#include "GaudiKernel/Property.h"

/** @class TrgConfigSvc
* @brief Service to retrieve the GEM configuration
*
* Author:  M. Kocian
*
*/

class TrgConfigSvc : virtual public Service, virtual public IIncidentListener, virtual public ITrgConfigSvc  {

public:

    TrgConfigSvc(const std::string& name, ISvcLocator* pSvcLocator); 

    StatusCode initialize();
    StatusCode finalize();
    /// Handles incidents, implementing IIncidentListener interface
    virtual void handle(const Incident& inc);    



    /// queryInterface - for implementing a Service this is necessary
    StatusCode queryInterface(const InterfaceID& riid, void** ppvUnknown);

    static const InterfaceID& interfaceID() {
        return ITrgConfigSvc::interfaceID(); 
    }

    /// return the service type
    const InterfaceID& type() const;

    /// get the GEM configuration object
    const TrgConfig* getTrgConfig() ;
    const bool configChanged();
    
private:
    IDataProviderSvc*       eventSvc()       const {return m_eventSvc;}
    TrgConfigDB* m_trgconfig;
    BooleanProperty m_configureFromFile;
    StringProperty m_xmlFile;
    BooleanProperty m_configureFromMoot;
    BooleanProperty m_useKeyFromData;
    UnsignedIntegerProperty m_fixedKey;
    BooleanProperty m_useDefaultConfiguration;
    IDataProviderSvc * m_eventSvc ;
    bool m_eventread;
    bool m_configchanged;
};


#endif // TrgConfigSvc_H

