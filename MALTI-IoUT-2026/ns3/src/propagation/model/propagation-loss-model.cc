/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Contributions: Timo Bingmann <timo.bingmann@student.kit.edu>
 * Contributions: Tom Hewer <tomhewer@mac.com> for Two Ray Ground Model
 *                Pavel Boyko <boyko@iitp.ru> for matrix
 */

#include "propagation-loss-model.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <cmath>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PropagationLossModel");

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (PropagationLossModel);
#pragma region 
TypeId
PropagationLossModel::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::PropagationLossModel")
      .SetParent<Object> ()
      .SetGroupName ("Propagation");
  return tid;
}

PropagationLossModel::PropagationLossModel () : m_next (0)
{
}

PropagationLossModel::~PropagationLossModel ()
{
}

void
PropagationLossModel::SetNext (Ptr<PropagationLossModel> next)
{
  m_next = next;
}

Ptr<PropagationLossModel>
PropagationLossModel::GetNext ()
{
  return m_next;
}

double
PropagationLossModel::CalcRxPower (double txPowerDbm, 
                                    Ptr<MobilityModel> a,
                                    Ptr<MobilityModel> b) const
{
  double self = DoCalcRxPower (txPowerDbm, a, b);
  if (m_next != 0)
    {
      self = m_next->CalcRxPower (self, a, b);
    }
  return self;
}

int64_t
PropagationLossModel::AssignStreams (int64_t stream)
{
  int64_t currentStream = stream;
  currentStream += DoAssignStreams (stream);
  if (m_next != 0)
    {
      currentStream += m_next->AssignStreams (currentStream);
    }
  return (currentStream - stream);
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (RandomPropagationLossModel);

TypeId
RandomPropagationLossModel::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::RandomPropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<RandomPropagationLossModel> ()
          .AddAttribute (
              "Variable",
              "The random variable used to pick a loss every time CalcRxPower is invoked.",
              StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
              MakePointerAccessor (&RandomPropagationLossModel::m_variable),
              MakePointerChecker<RandomVariableStream> ());
  return tid;
}
RandomPropagationLossModel::RandomPropagationLossModel () : PropagationLossModel ()
{
}

RandomPropagationLossModel::~RandomPropagationLossModel ()
{
}

double
RandomPropagationLossModel::DoCalcRxPower (double txPowerDbm, Ptr<MobilityModel> a,
                                           Ptr<MobilityModel> b) const
{
  double rxc = -m_variable->GetValue ();
  NS_LOG_DEBUG ("attenuation coefficient=" << rxc << "Db");
  return txPowerDbm + rxc;
}

int64_t
RandomPropagationLossModel::DoAssignStreams (int64_t stream)
{
  m_variable->SetStream (stream);
  return 1;
}
#pragma endregion
// ------------------------------------------------------------------------- //

// NS_OBJECT_ENSURE_REGISTERED (FriisPropagationLossModel);
// #pragma region 
// TypeId
// FriisPropagationLossModel::GetTypeId (void) // MODIFICATION: We will modify to UG-Friis
// {
//   static TypeId tid =
//       TypeId ("ns3::FriisPropagationLossModel")
//           .SetParent<PropagationLossModel> ()
//           .SetGroupName ("Propagation")
//           .AddConstructor<FriisPropagationLossModel> ()
//           .AddAttribute ("Frequency",
//                         "The carrier frequency (in Hz) at which propagation occurs  (default is 5.15 GHz. Changed to 433MHz, 915 MHz)", // MODIFICATION: LoRA 433MHz/ 915MHz
//                         DoubleValue (2.4e9), // MODIFICATION: LoRA 433e6/868.1e6/915e6  --edited (433e6) from ORIGINAL (5.150e9) -- edit02182023
//                         MakeDoubleAccessor (&FriisPropagationLossModel::SetFrequency,
//                                             &FriisPropagationLossModel::GetFrequency),
//                         MakeDoubleChecker<double> ())
//           .AddAttribute ("SystemLoss", "The system loss",
//                          DoubleValue (1.0), // MODIFICATION: System loss=1: Change for UG-Friis?
//                          MakeDoubleAccessor (&FriisPropagationLossModel::m_systemLoss),
//                          MakeDoubleChecker<double> ())
//           .AddAttribute ("MinLoss",
//                          "The minimum value (dB) of the total loss, used at short ranges. Note: ",
//                          DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
//                          MakeDoubleAccessor (&FriisPropagationLossModel::SetMinLoss,
//                                              &FriisPropagationLossModel::GetMinLoss),
//                          MakeDoubleChecker<double> ())
//           /* .AddAttribute ("RxPower",
//                          "RxPower being overwritten. Note: ",
//                          DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
//                          MakeDoubleAccessor (&FriisPropagationLossModel::rxPower),
//                          MakeDoubleChecker<double> ()); */
//   ;
//   return tid;
// }

// FriisPropagationLossModel::FriisPropagationLossModel ()
// {
// }
// void
// FriisPropagationLossModel::SetSystemLoss (double systemLoss)
// {
//   m_systemLoss = systemLoss;
// }
// double
// FriisPropagationLossModel::GetSystemLoss (void) const
// {
//   return m_systemLoss;
// }
// void
// FriisPropagationLossModel::SetMinLoss (double minLoss)
// {
//   m_minLoss = minLoss;
// }
// double
// FriisPropagationLossModel::GetMinLoss (void) const
// {
//   return m_minLoss;
// }

// void
// FriisPropagationLossModel::SetFrequency (double frequency)
// {
//   m_frequency = frequency;
//   // introducing Eff Refractive Index, ref_ind = (sqrt(eps)) relation ---- MODIFICATION-Rummana 04/05/23
//   // with VWC = theta and bulk density of soil, b_density
  
//   // ref_ind = (9.93*vwc) + (2.454*b_density) - 1.208

//   // VWC range from 0 to 1 (we will use 0.01 to 0.50) . UNIT: cm-cube/ cm-cube
//   // bulk density range from 0.13 to 1.66 . UNIT: gm/ cm-cube

//   std::cout << "Frequency= " << m_frequency << std::endl; 
  
//   // double ref_ind = 4.796; // ref_ind of soil---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
//   double vwc = 0.005; // assigning 0.5%, 2.5%, 25% VWC, date 04/28/2024, MeasurementEnhanced----------//OLD: 5% to 30%
//   double b_density = 2.7409; // assigning 2.7409, date 04/28/2024, MeasurementEnhanced-------//OLD: 0.96 --assigned a mid value from range
//   double ref_ind = (9.93 * vwc) + (2.454 * b_density) - 1.208; // assign-derived ref_ind = SQRT(31), for VWC = 0.005, date 04/28/2024, MeasurementEnhanced----

//   static const double C = 299792458.0; 
//   double m_C = (299792458.0 / ref_ind); 

//   std::cout<< "vwc= " << vwc << ", b_density= " << b_density << std::endl;
//   //static const double C = (299792458.0/4.796); // speed of light in vacuum // MODIFICATION: UG Speed of Light C/ Effetive Refractive Index:: Change for UG: C(soil) = C/4.796 , soil's refractive indx  ---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
  
//   //std::cout<< "ref_ind= " << ref_ind << std::endl;
//   //ref_ind =3.2377 for the assigned values of vwc and b_density.
  
//   //static const double C = (299792458.0); // speed of light in vacuum 
//   m_lambda = C / frequency;
//   //m_lambda = m_C / frequency;
  
// }

// double
// FriisPropagationLossModel::GetFrequency (void) const
// {
//   return m_frequency;
// }

// double
// FriisPropagationLossModel::DbmToW (double dbm) const // Function for dB into Watts
// {
//   double mw = std::pow (10.0, dbm / 10.0);
//   return mw / 1000.0;
// }

// double
// FriisPropagationLossModel::DbmFromW (double w) const // Function for dB from Watts
// {
//   double dbm = std::log10 (w * 1000.0) * 10.0;
//   return dbm;
// }

