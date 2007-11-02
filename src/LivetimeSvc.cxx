/** 
 * @file LivetimeSvc.cxx
 * @brief declare, implement the class LivetimeSvc
 *
 * $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/LivetimeSvc.cxx,v 1.5 2007/08/16 20:37:22 burnett Exp $
 */

#include "Trigger/ILivetimeSvc.h"


#include "CLHEP/Random/RandFlat.h"
#include "CLHEP/Random/RandPoisson.h"

#include "GaudiKernel/Service.h"
#include "GaudiKernel/SvcFactory.h"
#include "GaudiKernel/Property.h"
#include "GaudiKernel/SmartDataPtr.h"
#include "GaudiKernel/MsgStream.h"


/**@class LivetimeSvc
   @brief implement ILivetimeSvc interface

   Manage livetime
*/
class LivetimeSvc :  public Service, 
        virtual public ILivetimeSvc

{  

public:

    /// perform initializations for this service - required by Gaudi
    virtual StatusCode initialize ();

    /// clean up after processing all events - required by Gaudi
    virtual StatusCode finalize ();

    /// Query interface - required of all Gaudi services
    virtual StatusCode queryInterface( const InterfaceID& riid, void** ppvUnknown );


    ///check if valid trigger, and set last trigger time if valid
    virtual bool tryToRegisterEvent(double current_time, bool highdeadtime);

    ///check if valid trigger
    virtual bool isLive(double current_time);
    // check state at time current_time
    virtual enums::GemState checkState(double current_time);


    virtual double livetime(); ///< return the accumulated livetime

/**
 *
 *  @fn     unsigned int ticks (double time)
 *  @brief  Returns the low 32 bits of the number of elapsed ticks
 *          of the LAT system clock
 **/
  unsigned int ticks(double time) const;
   
private:
    /// Allow only SvcFactory to instantiate the service.
    friend class SvcFactory<LivetimeSvc>;

    LivetimeSvc ( const std::string& name, ISvcLocator* al );

    DoubleProperty m_deadtime; ///< deadtime per trigger (nominal 25 us)
    DoubleProperty m_deadtimelong; ///< deadtime per trigger for large events
    DoubleProperty m_frequency; ///< background trigger rate
    double m_livetime; ///< update with request
    double m_totalTime;

    int m_total, m_accepted;
    double m_lastTriggerTime;
    double m_previousDeadtime; 
    enum state {live, deadzone, busy}; 
    double m_deadzoneTime;
};
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// declare the service factories for the ntupleWriterSvc
static SvcFactory<LivetimeSvc> a_factory;
const ISvcFactory& LivetimeSvcFactory = a_factory;
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//         Implementation of LivetimeSvc methods
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
LivetimeSvc::LivetimeSvc(const std::string& name,ISvcLocator* svc)
: Service(name,svc)
, m_livetime(0)
, m_totalTime(0)
, m_total(0)
, m_accepted(0)
, m_lastTriggerTime(0)
, m_previousDeadtime(0)
{
    // declare the properties and set defaults
    declareProperty("Deadtime",    m_deadtime=26.4e-6 );  // deadtime to apply to trigger, in sec.
    declareProperty("DeadtimeLong", m_deadtimelong=65.0e-6); // deadtime for large events
    declareProperty("clockrate", m_frequency=20e6);  // 20 MHz clock

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StatusCode LivetimeSvc::initialize () 
{
    StatusCode  status =  Service::initialize ();

    // bind all of the properties for this service
    setProperties ();
    // open the message log
    MsgStream log( msgSvc(), name() );
    m_deadzoneTime=100e-9; // 2 clock tick dead zone
    
    return status;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LivetimeSvc::tryToRegisterEvent(double current_time, bool highdeadtime)
{ 
   ++m_total;

   if (m_deadtime<=0) return true;

   bool live = current_time-m_lastTriggerTime >= m_previousDeadtime;
   if( live ){
     // here if valid. Update the livetime
     if( m_lastTriggerTime>0){
       double elapsed(current_time-m_lastTriggerTime);
       m_livetime += elapsed-m_previousDeadtime;
       
       m_totalTime+= elapsed;
     }
     m_lastTriggerTime = current_time;
     if (highdeadtime){
       m_previousDeadtime=m_deadtimelong;
     }else{
       m_previousDeadtime=m_deadtime;
     }
     ++m_accepted;
   }
   return live;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LivetimeSvc::isLive(double current_time)
{
  if (m_deadtime<=0) return true;
  bool live = current_time-m_lastTriggerTime >= m_previousDeadtime;
  return live;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
enums::GemState LivetimeSvc::checkState(double current_time)
{
  enums::GemState st;
  if(m_deadtime<=0) st=enums::LIVE;
  else if (current_time-m_lastTriggerTime<=m_deadzoneTime)st=enums::DEADZONE;
  else if (current_time-m_lastTriggerTime<=m_previousDeadtime)st=enums::BUSY;
  else st=enums::LIVE;
  return st;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double LivetimeSvc::livetime() ///< return the accumulated livetime
{
    return m_livetime;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StatusCode LivetimeSvc::queryInterface(const InterfaceID& riid, void** ppvInterface)
{
    if ( IID_ILivetimeSvc.versionMatch(riid) )  {
        *ppvInterface = (ILivetimeSvc*)this;
    }else{
        return Service::queryInterface(riid, ppvInterface);
    }
    addRef();
    return SUCCESS;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StatusCode LivetimeSvc::finalize ()
{
    MsgStream log( msgSvc(), name() );
    if( m_total>0 && m_deadtime>0){
        log << MSG::INFO 
            << "Processed " << m_total << " livetime requests, accepted "<< m_accepted 
            << "\n\t\t\t                Total livetime: "<< m_livetime
            << ", (" << int(100*m_livetime/m_totalTime+0.5) << "% of total)"
            <<  endreq;
    }


    return StatusCode::SUCCESS;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
unsigned int LivetimeSvc::ticks(double time) const
{
  /*
    | Haven't been careful about the absolute phasing
    | Note that 32 bits is enough to cover the necessary range. At
    | 20MHz, a 32 bit number covers about 217 seconds. We only need
    | to cover about 128 seconds.
  */
  return (unsigned int)(m_frequency * time);
}


