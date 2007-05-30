/** @file ITrgConfigSvc.h
@brief Abstract interface to TrgConfigSvc (q.v.)
@author Leon Rochester

$Header: $
*/

#ifndef ITrgConfigSvc_H
#define ITrgConfigSvc_H

// Include files
#include "GaudiKernel/IInterface.h"
#include "configData/gem/TrgConfig.h"


// Declaration of the interface ID ( interface id, major version,
// minor version)

static const InterfaceID IID_ITrgConfigSvc("ITrgConfigSvc", 1 , 1);

/** @class ITrgConfigSvc
* @brief Interface class for TrgConfigSvc
*
* Author:  M. Kocian
*
*/


class ITrgConfigSvc : virtual public IInterface {

public:
    static const InterfaceID& interfaceID() { return IID_ITrgConfigSvc; }

    /// get the GEM configuration object
    virtual const TrgConfig*  getTrgConfig() = 0;
    /// has the trigger configuration changed for this event?
    virtual const bool  configChanged() = 0;

};

#endif // ITrgConfigSvc_H