// double
// FriisPropagationLossModel::DoCalcRxPower (double txPowerDbm, 
//                                           Ptr<MobilityModel> a,
//                                           Ptr<MobilityModel> b) const
// {
//   /*
//    * Friis free space equation:
//    * where Pt (Tx pwr), Gr, Gr and P(Rx pwr) are in Watt units
//    * L is in meter units.
//    *
//    *    P     Gt * Gr * (lambda^2)
//    *   --- = ---------------------       -------------------(Eq1)
//    *    Pt     (4 * pi * d)^2 * L
//    *
//    * Gt: tx gain (unit-less)  = 1 assigned in this case
//    * Gr: rx gain (unit-less)  = 1 assigned in this case
//    * Pt: tx power (W)
//    * d: distance (m)
//    * L: system loss
//    * lambda: wavelength (m)
//    *
//    * Here, we ignore tx and rx gain (considering gain=1) and the input and output values 
//    * are in dB or dBm:
//    * 
//    *                           lambda^2
//    * rx = tx +  10 log10 (-------------------)   // Performing 10log10 on both sides of (Eq1)
//    *                       (4 * pi * d)^2 * L
//    *
//    * rx: rx power (dB)
//    * tx: tx power (dB)
//    * d: distance (m)
//    * L: system loss (unit-less)
//    * lambda: wavelength (m)
//    */
//   double distance = a->GetDistanceFrom (b);
//   if (distance <
//       3 * m_lambda) // VVI: Friis Model works ONLY for FAR FIELD // CHECKPOINT: Calculate FAR FIELD RANGE for UG LoRA
//     {
//       NS_LOG_WARN ("distance not within the far field region => inaccurate propagation loss value");
//     }
//   if (distance <= 0)
//     {
//       return txPowerDbm - m_minLoss;
//     }
//   double numerator = m_lambda * m_lambda;
//   double denominator = 16 * M_PI * M_PI * distance * distance * m_systemLoss;
//   double lossDb = -10 * log10 (numerator / denominator);
  
//   NS_LOG_DEBUG ("distance=" << distance << "m, loss=" << lossDb << "dB");

//   // MODIFICATION: Adding an extra printing line here: Distance, Loss, and Received Power
//   //NS_LOG_UNCOND ("This is DoCalcRxPower");  //Tester
//   //double m_rxPower = txPowerDbm - std::max (lossDb, m_minLoss);
//   // m_distance = distance;
//   // m_txPower = txPowerDbm;

//   /* NS_LOG_UNCOND ("distance=" << distance << " m,  Txp " << txPowerDbm << "dB loss=" << lossDb
//                              << "dB, Received Power in dB="
//                              << txPowerDbm - std::max (lossDb, m_minLoss));  */
//   return txPowerDbm - std::max (lossDb, m_minLoss);
// }

// int64_t
// FriisPropagationLossModel::DoAssignStreams (int64_t stream)
// {
//   return 0;
// }
#pragma endregion

//-------------------------------------------------------------------------------------//
//-----------------MAME-WiFi Model-After Testbed 04/28/2024--------------------------//
// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (FriisPropagationLossModel);

