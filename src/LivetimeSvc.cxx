/** 
 * @file LivetimeSvc.cxx
 * @brief declare, implement the class LivetimeSvc
 *
 * $Header: /nfs/slac/g/glast/ground/cvs/Trigger/src/Attic/LivetimeSvc.cxx,v 1.1.2.3 2006/01/13 16:22:14 burnett Exp $
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

   Manage livetime, accounting for the effect of invisible background triggers.
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
    virtual StatusCode queryInterface( const IID& riid, void** ppvUnknown );


    ///check if valid trigger, and set last trigger time
    virtual bool isLive(double current_time);


    virtual double livetime(); ///< return the accumulated livetime

    double setTriggerRate(double rate);

private:
    /// Allow only SvcFactory to instantiate the service.
    friend class SvcFactory<LivetimeSvc>;

    LivetimeSvc ( const std::string& name, ISvcLocator* al );

    DoubleProperty m_deadtime; ///< deadtime per trigger (nominal 25 us)
    DoubleProperty m_triggerRate; ///< background trigger rate
    double m_efficiency;
    double m_livetime; ///< update with request
    double m_totalTime;

    int m_total, m_accepted;
    int m_invisible_trig;
    double m_lastTriggerTime;
 
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
{
    // declare the properties and set defaults
    declareProperty("Deadtime",    m_deadtime=25.0e-6 );  // deadtime to apply to trigger, in sec.
    declareProperty("TriggerRate", m_triggerRate=2000.);  // effective total trigger rate

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
StatusCode LivetimeSvc::initialize () 
{
    StatusCode  status =  Service::initialize ();

    // bind all of the properties for this service
    setProperties ();
    // open the message log
    MsgStream log( msgSvc(), name() );
    
    m_efficiency = 1.0 - m_deadtime * m_triggerRate;
    log << "Applying efficiency of " << m_efficiency*100 << "%" << endreq;
    return status;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool LivetimeSvc::isLive(double current_time)
{ 
   ++m_total;

   if (m_deadtime<=0) return true;

   bool live = current_time-m_lastTriggerTime >= m_deadtime;
   if( live && m_efficiency<1.0){
        // put in randome
        double r = RandFlat::shoot();
        if( r > m_efficiency){
            live=false;
        }else{
            // here if valid. Update the livetime, accounting for invisible triggers
            if( m_lastTriggerTime>0){
                double elapsed(current_time-m_lastTriggerTime);
                double ntrig( RandPoisson::shoot(elapsed*m_triggerRate));
                m_livetime += elapsed-ntrig*m_deadtime;
                m_totalTime+= elapsed;
                m_invisible_trig+=ntrig;
            }
            m_lastTriggerTime = current_time;
            ++m_accepted;
        }
   }
   return live;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double LivetimeSvc::livetime() ///< return the accumulated livetime
{
    return m_livetime;
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
StatusCode LivetimeSvc::queryInterface(const IID& riid, void** ppvInterface)
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
    log << MSG::INFO 
        << "Processed " << m_total << " livetime requests, accepted "<< m_accepted 
        << "\n\t\t\t  Invisible triggers generated: "<< m_invisible_trig 
        << "\n\t\t\t                Total livetime: "<< m_livetime
        << ", (" << int(100*m_livetime/m_totalTime+0.5) << "% of total)"
        <<  endreq;
    

    return StatusCode::SUCCESS;
}
