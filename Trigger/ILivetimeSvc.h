/** 
* @file ILivetimeSvc.h
* @brief definition of the interface for ILivetimeSvc
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/Trigger/Attic/ILivetimeSvc.h,v 1.1.2.1 2006/01/08 01:52:14 burnett Exp $
*/
#ifndef _H_ILivetimeSvc
#define _H_ILivetimeSvc

// includes
#include "GaudiKernel/IInterface.h"


// Declaration of the interface ID ( interface id, major version, minor version) 
static const InterfaceID IID_ILivetimeSvc("ILivetimeSvc", 1, 0); 

/** 
* \class ILivetimeSvc
* \brief The  gaudi service interface
*
* \author Toby Burnett tburnett@u.washington.edu

The livetime service has two functions: 

# for a given time, is the system live?
# accumulate the live time for a run

* 
*/
class  ILivetimeSvc : virtual public IInterface {
public:

    /// Retrieve interface ID
    static const InterfaceID& interfaceID() { return IID_ILivetimeSvc; }


    ///check if valid trigger, and set last trigger time
    virtual bool isLive(double current_time)=0;

    virtual double livetime()=0; ///< return the accumulated livetime

    /// set a new trigger rate, return the old value
    virtual double setTriggerRate(double rate)=0;

};

#endif  // _H_ILivetimeSvc