TypeId
FriisPropagationLossModel::GetTypeId (void) // MODIFICATION: We will modify to UG-Friis
{
  static TypeId tid =
      TypeId ("ns3::FriisPropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<FriisPropagationLossModel> ()
          .AddAttribute ("Frequency",
                        "The carrier frequency (in Hz) at which propagation occurs  (default is 5.15 GHz. Changed to 433MHz, 915 MHz)", // MODIFICATION: LoRA 433MHz/ 915MHz
                        DoubleValue (2.4e9), // MODIFICATION: LoRA 433e6/868.1e6/915e6  --edited (433e6) from ORIGINAL (5.150e9) -- edit02182023
                        MakeDoubleAccessor (&FriisPropagationLossModel::SetFrequency,
                                            &FriisPropagationLossModel::GetFrequency),
                        MakeDoubleChecker<double> ())
          .AddAttribute ("SystemLoss", "The system loss",
                         DoubleValue (1.0), // MODIFICATION: System loss=1: Change for UG-Friis?
                         MakeDoubleAccessor (&FriisPropagationLossModel::m_systemLoss),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("MinLoss",
                         "The minimum value (dB) of the total loss, used at short ranges. Note: ",
                         DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
                         MakeDoubleAccessor (&FriisPropagationLossModel::SetMinLoss,
                                             &FriisPropagationLossModel::GetMinLoss),
                         MakeDoubleChecker<double> ())
          /* .AddAttribute ("RxPower",
                         "RxPower being overwritten. Note: ",
                         DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
                         MakeDoubleAccessor (&FriisPropagationLossModel::rxPower),
                         MakeDoubleChecker<double> ()); */   
          .AddAttribute ("VWC",
               "Volumetric water content for soil-aware Friis model.",
               DoubleValue (0.15),
               MakeDoubleAccessor (&FriisPropagationLossModel::m_vwc),
               MakeDoubleChecker<double> (0.0, 1.0))
          .AddAttribute ("BulkDensity",
               "Soil bulk density for soil-aware Friis model.",
               DoubleValue (1.75),
               MakeDoubleAccessor (&FriisPropagationLossModel::m_bulkDensity),
               MakeDoubleChecker<double> ()) 
          .AddAttribute ("ConcreteDepthInch",
               "Concrete slab depth/thickness in inches for MAME concrete loss.",
               DoubleValue (10.0),
               MakeDoubleAccessor (&FriisPropagationLossModel::m_concreteDepthInch),
               MakeDoubleChecker<double> (0.0))              
  ;
  return tid;
}

// FriisPropagationLossModel::FriisPropagationLossModel ()
// {
// }
FriisPropagationLossModel::FriisPropagationLossModel ()
  : m_lambda (0.0),
    m_frequency (2.4e9),
    m_systemLoss (1.0),
    m_minLoss (0.0),
    m_vwc (0.15),
    m_bulkDensity (1.75),
    m_concreteDepthInch (10.0)
{
}
//------MODIFICATION 05/24/2026----FOR AUTOMATING VWC, B_DENSITY ---------------//
void
FriisPropagationLossModel::SetSystemLoss (double systemLoss)
{
  m_systemLoss = systemLoss;
}
double
FriisPropagationLossModel::GetSystemLoss (void) const
{
  return m_systemLoss;
}
void
FriisPropagationLossModel::SetMinLoss (double minLoss)
{
  m_minLoss = minLoss;
}
double
FriisPropagationLossModel::GetMinLoss (void) const
{
  return m_minLoss;
}
//----------MODIFICATION 05/24/2026----BRINGING BACK TO SIMPLE, FOR AUTOMATING VWC, B_DENSITY--------//
void
FriisPropagationLossModel::SetFrequency (double frequency)
{
  m_frequency = frequency;
}
//----------MODIFICATION 05/24/2026----BRINGING BACK TO SIMPLE, FOR AUTOMATING VWC, B_DENSITY--------//
// /*
// void
// FriisPropagationLossModel::SetFrequency (double frequency)
// {
//   m_frequency = frequency;
//   // introducing Eff Refractive Index, ref_ind = (sqrt(eps)) relation ---- MODIFICATION-Rummana 04/05/23
//   // with VWC = theta and bulk density of soil, b_density
  
//   // ref_ind = (9.93*vwc) + (2.454*b_density) - 1.208

//   // VWC range from 0 to 1 (we will use 0.01 to 0.50) . UNIT: cm-cube/ cm-cube
//   // bulk density range from 0.13 to 1.66 . UNIT: gm/ cm-cube

//   std::cout << "Frequency= " << m_frequency << std::endl; 
  

//   //==== Commented 05/24/2026=====REPLACED BY VALUES FROM ATTRIBUTES=== FOR AUTOMATION=======//
//   // // double ref_ind = 4.796; // ref_ind of soil---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
//   // double vwc = 0.29; // 0.15, 0.29; assigning 0.5%, 2.5%, 25% VWC, date 04/28/2024, MeasurementEnhanced----------//OLD: 5% to 30%
//   // /*   Unit cc/cc: values {0.01, 0.03, 0.05, 0.07, 0.9, 
//   //                         0.11, 0.13, 0.15, 0.17, 0.19, 
//   //                         0.21, 0.23, 0.25, 0.27, 0.29}
//   // */
//   // double b_density = 1.75; // assigning 2.7409, date 04/28/2024, MeasurementEnhanced-------//OLD: 0.96 --assigned a mid value from range
//   //  /*   Unit gm/cc: values {0.96, 1.10, 1.25, 1.5, 
//   //                           1.75, 2.0, 2.25, 2.5, 2.7409}
//   // */
//   //==== Commented 05/24/2026=====REPLACED BY VALUES FROM ATTRIBUTES=== FOR AUTOMATION=======//

//   double vwc = m_vwc;
//   double b_density = m_bulkDensity;
//   double ref_ind = (9.93 * vwc) + (2.454 * b_density) - 1.208; // assign-derived ref_ind = SQRT(31)=5.5677, for VWC = 0.005, date 04/28/2024, MeasurementEnhanced----

//   static const double C = 299792458.0; 
//   double m_C = (299792458.0 / ref_ind); 

//   std::cout<< "vwc= " << vwc << ", b_density= " << b_density << std::endl;
//   //static const double C = (299792458.0/4.796); // speed of light in vacuum // MODIFICATION: UG Speed of Light C/ Effetive Refractive Index:: Change for UG: C(soil) = C/4.796 , soil's refractive indx  ---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
  
//   //std::cout<< "ref_ind= " << ref_ind << std::endl;
//   //ref_ind =3.2377 for the assigned values of vwc and b_density.
  
//   //static const double C = (299792458.0); // speed of light in vacuum 
//   //m_lambda = C / frequency;
//   m_lambda = m_C / frequency;
  
//  }


double
FriisPropagationLossModel::GetFrequency (void) const
{
  return m_frequency;
}

double
FriisPropagationLossModel::DbmToW (double dbm) const // Function for dB into Watts
{
  double mw = std::pow (10.0, dbm / 10.0);
  return mw / 1000.0;
}

double
FriisPropagationLossModel::DbmFromW (double w) const // Function for dB from Watts
{
  double dbm = std::log10 (w * 1000.0) * 10.0;
  return dbm;
}

double
FriisPropagationLossModel::DoCalcRxPower (double txPowerDbm, 
                                          Ptr<MobilityModel> a,
                                          Ptr<MobilityModel> b) const
{
  /*
   * Friis free space equation:
   * where Pt (Tx pwr), Gr, Gr and P(Rx pwr) are in Watt units
   * L is in meter units.
   *
   *    P     Gt * Gr * (lambda^2)
   *   --- = ---------------------       -------------------(Eq1)
   *    Pt     (4 * pi * d)^2 * L
   *
   * Gt: tx gain (unit-less)  = 1 assigned in this case
   * Gr: rx gain (unit-less)  = 1 assigned in this case
   * Pt: tx power (W)
   * d: distance (m)
   * L: system loss
   * lambda: wavelength (m)
   *
   * Here, we ignore tx and rx gain (considering gain=1) and the input and output values 
   * are in dB or dBm:
   * 
   *                           lambda^2
   * rx = tx +  10 log10 (-------------------)   // Performing 10log10 on both sides of (Eq1)
   *                       (4 * pi * d)^2 * L
   *
   * rx: rx power (dB)
   * tx: tx power (dB)
   * d: distance (m)
   * L: system loss (unit-less)
   * lambda: wavelength (m)
   */
  double ref_ind = (9.93 * m_vwc) + (2.454 * m_bulkDensity) - 1.208; // Refractive Index for 
                                                                  // UG material as a function of VWC and BulkDensity

  if (ref_ind <= 0.0)
  {
    NS_LOG_WARN ("Invalid ref_ind=" << ref_ind
                << " from VWC=" << m_vwc
                << " BulkDensity=" << m_bulkDensity);
    ref_ind = 1.0;
  }

  double soilSpeed = 299792458.0 / ref_ind;
  double lambda = soilSpeed / m_frequency;

  double distance = a->GetDistanceFrom (b);
  if (distance <
      3 * lambda) // VVI: Friis Model works ONLY for FAR FIELD // CHECKPOINT: Calculate FAR FIELD RANGE for UG LoRA
    {
      NS_LOG_WARN ("distance not within the far field region => inaccurate propagation loss value");
    }
  if (distance <= 0)
    {
      return txPowerDbm - m_minLoss;
    }
  //-------Original Friis-Material Enhanced--------------//
  // double numerator = m_lambda * m_lambda;
  // double denominator = 16 * M_PI * M_PI * distance * distance * m_systemLoss;
  // double lossDb = -10 * log10 (numerator / denominator);

   //------- MAME-wifi Model, Replacing Original Friis-------------//
   //===== FRIIS_FOR_SOIL========================================//
   double gamma_m = 20.0; 
   double beta_m = 20.0; 
   double alpha_m = 1.5; // 10.0 --- original: Value 1.5 chosen after fitting with the empirical data
   double A_absorption_soil = 0.1406; //for SOIL max: 0.3406 dB/m-- original A_absorp = 0.868*alpha_absorp

   double distance2_soil = distance;
   if(distance>= 1.99){distance2_soil = distance - 2.0;}
   else if(distance>= 1.017){distance2_soil = distance - 1.016;}
   else {distance2_soil = distance;}

  //  double lossDb_soil = 21.984 + (gamma_m * log10 (distance)) - (beta_m * log10 (m_lambda)) 
  //                               + (alpha_m * distance) ; // 20 log(4 Pi) = 21.984
   double lossDb_soil = 21.984 + (gamma_m * log10 (distance)) 
                                - (beta_m * log10 (lambda)) 
                                - (alpha_m * distance2_soil) 
                                + (A_absorption_soil * distance); // 20 log(4 Pi) = 21.984

   //===== FRIIS_FOR_CONCRETE======================================//
   double depth_conc_inch = m_concreteDepthInch;
   double depth_conc = (0.0254)* (depth_conc_inch);    // = (0.3048)* (depth_conc_inch/12.0);
  //  double lossDb_conc = 31.4; // Average loss in a 6inch concrete wall loss_conc = 31.4 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
  // double lossDb_conc = 47.1; // Average loss in a 9inch concrete wall loss_conc = 31.4 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
   if (depth_conc <= 0.0508 || depth_conc >= 0.9144){
    NS_LOG_WARN ("Invalid concrete depth=" << depth_conc
                  << " m from ConcreteDepthInch=" << m_concreteDepthInch << " outside the range of 2 inches to 36 inches");
                    depth_conc = 0.0254*2; // fallback to 2 inch
   }
   double conc_lambda = 0.037; //conc_lambda = lambda0 / sqrt(eps_c=11.9) ; lambda0= (C0/ freq_0) = 0.1249166667
   // for eps_c {Re,Im} = {10.75, 3}, HIGH value, considering heavy FerroMag content
   // ==> eps_c_eff = 11.16 => ref_ind_c = 3.34 (=sqrt(eps_c_eff))
   // eps_c_eff = {4.5 min-dry, ..... 8.1 max-wet, 6.25 mid} => ref_ind_c = 2.5 => conc_lambda = 0.05

   double gamma_c = 20.0; 
   double beta_c = 20.0; 
   double alpha_c = 10.0; // 10., 5, 2.5, 1.25, for concrete
   double A_absorption_conc = 15.0; //for SOIL max: 0.3406 dB/m-- original A_absorp = 0.868*alpha_absorp

   double depth2_conc = depth_conc;
   if(depth_conc_inch >= 30.0001){depth2_conc = depth_conc - (0.0254)* (30.0);}
   else if(depth_conc_inch >= 24.0001){depth2_conc = depth_conc - (0.0254)* (24.0);} 
   else if(depth_conc_inch >= 14.0001){depth2_conc = depth_conc - (0.0254)* (14.0);} 
   else {depth2_conc = depth_conc;}

   if(depth_conc_inch >= 34.0001){alpha_c = 0.75;}
   else if(depth_conc_inch >= 30.0001){alpha_c = 1.25;} 
   else if(depth_conc_inch >= 24.0001){alpha_c = 2.5;}  
   else if(depth_conc_inch >= 14.0001){alpha_c = 5.0;} 
   else {alpha_c = 10.0;}

   double lossDb_conc = 21.984 + (gamma_c * log10 (depth_conc)) 
                                - (beta_c * log10 (conc_lambda)) 
                                - (alpha_c * depth2_conc) 
                                + (A_absorption_conc * depth_conc); // 20 log(4 Pi) = 21.984


  //  double lossDb_conc = 38.27; // 10 inch.... //with A_abs 22.56 | w/o 24.416 = 2 inch,
                                //with A_abs 29.01 | w/o 29.929 = 4 inch,
                                //with A_abs 32.9 | w/o 32.943 = 6 inch, ... Average loss in a 12inch concrete wall loss_conc = 32.943 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
                                //with A_abs 35.9 | w/o 34.934 = 8 inch,
                                //with A_abs 38.27 | w/o 36.364 = 10 inch,
                                //with A_abs 40.2 | w/o 37.440 = 12 inch, ...or 23.26-dry ; 
                                //with A_abs 48.913 | w/o 38.270 = 14 inch, 
                                //with A_abs 53.117 | w/o 38.922 = 16 inch, 
                                //with A_abs 57.185 | w/o 43.755	= 18 inch, // derived from Modified MAME_concrete, beyond 16 inches
                                //with A_abs 59.366 | w/o 44.416	= 20 inch, ... // 
                                //with A_abs 61.460 | w/o 44.990	= 22 inch,
                                //with A_abs 63.483 | w/o 45.492	= 24 inch,
                                //with A_abs 66.841 | w/o 47.076	= 26 inch,
                                //with A_abs 68.878 | w/o 47.593	= 28 inch,
                                //with A_abs 70.871 | w/o 48.065	= 30 inch,
                                //with A_abs 73.269 | w/o 48.499	= 32 inch,
                                //with A_abs 75.253 | w/o 49.470	= 34 inch,
                                //with A_abs 77.282 | w/o 50.030	= 36 inch,        


  //===== LOSS_FRIIS_FOR_SOIL+CONCRETE========================================//
   double lossDb = lossDb_conc + lossDb_soil; 
  //  std::cout<< "Distance= " << distance << ", tx Power (dBm)= " << txPowerDbm << ", Loss_total= " << lossDb 
  //           << ", Loss_soil= " << lossDb_soil << ", d_2_soil= " << distance2_soil 
  //           << ", Loss_absorp (alpha_absrp x dist)= " << A_absorption_soil * distance << std::endl;

   return txPowerDbm - std::max (lossDb, m_minLoss);

  //------- MAME-lora Model, Replacing Original Friis-------------//
//  if (distance <= 1)
//    { 
//  double gamma_m = 32.0; 
//  double beta_m = 32.0; 
//  double alpha_m = 6.0; 
//  double lossDb_conc = 31.4; // Considering 2" brick. Average loss in a 5inch concrete wall loss_conc = 31.4 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
//  double lossDb_soil = 21.984 + (gamma_m * log10 (distance)) - (beta_m * log10 (m_lambda)) + (alpha_m * distance) ; // 20 log(4 Pi) = 21.984
//  double lossDb = lossDb_conc + lossDb_soil; 
//
//  NS_LOG_UNCOND ("distance=" << distance << " m,  Txp " << txPowerDbm << "dB loss=" << lossDb
//                             << "dB, Received Power in dB="
//                             << txPowerDbm - std::max (lossDb, m_minLoss));
//  return txPowerDbm - std::max (lossDb, m_minLoss);
//    }
//  
//  if (distance > 1)
//   {
//  double gamma_m = 32.0; 
//  double beta_m = 32.0; 
//  double alpha_m = 15.0; 
//  double lossDb_conc = 31.4; // Average loss in a 5inch concrete wall loss_conc = 31.4 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
//  double lossDb_soil = 21.984 + (gamma_m * log10 (distance)) - (beta_m * log10 (m_lambda)) + (alpha_m * distance) ; // 20 log(4 Pi) = 21.984
//  double lossDb = lossDb_conc + lossDb_soil; 
//  
//  NS_LOG_DEBUG ("distance=" << distance << "m, loss=" << lossDb << "dB");
 
//  NS_LOG_UNCOND ("distance=" << distance << " m,  Txp " << txPowerDbm << "dB loss=" << lossDb
//                             << "dB, Received Power in dB="
//                             << txPowerDbm - std::max (lossDb, m_minLoss));
//  return txPowerDbm - std::max (lossDb, m_minLoss);
//   } 

  
}

int64_t
FriisPropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}
//-------------MAME-WiFi Model-After Testbed---new class---Ends Here----04/28/2024---------------------------// 

// ----------------------------------------------------------------------------//
//------------------------------ Model After Testbed----------------------------------//
// // -------------New Friis2 Class BEGINs HERE------------------------------------//
NS_OBJECT_ENSURE_REGISTERED (FriisPropagationLossModel2);

TypeId
FriisPropagationLossModel2::GetTypeId (void) // MODIFICATION: We will modify to UG-Friis
{
  static TypeId tid =
      TypeId ("ns3::FriisPropagationLossModel2")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<FriisPropagationLossModel2> ()
          .AddAttribute ("Frequency",
                        "The carrier frequency (in Hz) at which propagation occurs  (default is 5.15 GHz. Changed to 433MHz, 915 MHz)", // MODIFICATION: LoRA 433MHz/ 915MHz
                        DoubleValue (2.4e9), // MODIFICATION: LoRA 433e6/868.1e6/915e6  --edited (433e6) from ORIGINAL (5.150e9) -- edit02182023
                        MakeDoubleAccessor (&FriisPropagationLossModel2::SetFrequency,
                                            &FriisPropagationLossModel2::GetFrequency),
                        MakeDoubleChecker<double> ())
          .AddAttribute ("BulkDensity",
                        "The bulk density of soil (in gm/cc) (default is 0.96. Range 0.13 to 1.4)", // MODIFICATION: 
                        DoubleValue (0.96), // MODIFICATION: -- edit06132023
                        MakeDoubleAccessor (&FriisPropagationLossModel2::SetBulkDensity,
                                            &FriisPropagationLossModel2::GetBulkDensity),
                        MakeDoubleChecker<double> ())
          .AddAttribute ("VWC",
                        "Volumetric Water Content of soil (in cc/cc) (may vary from 1\% to 90%)", // MODIFICATION: 
                        DoubleValue (0.10), // MODIFICATION: -- edit06142023
                        MakeDoubleAccessor (&FriisPropagationLossModel2::SetVWC,
                                            &FriisPropagationLossModel2::GetVWC),
                        MakeDoubleChecker<double> ())
          .AddAttribute ("SystemLoss", "The system loss",
                         DoubleValue (1.0), // MODIFICATION: System loss=1: Change for UG-Friis?
                         MakeDoubleAccessor (&FriisPropagationLossModel2::m_systemLoss),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("MinLoss",
                         "The minimum value (dB) of the total loss, used at short ranges. Note: ",
                         DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
                         MakeDoubleAccessor (&FriisPropagationLossModel2::SetMinLoss,
                                             &FriisPropagationLossModel2::GetMinLoss),
                         MakeDoubleChecker<double> ())
          /* .AddAttribute ("RxPower",
                         "RxPower being overwritten. Note: ",
                         DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
                         MakeDoubleAccessor (&FriisPropagationLossModel2::rxPower),
                         MakeDoubleChecker<double> ()); */
  ;
  return tid;
}

FriisPropagationLossModel2::FriisPropagationLossModel2 ()
{
}
void
FriisPropagationLossModel2::SetSystemLoss (double systemLoss)
{
  m_systemLoss = systemLoss;
}
double
FriisPropagationLossModel2::GetSystemLoss (void) const
{
  return m_systemLoss;
}
void
FriisPropagationLossModel2::SetMinLoss (double minLoss)
{
  m_minLoss = minLoss;
}
double
FriisPropagationLossModel2::GetMinLoss (void) const
{
  return m_minLoss;
}

void
FriisPropagationLossModel2::SetBulkDensity (double bulkDensity)            //---------ADDed by Rummana
{
  b_density = bulkDensity;
}
double
FriisPropagationLossModel2::GetBulkDensity (void) const               //---------ADDed by Rummana
{
  return b_density;
}

void
FriisPropagationLossModel2::SetVWC (double vWaterc)            //---------ADDed by Rummana
{
  vw_content = vWaterc;
}
double
FriisPropagationLossModel2::GetVWC (void) const               //---------ADDed by Rummana
{
  return vw_content;
}

void
FriisPropagationLossModel2::SetFrequency (double frequency)
{
  m_frequency = frequency;
  // introducing Eff Refractive Index, ref_ind = (sqrt(eps)) relation ---- MODIFICATION-Rummana 04/05/23
  // with VWC = theta and bulk density of soil, b_density
  // ref_ind = (9.93*theta) + (2.454*b_density) - 1.208
  // ref_ind = (9.93*vwc) + (2.454*b_density) - 1.208

  // VWC range from 0 to 1 (we will use 0.01 to 0.50) . UNIT: cm-cube/ cm-cube
  // bulk density range from 0.13 to 1.66 . UNIT: gm/ cm-cube

  std::cout << "Frequency= " << m_frequency << std::endl; 
  
  // double ref_ind = 4.796; // ref_ind of soil---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
  

/*----Reactivating, 12/25/2024, this section for MAME-VWC-BDensity---originally from 4/2023 */ 
  //Calculation for vwc-b_density based model //
  double vwc = GetVWC(); // assigned a mid value from range 0.01 to 0.30
  double b_Density = GetBulkDensity(); // =0.96 , assigned a mid value from range
  double ref_ind = (9.93 * vwc) + (2.454 * b_Density) - 1.208; 
  
  std::cout<< "VWC= " << vwc << ", b_Density= " << b_Density << std::endl;
  //static const double C = (299792458.0/4.796); // speed of light in vacuum // MODIFICATION: UG Speed of Light C/ Effetive Refractive Index:: Change for UG: C(soil) = C/4.796 , soil's refractive indx  ---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
  //std::cout<< "ref_ind= " << ref_ind << std::endl;

/*----Reactivating, 12/25/2024, ABOVE section for MAME-VWC-BDensity---originally from 4/2023 */


/* This section below was before-MAME: Soil+concrete: thickness ratio 4:1 ------*/
  // double ref_ind_s = sqrt(23.08);   //Eps_r_s = 23.08, 23.1
  // double ref_ind_c = sqrt (11.9);   //Eps_r_c = 11.9, 11.16
  // double ref_ind = ((ref_ind_s*4)+(ref_ind_c))/5;        //MODIFICATION for Testbed Model

  //ref_ind =3.2377 for the assigned values of vwc and b_density.
/* This section above was before-MAME: Soil+concrete: thickness ratio 4:1 ------*/

  std::cout<< "ref_ind= " << ref_ind << std::endl;       //-----------ADDED by RR. testing
  static const double C = 299792458.0; 
  double m_C = (299792458.0 / ref_ind); 
  std::cout<< "m_C= " << m_C << std::endl;               //-----------ADDED by RR. Testing
 
    
  //static const double C = (299792458.0); // speed of light in vacuum 
  //m_lambda = C / frequency;
  m_lambda = m_C / frequency;
  std::cout<< "m_lambda= " << m_lambda << std::endl;               //-----------ADDED by RR. Testing
}

double
FriisPropagationLossModel2::GetFrequency (void) const
{
  return m_frequency;
}

double
FriisPropagationLossModel2::DbmToW (double dbm) const // Function for dB into Watts
{
  double mw = std::pow (10.0, dbm / 10.0);
  return mw / 1000.0;
}

double
FriisPropagationLossModel2::DbmFromW (double w) const // Function for dB from Watts
{
  double dbm = std::log10 (w * 1000.0) * 10.0;
  return dbm;
}

double
FriisPropagationLossModel2::DoCalcRxPower (double txPowerDbm, 
                                          Ptr<MobilityModel> a,
                                          Ptr<MobilityModel> b) const
{
  /*
   * Friis free space equation:
   * where Pt (Tx pwr), Gr, Gr and P(Rx pwr) are in Watt units
   * L is in meter units.
   *
   *    P     Gt * Gr * (lambda^2)
   *   --- = ---------------------       -------------------(Eq1)
   *    Pt     (4 * pi * d)^2 * L
   *
   * Gt: tx gain (unit-less)  = 1 assigned in this case
   * Gr: rx gain (unit-less)  = 1 assigned in this case
   * Pt: tx power (W)
   * d: distance (m)
   * L: system loss
   * lambda: wavelength (m)
   *
   * Here, we ignore tx and rx gain (considering gain=1) and the input and output values 
   * are in dB or dBm:
   * 
   *                           lambda^2
   * rx = tx +  10 log10 (-------------------)   // Performing 10log10 on both sides of (Eq1)
   *                       (4 * pi * d)^2 * L
   *
   * rx: rx power (dB)
   * tx: tx power (dB)
   * d: distance (m)
   * L: system loss (unit-less)
   * lambda: wavelength (m)
   */
  double distance = a->GetDistanceFrom (b);
  //std::cout<< "distance_frm_DoCalcRxP= " << distance << std::endl;               //-----------ADDED by RR. Testing
  if (distance <
      3 * m_lambda) // VVI: Friis Model works ONLY for FAR FIELD // CHECKPOINT: Calculate FAR FIELD RANGE for UG LoRA
    {
      NS_LOG_WARN ("distance not within the far field region => inaccurate propagation loss value");
    }
  if (distance <= 0)
    {
      return txPowerDbm - m_minLoss;
    }
  
/* //----Original Friis-------also for Friis Material Enhanced------//
  double numerator = m_lambda * m_lambda;
  double denominator = 16 * M_PI * M_PI * distance * distance * m_systemLoss;
  double lossDb = -10 * log10 (numerator / denominator);
*/ 

   //------- MAME-wifi Model, Replacing Original Friis-------------//
   double gamma_m = 20.0; 
   double beta_m = 20.0; 
   double alpha_m = -10.0; 
   double lossDb_conc = 31.4; // Average loss in a 5inch concrete wall loss_conc = 31.4 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
   double lossDb_soil = 21.984 + (gamma_m * log10 (distance)) - (beta_m * log10 (m_lambda)) + (alpha_m * distance) ; // 20 log(4 Pi) = 21.984
   double lossDb = lossDb_conc + lossDb_soil; 
  
  //  NS_LOG_UNCOND ("distance=" << distance << " m,  Txp " << txPowerDbm << "dB loss=" << lossDb
  //                           << "dB, Received Power in dB="
  //                           << txPowerDbm - std::max (lossDb, m_minLoss));

   return txPowerDbm - std::max (lossDb, m_minLoss);

  //------- MAME-lora Model, Replacing Original Friis-------------//
//  if (distance <= 1)
//    { 
//  double gamma_m = 32.0; 
//  double beta_m = 32.0; 
//  double alpha_m = 6.0; 
//  double lossDb_conc = 31.4; // Considering 2" brick. Average loss in a 5inch concrete wall loss_conc = 31.4 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
//  double lossDb_soil = 21.984 + (gamma_m * log10 (distance)) - (beta_m * log10 (m_lambda)) + (alpha_m * distance) ; // 20 log(4 Pi) = 21.984
//  double lossDb = lossDb_conc + lossDb_soil; 
//
//  NS_LOG_UNCOND ("distance=" << distance << " m,  Txp " << txPowerDbm << "dB loss=" << lossDb
//                             << "dB, Received Power in dB="
//                             << txPowerDbm - std::max (lossDb, m_minLoss));
//  return txPowerDbm - std::max (lossDb, m_minLoss);
//    }
//  
//  if (distance > 1)
//   {
//  double gamma_m = 32.0; 
//  double beta_m = 32.0; 
//  double alpha_m = 15.0; 
//  double lossDb_conc = 31.4; // Average loss in a 5inch concrete wall loss_conc = 31.4 dBm from (eps_c = 11.9, lambda_c = 0.036 m)
//  double lossDb_soil = 21.984 + (gamma_m * log10 (distance)) - (beta_m * log10 (m_lambda)) + (alpha_m * distance) ; // 20 log(4 Pi) = 21.984
//  double lossDb = lossDb_conc + lossDb_soil; 
//  

//  NS_LOG_DEBUG ("distance=" << distance << "m, loss=" << lossDb << "dB");

  /* NS_LOG_UNCOND ("distance=" << distance << " m,  Txp " << txPowerDbm << "dB loss=" << lossDb
                             << "dB, Received Power in dB="
                             << txPowerDbm - std::max (lossDb, m_minLoss));  */
  return txPowerDbm - std::max (lossDb, m_minLoss);
}

int64_t
FriisPropagationLossModel2::DoAssignStreams (int64_t stream)
{
  return 0;
}
// // -------------NewClass ENDs HERE------------------------------------//

// //------------- Model before TestBed---------------------------------------------//
//
// -------------New Friis Class BEGINs HERE------------------------------------//
// NS_OBJECT_ENSURE_REGISTERED (FriisPropagationLossModel2);

// TypeId
// FriisPropagationLossModel2::GetTypeId (void) // MODIFICATION: We will modify to UG-Friis
// {
//   static TypeId tid =
//       TypeId ("ns3::FriisPropagationLossModel2")
//           .SetParent<PropagationLossModel> ()
//           .SetGroupName ("Propagation")
//           .AddConstructor<FriisPropagationLossModel2> ()
//           .AddAttribute ("Frequency",
//                         "The carrier frequency (in Hz) at which propagation occurs  (default is 5.15 GHz. Changed to 433MHz, 915 MHz)", // MODIFICATION: LoRA 433MHz/ 915MHz
//                         DoubleValue (433e6), // MODIFICATION: LoRA 433e6/868.1e6/915e6  --edited (433e6) from ORIGINAL (5.150e9) -- edit02182023
//                         MakeDoubleAccessor (&FriisPropagationLossModel2::SetFrequency,
//                                             &FriisPropagationLossModel2::GetFrequency),
//                         MakeDoubleChecker<double> ())
//           .AddAttribute ("BulkDensity",
//                         "The bulk density of soil (in gm/cc) (default is 0.96. Range 0.13 to 1.4)", // MODIFICATION: 
//                         DoubleValue (0.96), // MODIFICATION: -- edit06132023
//                         MakeDoubleAccessor (&FriisPropagationLossModel2::SetBulkDensity,
//                                             &FriisPropagationLossModel2::GetBulkDensity),
//                         MakeDoubleChecker<double> ())
//           .AddAttribute ("VWC",
//                         "Volumetric Water Content of soil (in cc/cc) (may vary from 1\% to 90%)", // MODIFICATION: 
//                         DoubleValue (0.10), // MODIFICATION: -- edit06142023
//                         MakeDoubleAccessor (&FriisPropagationLossModel2::SetVWC,
//                                             &FriisPropagationLossModel2::GetVWC),
//                         MakeDoubleChecker<double> ())
//           .AddAttribute ("SystemLoss", "The system loss",
//                          DoubleValue (1.0), // MODIFICATION: System loss=1: Change for UG-Friis?
//                          MakeDoubleAccessor (&FriisPropagationLossModel2::m_systemLoss),
//                          MakeDoubleChecker<double> ())
//           .AddAttribute ("MinLoss",
//                          "The minimum value (dB) of the total loss, used at short ranges. Note: ",
//                          DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
//                          MakeDoubleAccessor (&FriisPropagationLossModel2::SetMinLoss,
//                                              &FriisPropagationLossModel2::GetMinLoss),
//                          MakeDoubleChecker<double> ())
//           /* .AddAttribute ("RxPower",
//                          "RxPower being overwritten. Note: ",
//                          DoubleValue (0.0), // MODIFICATION: Minimum loss=0: Change for UG-Friis?
//                          MakeDoubleAccessor (&FriisPropagationLossModel2::rxPower),
//                          MakeDoubleChecker<double> ()); */
//   ;
//   return tid;
// }

// FriisPropagationLossModel2::FriisPropagationLossModel2 ()
// {
// }
// void
// FriisPropagationLossModel2::SetSystemLoss (double systemLoss)
// {
//   m_systemLoss = systemLoss;
// }
// double
// FriisPropagationLossModel2::GetSystemLoss (void) const
// {
//   return m_systemLoss;
// }
// void
// FriisPropagationLossModel2::SetMinLoss (double minLoss)
// {
//   m_minLoss = minLoss;
// }
// double
// FriisPropagationLossModel2::GetMinLoss (void) const
// {
//   return m_minLoss;
// }

// void
// FriisPropagationLossModel2::SetBulkDensity (double bulkDensity)            //---------ADDed by Rummana
// {
//   b_density = bulkDensity;
// }
// double
// FriisPropagationLossModel2::GetBulkDensity (void) const               //---------ADDed by Rummana
// {
//   return b_density;
// }

// void
// FriisPropagationLossModel2::SetVWC (double vWaterc)            //---------ADDed by Rummana
// {
//   vw_content = vWaterc;
// }
// double
// FriisPropagationLossModel2::GetVWC (void) const               //---------ADDed by Rummana
// {
//   return vw_content;
// }

// void
// FriisPropagationLossModel2::SetFrequency (double frequency)
// {
//   m_frequency = frequency;
//   // introducing Eff Refractive Index, ref_ind = (sqrt(eps)) relation ---- MODIFICATION-Rummana 04/05/23
//   // with VWC = theta and bulk density of soil, b_density
//   // ref_ind = (9.93*theta) + (2.454*b_density) - 1.208
//   // ref_ind = (9.93*vwc) + (2.454*b_density) - 1.208

//   // VWC range from 0 to 1 (we will use 0.01 to 0.50) . UNIT: cm-cube/ cm-cube
//   // bulk density range from 0.13 to 1.66 . UNIT: gm/ cm-cube

//   std::cout << "Frequency= " << m_frequency << std::endl; 
  
//   // double ref_ind = 4.796; // ref_ind of soil---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
//   double vwc = GetVWC(); // assigned a mid value from range 0.01 to 0.30
//   double b_Density = GetBulkDensity(); // =0.96 , assigned a mid value from range
//   double ref_ind = (9.93 * vwc) + (2.454 * b_Density) - 1.208; 

//   static const double C = 299792458.0; 
//   double m_C = (299792458.0 / ref_ind); 

//   std::cout<< "VWC= " << vwc << ", b_Density= " << b_Density << std::endl;
//   //static const double C = (299792458.0/4.796); // speed of light in vacuum // MODIFICATION: UG Speed of Light C/ Effetive Refractive Index:: Change for UG: C(soil) = C/4.796 , soil's refractive indx  ---- MODIFICATION-COMMENTED ORIGINAL LINE: Rummana 04/05/23
//   //std::cout<< "ref_ind= " << ref_ind << std::endl;
//   //ref_ind =3.2377 for the assigned values of vwc and b_density.
  
//   //static const double C = (299792458.0); // speed of light in vacuum 
//   //m_lambda = C / frequency;
//   m_lambda = m_C / frequency;
  
// }

// double
// FriisPropagationLossModel2::GetFrequency (void) const
// {
//   return m_frequency;
// }

// double
// FriisPropagationLossModel2::DbmToW (double dbm) const // Function for dB into Watts
// {
//   double mw = std::pow (10.0, dbm / 10.0);
//   return mw / 1000.0;
// }

// double
// FriisPropagationLossModel2::DbmFromW (double w) const // Function for dB from Watts
// {
//   double dbm = std::log10 (w * 1000.0) * 10.0;
//   return dbm;
// }

// double
// FriisPropagationLossModel2::DoCalcRxPower (double txPowerDbm, 
//                                           Ptr<MobilityModel> a,
//                                           Ptr<MobilityModel> b) const
// {
//   /*
//    * Friis free space equation:
//    * where Pt (Tx pwr), Gr, Gr and P(Rx pwr) are in Watt units
//    * L is in meter units.
//    *
//    *    P     Gt * Gr * (lambda^2)
//    *   --- = ---------------------       -------------------(Eq1)
//    *    Pt     (4 * pi * d)^2 * L
//    *
//    * Gt: tx gain (unit-less)  = 1 assigned in this case
//    * Gr: rx gain (unit-less)  = 1 assigned in this case
//    * Pt: tx power (W)
//    * d: distance (m)
//    * L: system loss
//    * lambda: wavelength (m)
//    *
//    * Here, we ignore tx and rx gain (considering gain=1) and the input and output values 
//    * are in dB or dBm:
//    * 
//    *                           lambda^2
//    * rx = tx +  10 log10 (-------------------)   // Performing 10log10 on both sides of (Eq1)
//    *                       (4 * pi * d)^2 * L
//    *
//    * rx: rx power (dB)
//    * tx: tx power (dB)
//    * d: distance (m)
//    * L: system loss (unit-less)
//    * lambda: wavelength (m)
//    */
//  double distance = a->GetDistanceFrom (b);
//   if (distance <
//       3 * m_lambda) // VVI: Friis Model works ONLY for FAR FIELD // CHECKPOINT: Calculate FAR FIELD RANGE for UG LoRA
//     {
//       NS_LOG_WARN ("distance not within the far field region => inaccurate propagation loss value");
//     }
//   if (distance <= 0)
//     {
//       return txPowerDbm - m_minLoss;
//     }
//   double numerator = m_lambda * m_lambda;
//   double denominator = 16 * M_PI * M_PI * distance * distance * m_systemLoss;
//   double lossDb = -10 * log10 (numerator / denominator);
  
//   NS_LOG_DEBUG ("distance=" << distance << "m, loss=" << lossDb << "dB");

//   // MODIFICATION: Adding an extra printing line here: Distance, Loss, and Received Power
//   //NS_LOG_UNCOND ("This is DoCalcRxPower");  //Tester
//   //double m_rxPower = txPowerDbm - std::max (lossDb, m_minLoss);
//   // m_distance = distance;
//   // m_txPower = txPowerDbm;

//   /*NS_LOG_UNCOND ("distance=" << distance << " m,  Txp " << txPowerDbm << "dB loss=" << lossDb
//                              << "dB, Received Power in dB="
//                              << txPowerDbm - std::max (lossDb, m_minLoss)); */   //-----this line can be kept commented
//   return txPowerDbm - std::max (lossDb, m_minLoss);
// }

// int64_t
// FriisPropagationLossModel2::DoAssignStreams (int64_t stream)
// {
//   return 0;
// } 

// -------------NewClass ENDs HERE------------------------------------//

// //------------- Model before Testbed Ends Here -------------------------------//



// ------------------------------------------------------------------------- //
// -- Two-Ray Ground Model ported from NS-2 -- tomhewer@mac.com -- Nov09 //

NS_OBJECT_ENSURE_REGISTERED (TwoRayGroundPropagationLossModel);
#pragma region 
TypeId 
TwoRayGroundPropagationLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TwoRayGroundPropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<TwoRayGroundPropagationLossModel> ()
          .AddAttribute (
              "Frequency",
              "The carrier frequency (in Hz) at which propagation occurs  (default is 5.15 GHz).",
              DoubleValue (5.150e9),
                   MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::SetFrequency,
                                       &TwoRayGroundPropagationLossModel::GetFrequency),
                   MakeDoubleChecker<double> ())
          .AddAttribute ("SystemLoss", "The system loss", DoubleValue (1.0),
                         MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::m_systemLoss),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("MinDistance",
                        "The distance under which the propagation model refuses to give results (m)",
                        DoubleValue (0.5),
                        MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::SetMinDistance,
                                            &TwoRayGroundPropagationLossModel::GetMinDistance),
                        MakeDoubleChecker<double> ())
          .AddAttribute ("HeightAboveZ",
                         "The height of the antenna (m) above the node's Z coordinate",
                         DoubleValue (0),
                         MakeDoubleAccessor (&TwoRayGroundPropagationLossModel::m_heightAboveZ),
                         MakeDoubleChecker<double> ())
  ;
  return tid;
}

