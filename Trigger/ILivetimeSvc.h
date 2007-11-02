/** 
* @file ILivetimeSvc.h
* @brief definition of the interface for ILivetimeSvc
*
*  $Header: /nfs/slac/g/glast/ground/cvs/Trigger/Trigger/ILivetimeSvc.h,v 1.2 2006/03/14 05:55:10 burnett Exp $
*/
#ifndef _H_ILivetimeSvc
#define _H_ILivetimeSvc

// includes
#include "GaudiKernel/IInterface.h"
#include "enums/GemState.h"


// Declaration of the interface ID ( interface id, major version, minor version) 
static const InterfaceID IID_ILivetimeSvc("ILivetimeSvc", 1, 0); 

/** 
* \class ILivetimeSvc
* \brief The  gaudi service interface
*
* \author Toby Burnett tburnett@u.washington.edu

Livetime service interface
* 
*/
class  ILivetimeSvc : virtual public IInterface {
public:

    /// Retrieve interface ID
    static const InterfaceID& interfaceID() { return IID_ILivetimeSvc; }


    ///check if valid trigger, and set last trigger time if valid
    virtual bool tryToRegisterEvent(double current_time, bool longdeadtime)=0;

    ///check if valid trigger
    virtual bool isLive(double current_time)=0;

    ///check state at time current_time
    virtual enums::GemState checkState(double current_time)=0;

    virtual double livetime()=0; ///< return the accumulated livetime

    /// get number of GEM clock ticks
    virtual unsigned int ticks(double time) const=0;
};

#endif  // _H_ILivetimeSvc
