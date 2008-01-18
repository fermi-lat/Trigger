/** 
 * @file LivetimeSvc.cxx
 * @brief declare, implement the class LivetimeSvc
 *
 * $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/LivetimeSvc.cxx,v 1.7 2007/11/06 22:36:56 kocian Exp $
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
    /// check state at time current_time
    virtual enums::GemState checkState(double current_time);


    virtual double livetime(); ///< return the accumulated livetime
    /// set the trigger rate for interleave mode
    double setTriggerRate(double rate);
    /// elapsed time
    virtual double elapsed();
/**
 *
 *  @fn     unsigned int ticks (double time)
 *  @brief  Returns the low 32 bits of the number of elapsed ticks
 *          of the LAT system clock
 **/
  unsigned long long ticks(double time) const;
   
private:
    /// Allow only SvcFactory to instantiate the service.
    friend class SvcFactory<LivetimeSvc>;

    LivetimeSvc ( const std::string& name, ISvcLocator* al );

    DoubleProperty m_deadtime; ///< deadtime per trigger (nominal 25 us)
    DoubleProperty m_triggerRate; ///< background trigger rate
    DoubleProperty m_deadtimelong; ///< deadtime per trigger for large events
    DoubleProperty m_frequency; ///< background trigger rate
    BooleanProperty  m_interleave;
    double m_efficiency;
    double m_livetime; ///< update with request
    double m_totalTime;

    int m_total, m_accepted;
    int m_invisible_trig;
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
, m_invisible_trig(0)
, m_lastTriggerTime(0)
, m_previousDeadtime(0)
{
    // declare the properties and set defaults
    declareProperty("Deadtime",    m_deadtime=26.45e-6 );  // deadtime to apply to trigger, in sec.
    declareProperty("DeadtimeLong", m_deadtimelong=65.4e-6); // deadtime for four-range events
    declareProperty("clockrate", m_frequency=20e6);  // 20 MHz clock
    declareProperty("TriggerRate", m_triggerRate=2000.);  // effective total trigger rate
    declareProperty("InterleaveMode", m_interleave=true);  // Apply efficiency correction?
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
    m_efficiency = 1.0 - m_deadtime * m_triggerRate;
    if (m_interleave==true){
      log << MSG::INFO 
	  << "Interleave mode. Applying efficiency of " << m_efficiency*100 << "%" << endreq;
    }
    return status;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LivetimeSvc::tryToRegisterEvent(double current_time, bool highdeadtime)
{ 
   ++m_total;

   if (m_deadtime<=0) return true;
   bool live;
   if(m_interleave==true){
     live = current_time-m_lastTriggerTime >= m_deadtime;
   }else{
     live = current_time-m_lastTriggerTime >= m_previousDeadtime;
   }
   if( live ){
     // here if valid. Update the livetime
     if( m_lastTriggerTime>0){
       double elapsed(current_time-m_lastTriggerTime);
       m_totalTime+= elapsed;
       if(m_interleave==true){
	 double ntrig( RandPoisson::shoot(elapsed*m_triggerRate));
	 m_invisible_trig+=(int)ntrig;
	 m_livetime += elapsed-ntrig*m_deadtime;
       }else{
	 m_livetime += elapsed-m_previousDeadtime;
       }
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
  if (m_deadtime<=0) return true; // for backward compatibility
  bool live=true;
  if(m_interleave==true){
    live = current_time-m_lastTriggerTime >= m_deadtime;
    if( live && m_efficiency<1.0){
      // put in random
      double r = RandFlat::shoot();
      if( r > m_efficiency){
	live=false;
      }
    }
  }else{
    live = current_time-m_lastTriggerTime >= m_previousDeadtime;
  }
  return live;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double LivetimeSvc::setTriggerRate(double rate)
{
    double old = m_triggerRate;
    m_triggerRate = rate;
    m_efficiency = 1.0 - m_deadtime * m_triggerRate;
    return old;
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
double LivetimeSvc::elapsed() ///< return the accumulated elapsed time
{
    return m_totalTime;
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
            << "\n\t\t\t  Invisible triggers generated: "<< m_invisible_trig 
            << "\n\t\t\t                Total livetime: "<< m_livetime
            << ", (" << int(100*m_livetime/m_totalTime+0.5) << "% of total)"
            <<  endreq;
    }


    return StatusCode::SUCCESS;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  
unsigned long long LivetimeSvc::ticks(double time) const
{
  return (unsigned long long)(m_frequency * time);
}