TwoRayGroundPropagationLossModel::TwoRayGroundPropagationLossModel ()
{
}
void
TwoRayGroundPropagationLossModel::SetSystemLoss (double systemLoss)
{
  m_systemLoss = systemLoss;
}
double
TwoRayGroundPropagationLossModel::GetSystemLoss (void) const
{
  return m_systemLoss;
}
void
TwoRayGroundPropagationLossModel::SetMinDistance (double minDistance)
{
  m_minDistance = minDistance;
}
double
TwoRayGroundPropagationLossModel::GetMinDistance (void) const
{
  return m_minDistance;
}
void
TwoRayGroundPropagationLossModel::SetHeightAboveZ (double heightAboveZ)
{
  m_heightAboveZ = heightAboveZ;
}

void
TwoRayGroundPropagationLossModel::SetFrequency (double frequency)
{
  m_frequency = frequency;
  static const double C = 299792458.0; // speed of light in vacuum
  m_lambda = C / frequency;
}

double
TwoRayGroundPropagationLossModel::GetFrequency (void) const
{
  return m_frequency;
}

double 
TwoRayGroundPropagationLossModel::DbmToW (double dbm) const
{
  double mw = std::pow (10.0, dbm / 10.0);
  return mw / 1000.0;
}

double
TwoRayGroundPropagationLossModel::DbmFromW (double w) const
{
  double dbm = std::log10 (w * 1000.0) * 10.0;
  return dbm;
}

double 
TwoRayGroundPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                 Ptr<MobilityModel> a,
                                                 Ptr<MobilityModel> b) const
{
  /*
   * Two-Ray Ground equation:
   *
   * where Pt, Gt and Gr are in dBm units
   * L, Ht and Hr are in meter units.
   *
   *   Pr      Gt * Gr * (Ht^2 * Hr^2)
   *   -- =  (-------------------------)
   *   Pt            d^4 * L
   *
   * Gt: tx gain (unit-less)
   * Gr: rx gain (unit-less)
   * Pt: tx power (dBm)
   * d: distance (m)
   * L: system loss
   * Ht: Tx antenna height (m)
   * Hr: Rx antenna height (m)
   * lambda: wavelength (m)
   *
   * As with the Friis model we ignore tx and rx gain and output values
   * are in dB or dBm
   *
   *                      (Ht * Ht) * (Hr * Hr)
   * rx = tx + 10 log10 (-----------------------)
   *                      (d * d * d * d) * L
   */
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_minDistance)
    {
      return txPowerDbm;
    }

  // Set the height of the Tx and Rx antennae
  double txAntHeight = a->GetPosition ().z + m_heightAboveZ;
  double rxAntHeight = b->GetPosition ().z + m_heightAboveZ;

  // Calculate a crossover distance, under which we use Friis
  /*
   * 
   * dCross = (4 * pi * Ht * Hr) / lambda
   *
   */

  double dCross = (4 * M_PI * txAntHeight * rxAntHeight) / m_lambda;
  double tmp = 0;
  if (distance <= dCross)
    {
      // We use Friis
      double numerator = m_lambda * m_lambda;
      tmp = M_PI * distance;
      double denominator = 16 * tmp * tmp * m_systemLoss;
      double pr = 10 * std::log10 (numerator / denominator);
      NS_LOG_DEBUG ("Receiver within crossover (" << dCross << "m) for Two_ray path; using Friis");
      NS_LOG_DEBUG ("distance=" << distance << "m, attenuation coefficient=" << pr << "dB");
      return txPowerDbm + pr;
    }
  else // Use Two-Ray Pathloss
    {
      tmp = txAntHeight * rxAntHeight;
      double rayNumerator = tmp * tmp;
      tmp = distance * distance;
      double rayDenominator = tmp * tmp * m_systemLoss;
      double rayPr = 10 * std::log10 (rayNumerator / rayDenominator);
      NS_LOG_DEBUG ("distance=" << distance << "m, attenuation coefficient=" << rayPr << "dB");
      return txPowerDbm + rayPr;
    }
}

int64_t
TwoRayGroundPropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (LogDistancePropagationLossModel);

TypeId
LogDistancePropagationLossModel::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::LogDistancePropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<LogDistancePropagationLossModel> ()
          .AddAttribute ("Exponent", "The exponent of the Path Loss propagation model",
                         DoubleValue (3.0),
                         MakeDoubleAccessor (&LogDistancePropagationLossModel::m_exponent),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("ReferenceDistance",
                         "The distance at which the reference loss is calculated (m)",
                         DoubleValue (1.0),
                         MakeDoubleAccessor (&LogDistancePropagationLossModel::m_referenceDistance),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("ReferenceLoss",
                         "The reference loss at reference distance (dB). (Default is Friis at 1m "
                         "with 5.15 GHz)",
                         DoubleValue (46.6777),
                         MakeDoubleAccessor (&LogDistancePropagationLossModel::m_referenceLoss),
                         MakeDoubleChecker<double> ());
  return tid;

}

LogDistancePropagationLossModel::LogDistancePropagationLossModel ()
{
}

void
LogDistancePropagationLossModel::SetPathLossExponent (double n)
{
  m_exponent = n;
}
void
LogDistancePropagationLossModel::SetReference (double referenceDistance, double referenceLoss)
{
  m_referenceDistance = referenceDistance;
  m_referenceLoss = referenceLoss;
}
double
LogDistancePropagationLossModel::GetPathLossExponent (void) const
{
  return m_exponent;
}

double
LogDistancePropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_referenceDistance)
    {
      return txPowerDbm - m_referenceLoss;
    }
  /**
   * The formula is:
   * rx = 10 * log (Pr0(tx)) - n * 10 * log (d/d0)
   *
   * Pr0: rx power at reference distance d0 (W)
   * d0: reference distance: 1.0 (m)
   * d: distance (m)
   * tx: tx power (dB)
   * rx: dB
   *
   * Which, in our case is:
   *
   * rx = rx0(tx) - 10 * n * log (d/d0)
   */
  double pathLossDb = 10 * m_exponent * std::log10 (distance / m_referenceDistance);
  double rxc = -m_referenceLoss - pathLossDb;
  NS_LOG_DEBUG ("distance=" << distance << "m, reference-attenuation=" << -m_referenceLoss << "dB, "
                            << "attenuation coefficient=" << rxc << "db");
  /* std::cout << "TxP = " << txPowerDbm << " and  "
            << "received power= " << txPowerDbm + rxc << "dBm" << std::endl; */
  //NS_LOG_UNCOND ("This is DoCalcRxPower");  //Tester
  return txPowerDbm + rxc;
}

int64_t
LogDistancePropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (ThreeLogDistancePropagationLossModel);

TypeId
ThreeLogDistancePropagationLossModel::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::ThreeLogDistancePropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<ThreeLogDistancePropagationLossModel> ()
          .AddAttribute ("Distance0", "Beginning of the first (near) distance field",
                         DoubleValue (1.0),
                         MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_distance0),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("Distance1", "Beginning of the second (middle) distance field.",
                         DoubleValue (200.0),
                         MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_distance1),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("Distance2", "Beginning of the third (far) distance field.",
                         DoubleValue (500.0),
                         MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_distance2),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("Exponent0", "The exponent for the first field.", DoubleValue (1.9),
                         MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_exponent0),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("Exponent1", "The exponent for the second field.", DoubleValue (3.8),
                         MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_exponent1),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("Exponent2", "The exponent for the third field.", DoubleValue (3.8),
                         MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_exponent2),
                         MakeDoubleChecker<double> ())
          .AddAttribute (
              "ReferenceLoss",
              "The reference loss at distance d0 (dB). (Default is Friis at 1m with 5.15 GHz)",
              DoubleValue (46.6777),
              MakeDoubleAccessor (&ThreeLogDistancePropagationLossModel::m_referenceLoss),
              MakeDoubleChecker<double> ());
  return tid;

}

ThreeLogDistancePropagationLossModel::ThreeLogDistancePropagationLossModel ()
{
}

double 
ThreeLogDistancePropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                                     Ptr<MobilityModel> a,
                                                     Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  NS_ASSERT (distance >= 0);

  // See doxygen comments for the formula and explanation

  double pathLossDb;

  if (distance < m_distance0)
    {
      pathLossDb = 0;
    }
  else if (distance < m_distance1)
    {
      pathLossDb = m_referenceLoss + 10 * m_exponent0 * std::log10 (distance / m_distance0);
    }
  else if (distance < m_distance2)
    {
      pathLossDb = m_referenceLoss + 10 * m_exponent0 * std::log10 (m_distance1 / m_distance0) +
                   10 * m_exponent1 * std::log10 (distance / m_distance1);
    }
  else
    {
      pathLossDb = m_referenceLoss + 10 * m_exponent0 * std::log10 (m_distance1 / m_distance0) +
                   10 * m_exponent1 * std::log10 (m_distance2 / m_distance1) +
                   10 * m_exponent2 * std::log10 (distance / m_distance2);
    }

  NS_LOG_DEBUG ("ThreeLogDistance distance=" << distance << "m, "
                                             << "attenuation=" << pathLossDb << "dB");

  return txPowerDbm - pathLossDb;
}

int64_t
ThreeLogDistancePropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (NakagamiPropagationLossModel);

TypeId
NakagamiPropagationLossModel::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::NakagamiPropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<NakagamiPropagationLossModel> ()
          .AddAttribute ("Distance1", "Beginning of the second distance field. Default is 80m.",
                         DoubleValue (80.0),
                         MakeDoubleAccessor (&NakagamiPropagationLossModel::m_distance1),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("Distance2", "Beginning of the third distance field. Default is 200m.",
                         DoubleValue (200.0),
                         MakeDoubleAccessor (&NakagamiPropagationLossModel::m_distance2),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("m0", "m0 for distances smaller than Distance1. Default is 1.5.",
                         DoubleValue (1.5),
                         MakeDoubleAccessor (&NakagamiPropagationLossModel::m_m0),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("m1", "m1 for distances smaller than Distance2. Default is 0.75.",
                         DoubleValue (0.75),
                         MakeDoubleAccessor (&NakagamiPropagationLossModel::m_m1),
                         MakeDoubleChecker<double> ())
          .AddAttribute ("m2", "m2 for distances greater than Distance2. Default is 0.75.",
                         DoubleValue (0.75),
                         MakeDoubleAccessor (&NakagamiPropagationLossModel::m_m2),
                         MakeDoubleChecker<double> ())
          .AddAttribute (
              "ErlangRv", "Access to the underlying ErlangRandomVariable",
              StringValue ("ns3::ErlangRandomVariable"),
              MakePointerAccessor (&NakagamiPropagationLossModel::m_erlangRandomVariable),
              MakePointerChecker<ErlangRandomVariable> ())
          .AddAttribute ("GammaRv", "Access to the underlying GammaRandomVariable",
                         StringValue ("ns3::GammaRandomVariable"),
                         MakePointerAccessor (&NakagamiPropagationLossModel::m_gammaRandomVariable),
                         MakePointerChecker<GammaRandomVariable> ());
  ;
  return tid;

}

NakagamiPropagationLossModel::NakagamiPropagationLossModel ()
{
}

double
NakagamiPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                             Ptr<MobilityModel> a,
                                             Ptr<MobilityModel> b) const
{
  // select m parameter

  double distance = a->GetDistanceFrom (b);
  NS_ASSERT (distance >= 0);

  double m;
  if (distance < m_distance1)
    {
      m = m_m0;
    }
  else if (distance < m_distance2)
    {
      m = m_m1;
    }
  else
    {
      m = m_m2;
    }

  // the current power unit is dBm, but Watt is put into the Nakagami /
  // Rayleigh distribution.
  double powerW = std::pow (10, (txPowerDbm - 30) / 10);

  double resultPowerW;

  // switch between Erlang- and Gamma distributions: this is only for
  // speed. (Gamma is equal to Erlang for any positive integer m.)
  unsigned int int_m = static_cast<unsigned int> (std::floor (m));

  if (int_m == m)
    {
      resultPowerW = m_erlangRandomVariable->GetValue (int_m, powerW / m);
    }
  else
    {
      resultPowerW = m_gammaRandomVariable->GetValue (m, powerW / m);
    }

  double resultPowerDbm = 10 * std::log10 (resultPowerW) + 30;

  NS_LOG_DEBUG ("Nakagami distance=" << distance << "m, "
                                     << "power=" << powerW << "W, "
                                     << "resultPower=" << resultPowerW << "W=" << resultPowerDbm
                                     << "dBm");

  return resultPowerDbm;
}

int64_t
NakagamiPropagationLossModel::DoAssignStreams (int64_t stream)
{
  m_erlangRandomVariable->SetStream (stream);
  m_gammaRandomVariable->SetStream (stream + 1);
  return 2;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (FixedRssLossModel);

TypeId 
FixedRssLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FixedRssLossModel")
                          .SetParent<PropagationLossModel> ()
                          .SetGroupName ("Propagation")
                          .AddConstructor<FixedRssLossModel> ()
                          .AddAttribute ("Rss", "The fixed receiver Rss.", DoubleValue (-150.0),
                                         MakeDoubleAccessor (&FixedRssLossModel::m_rss),
                                         MakeDoubleChecker<double> ());
  return tid;
}
FixedRssLossModel::FixedRssLossModel ()
  : PropagationLossModel ()
{
}

FixedRssLossModel::~FixedRssLossModel ()
{
}

void
FixedRssLossModel::SetRss (double rss)
{
  m_rss = rss;
}

double
FixedRssLossModel::DoCalcRxPower (double txPowerDbm,
                                  Ptr<MobilityModel> a,
                                  Ptr<MobilityModel> b) const
{
  return m_rss;
}

int64_t
FixedRssLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (MatrixPropagationLossModel);

TypeId 
MatrixPropagationLossModel::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::MatrixPropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<MatrixPropagationLossModel> ()
          .AddAttribute ("DefaultLoss", "The default value for propagation loss, dB.",
                         DoubleValue (std::numeric_limits<double>::max ()),
                         MakeDoubleAccessor (&MatrixPropagationLossModel::m_default),
                         MakeDoubleChecker<double> ());
  return tid;
}

MatrixPropagationLossModel::MatrixPropagationLossModel ()
  : PropagationLossModel (), m_default (std::numeric_limits<double>::max ())
{
}

MatrixPropagationLossModel::~MatrixPropagationLossModel ()
{
}

void 
MatrixPropagationLossModel::SetDefaultLoss (double loss)
{
  m_default = loss;
}

void
MatrixPropagationLossModel::SetLoss (Ptr<MobilityModel> ma, Ptr<MobilityModel> mb, double loss, bool symmetric)
{
  NS_ASSERT (ma != 0 && mb != 0);

  MobilityPair p = std::make_pair (ma, mb);
  std::map<MobilityPair, double>::iterator i = m_loss.find (p);

  if (i == m_loss.end ())
    {
      m_loss.insert (std::make_pair (p, loss));
    }
  else
    {
      i->second = loss;
    }

  if (symmetric)
    {
      SetLoss (mb, ma, loss, false);
    }
}

double 
MatrixPropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                           Ptr<MobilityModel> a,
                                           Ptr<MobilityModel> b) const
{
  std::map<MobilityPair, double>::const_iterator i = m_loss.find (std::make_pair (a, b));

  if (i != m_loss.end ())
    {
      return txPowerDbm - i->second;
    }
  else
    {
      return txPowerDbm - m_default;
    }
}

int64_t
MatrixPropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (RangePropagationLossModel);

TypeId
RangePropagationLossModel::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::RangePropagationLossModel")
          .SetParent<PropagationLossModel> ()
          .SetGroupName ("Propagation")
          .AddConstructor<RangePropagationLossModel> ()
          .AddAttribute ("MaxRange", "Maximum Transmission Range (meters)", DoubleValue (250),
                         MakeDoubleAccessor (&RangePropagationLossModel::m_range),
                         MakeDoubleChecker<double> ());
  return tid;
}

RangePropagationLossModel::RangePropagationLossModel ()
{
}

double
RangePropagationLossModel::DoCalcRxPower (double txPowerDbm,
                                          Ptr<MobilityModel> a,
                                          Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_range)
    {
      return txPowerDbm;
    }
  else
    {
      return -1000;
    }
}

int64_t
RangePropagationLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}
#pragma endregion
// ------------------------------------------------------------------------- //

} // namespace ns3
