// RUN:: ./ns3 run scratch/node_iter_flow_module.cc

// to SAVE OUTPUTS in  a FILE, use:
//./ns3 run scratch/node_iter_flow_module.cc > "OP_12172024-node_iter_flow_flex2_run10_MAME_vwc015_node4_17.txt" 2>&1
//./ns3 run scratch/vFriisn6-performance >>"OP-vFriisn6-vwc02-bd96-Per" 2>&1

// FOR PCAP TRACING::           tcpdump -nn -tt -r vFriisn6-vwc02-bd96-0-0.pcap >> "OP-vFriisn6-vwc02-bd96-dump" 2>&1

//---Added from "tcp-star-server-v2"---for AsciiTrace--08/21/2024---//
#include <iostream> //---- needed for randomizaiton and other functions
#include <fstream>
#include <string>
#include <cassert>
#include <cstdlib> //----opt1 needed for randomizaiton 12/17/2024
#include <ctime> //----opt1 needed for randomizaiton 12/17/2024
#include <random> //----opt2 needed for randomizaiton 12/17/2024

#include <vector>
#include <chrono>   //----Added for wall-clock progress/ETA logging in batch/parallel runs
#include <iomanip>  //----Added for std::setprecision in progress logging
//---Added from "tcp-star-server-v2"---for AsciiTrace--08/21/2024---//

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"

#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-channel.h" // No Need until Air medium

#include "ns3/ssid.h" // CHECKed WITH THIRD.CC HEADER LIST
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-address-helper.h"

#include "ns3/wifi-mac-header.h" //----Not used. can be commented
#include "ns3/internet-stack-helper.h" //----Not used. can be commented

#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy.h"

#include "ns3/vector.h"
#include "ns3/csv-reader.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h" //----Used only when NetAnim is needed
#include <map>

// #include "functions.h"

// Default Network Topology
//
//            Wifi 10.1.1.0       Wifi 10.1.2.0           Wifi 10.1.3.0
//              AP=n0                 AP=n6                   AP=n9
//     *   *   *   *   *             *   *             *    *    *    *    *
//     |   |   |   |   |             |   |             |    |    |    |    |
//     n5  n4  n3  n2  n1            n8  n7           n14  n13  n12  n11  n10
//

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("NodeContainer-092024");


// int experiment_1(double pkt_itvl, uint32_t pkt_size, uint32_t max_num_pkt);
int experiment_1(double pkt_itvl, uint32_t pkt_size, uint32_t max_num_pkt,
                 double vwc, double b_density, double depth_conc_inch);

// std::ofstream op_csv("op_flow_wifi.csv"); // globally declared o/p file stream
std::ofstream op_csv;  //---------------12/21/2024------OP_csv file pointer declared. Will be opened inside main in APPEND Mode

//-------06/16/2026-----To log RSSI--------------------------//
std::map<Mac48Address, Ipv4Address> g_macToIp;
std::map<Ipv4Address, double> g_rssiSumDbm;
std::map<Ipv4Address, uint32_t> g_rssiCount;
//-------06/18/2026-----To log RSSI replacing -999--------------------------//
std::map<Ipv4Address, double> g_lastValidRssiByIp;
double g_lastValidRssiGlobal = -999.0;

bool
RecordRssiIfStaMacMatches (Mac48Address mac, double rssiDbm)
{
  std::map<Mac48Address, Ipv4Address>::iterator it = g_macToIp.find (mac);

  if (it == g_macToIp.end ())
    {
      return false;
    }

  Ipv4Address srcIp = it->second;
  g_rssiSumDbm[srcIp] += rssiDbm;
  g_rssiCount[srcIp] += 1;

  g_lastValidRssiByIp[srcIp] = rssiDbm;
  g_lastValidRssiGlobal = rssiDbm;

  return true;
}

void
ApMonitorSnifferRx (Ptr<const Packet> packet,
                    uint16_t channelFreqMhz,
                    WifiTxVector txVector,
                    MpduInfo mpduInfo,
                    SignalNoiseDbm signalNoise,
                    uint16_t staId)
{
  (void) channelFreqMhz;
  (void) txVector;
  (void) mpduInfo;
  (void) staId;

  Ptr<Packet> copy = packet->Copy ();
  WifiMacHeader hdr;

  if (copy->PeekHeader (hdr) == 0)
    {
      return;
    }

  if (!hdr.IsData ())
    {
      return;
    }

  double rssiDbm = signalNoise.signal;

  // For STA -> AP data frames, Addr2 is usually the STA transmitter.
  // But try multiple fields to avoid address-mode mismatch.
  if (RecordRssiIfStaMacMatches (hdr.GetAddr2 (), rssiDbm))
    {
      return;
    }

  if (RecordRssiIfStaMacMatches (hdr.GetAddr3 (), rssiDbm))
    {
      return;
    }

  if (RecordRssiIfStaMacMatches (hdr.GetAddr1 (), rssiDbm))
    {
      return;
    }
}

// int
// experiment_1(double pkt_itvl, uint32_t pkt_size, uint32_t max_num_pkt)
int
experiment_1(double pkt_itvl, uint32_t pkt_size, uint32_t max_num_pkt,
             double vwc, double b_density, double depth_conc_inch){
  std::random_device rand; // 12/17/2024: Randomization of samp_seed
  std::mt19937 gen (rand ());
  std::uniform_int_distribution<> dis (10, 100);
  int random_number = dis (gen); // 12/17/2024: Randomization of samp_seed
  std::cout << "random_number betn 10 and 100 is = " << random_number << std::endl;
  // 12/17/2024: Randomization of samp_seed

  // int samp_seed = random_number; //10, 50, 100, 200, 300, 400, 75
  int samp_seed = 10;
  RngSeedManager::SetSeed (samp_seed); // Changes seed from default of 1 to 3
  std::cout << "sample seed value is = " << samp_seed << std::endl;
  bool verbose = true;
  bool tracing = true;
  
  //------Clear RSSI log-------------//
  g_macToIp.clear ();
  g_rssiSumDbm.clear ();
  g_rssiCount.clear ();

  g_lastValidRssiByIp.clear ();
  g_lastValidRssiGlobal = -999.0;

  uint32_t nAp = 5; //====== validated config matching the 54.2 pkt-events/sec calibration ====
  uint32_t nWifi[] = {12, 14, 2, 16, 8}; // first 5 clusters of the 10-cluster coord file

  // double b_density = 2.74; // default 0.96
  // double vwc = 0.15;  // default 0.05

  CommandLine cmd (__FILE__);
  uint32_t id_wifi = 0;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi[id_wifi]); //~~~~~~~~~~~~~~ May go in loop
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  //--------------12/19/2024-----Modularity using userDefined Function: Commented the line below: cmd.Parse---//
  //cmd.Parse (argc, argv);

  // The underlying restriction of 18 is due to the grid position
  // allocator's configuration; the grid layout will exceed the bounding box if more than 18 nodes are provided.
  if (nWifi[id_wifi] > 18)
    {
      std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box"
                << std::endl;
      return 1;
    }

  
  /*~~~~~~~~~~~~~~~~~~ Section Below CAN BE LOOPED (if variable_naming can be looped)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

  NodeContainer Ap;
  // NodeContainer Sta;
  //-----------------------------PHY Layer-------------------------------------------------------------//
  YansWifiPhyHelper phy; 
  
  //YansWifiChannelHelper channel = YansWifiChannelHelper::Default (); // MODIFICATION: set to Friis
  // Ptr<FriisPropagationLossModel2> loss1 = CreateObject<FriisPropagationLossModel2> () ;  // MODIFICATION: Added this line
  // loss1->SetBulkDensity (b_density1); // default was (0.96) , (1.16)
  // loss1->SetVWC(vwc1);
  // //loss1->SetFrequency (433.0e6);  // LoRA default was(868.1e6);
  // // SetFrequency() has DEPENDENCY on SetBulkDensity() --CHANGE the location of m_lambda in propagation-loss-model.cc to get rid of this DEPENDENCY

  YansWifiChannelHelper channel;
  //---modified on 05/29/2026------//
  // channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel",
                             "VWC", DoubleValue (vwc),
                             "BulkDensity", DoubleValue (b_density));
 
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel",
                            "VWC", DoubleValue (vwc),
                            "BulkDensity", DoubleValue (b_density),
                            "ConcreteDepthInch", DoubleValue (depth_conc_inch));
  //---ChennelHelper Modification Ends here-----// MODIFICATION to Model2 from Model (01/29/2024)
  phy.SetChannel (channel.Create ());

  //--------------------------- MAC Layer -----------------------------------------------------------//

  WifiHelper wifi; //wifi.SetRemoteStationManager ("ns3::AarfWifiManager");  //from old version tutorials
  WifiMacHelper mac;
  // Ssid ssid = Ssid ("MAME-ssid");   // ---taking inside loop

  //---------------------Device setup------------------------------//
  NetDeviceContainer apDevices;
  NetDeviceContainer staDevices;

  /* ---------------SetPositionAllocator()  VECTOR OPTION--- */
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel"); // MobilityHelper class
  // Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  //------Relocating position allocator to INSIDE AP LOOP----6/1/2026--- for MULTI-CLUSTER RUN----//
  
  //============================ declaring input and output csv files==================================//
  CsvReader csv ("./scratch/coord_10Ap_flex_time.csv");  // matches the validated 5-cluster/52-node test run


  //======== wifi ---------Internet and Addressing -----//
  InternetStackHelper stack;
  Ipv4AddressHelper address;

  /*~~~~~~~~~~~~~~~~~~ Section Above CAN BE LOOPED (if variable_naming can be looped) ~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  // serverApps.Stop(Seconds(605));
  // clientApps.Stop(Seconds(600));
  // Simulator::Stop(Seconds(610));
  //Simulator::Stop (Seconds (615));


  //------Added on 6/1/2026--- for Distance logging--------------//
  std::map<Ipv4Address, Ptr<Node>> ipToNode; //------Added on 6/1/2026--- for Distance logging
  std::map<Ipv4Address, uint32_t> ipToClusterSize;
  //------Added on 6/1/2026--- for #NumNodes logging--------------//

  double globalSimStopTime = 0.0;
  // uint32_t m = 0;
  // for(NetDeviceContainer::Iterator k = Dev.Begin(); k != Dev.End(); k++, m++)
  for (uint32_t m = 0; m < nAp; m++){
      std::string wificluster_ = "wifiStaCluster_";
      id_wifi = m + 1; // id_wifi = k; should work also
      std::string wifiStaNodes_k = wificluster_ + std::to_string (id_wifi);
      std::cout << "==== wifiCluster_number: " << wifiStaNodes_k << " =====" << std::endl;

      Ssid ssid = Ssid ("MAME-wifi-ssid_" + std::to_string (id_wifi));
      std::cout << "Ssid created is: " << "MAME-wifi-ssid_" + std::to_string (id_wifi) << std::endl;


      Ap.Add (1); //Ap.Create (1);
      uint32_t id_ap = Ap.Get (m)->GetId ();
      //-----------------------------------------------std::cout << "------Ap_Node Number = " << id_ap << "-------" << std::endl;

      /*---this also works: ptr version for ApNode---*/
      // Ptr<Node> ptr_ap = Ap.Get (m);
      // uint32_t id_ap_ptr = ptr_ap->GetId();
      // std::cout <<"Ap_Node Number "<< id_ap_ptr << std::endl;

      /*...*/
      uint32_t numStaNodes = nWifi[m]; //MODIFICATION NEEDED for FLEXIBLE numStaNodes
      
      NodeContainer Sta;
      Sta.Add (numStaNodes); //Declaration of Sta NodeContainer, better inside the loop, to avoid cumulative printing
      for (NodeContainer::Iterator n = Sta.Begin (); n != Sta.End (); n++)
        {
          Ptr<Node> ptr_sta = *n;
          uint32_t id_sta = ptr_sta->GetId ();
          //------------------------------------------std::cout << "Sta_Node Number " << id_sta << std::endl;
        }

      /*---------------------DEVICE SET-UP--------------------------------------------*/
      mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
      apDevices =
          wifi.Install (phy, mac, Ap.Get (m)); //NetDeviceContainer (re)initialized inside loop

      mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing",
                   BooleanValue (false));
      staDevices = wifi.Install (phy, mac, Sta); //NetDeviceContainer (re)initialized inside loop
      std::cout << "Wifi Installed on device: " << "MAME-wifi-ssid_" + std::to_string (id_wifi) << std::endl;

      /* ---------------SetPositionAllocator()  VECTOR OPTION--- */
      /*----Position Allocation for this Cluster of mth_Ap with numStaNodes nodes*/
      Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
      //------Relocating position allocator to INSIDE AP LOOP----6/1/2026--- for MULTI-CLUSTER RUN----//
      Vector3D coord[numStaNodes + 1];
      double x, y, z;
      //while (csv.FetchNextRow())
      //{
      for (uint32_t j = 0; j < (numStaNodes + 1); j++)
        {
          csv.FetchNextRow ();
          bool ok = csv.GetValue (2, x); // (0, x) replaced by 
          ok &= csv.GetValue (3, y); // (1, y)
          ok &= csv.GetValue (4, z); // (2, z)

          if (ok)
            { //positionAlloc->Add(Vector(x, y, z));
              //std::cout<< "coordinate: x = " << x << ", y= " << y << "z = " << z << std::endl;
              coord[j] = ns3::Vector3D (x, y, z);
              //---------------------------------std::cout << "coord: " << coord[j] << std::endl;
              positionAlloc->Add (coord[j]); // j=0 for APnode = ueNode. j=1,2,3... for StaNodes
            }
          else
            {
              std::cerr << "Error reading CSV row" << std::endl;
            }
        //------------------Alternative method for creating coordinates using formula-----------------------//
          // coord[j] = ns3::Vector3D(((pow(-1,j)*j*0.49) + 10.0*m), (-0.25)*j, 0.0);
          // std::cout << "coord: " <<coord[j] << std::endl;
          // positionAlloc->Add (coord[j]);  // j=0 for APnode = ueNode. j=1,2,3... for StaNodes
        }
      //}
      mobility.SetPositionAllocator (positionAlloc);
      //----------------Install the LOCATION-COORDINATES on ALL Nodes ------------//

      mobility.Install (Ap.Get (m)); //Location Vector assigned is (Vector (0.0, 0.0, 0.0))
      mobility.Install (Sta); //Location Vector assigned
      //OR
      // for(uint32_t j=0; j<(numStaNodes); j++)
      // {mobility.Install (Sta.Get(id_sta-m-1)); //Location Vector assigned
      // }

      //======== wifi ---------Internet and Addressing -----//
      stack.Install (Ap.Get (m));
      stack.Install (Sta);

      std::string addr_base = "10.1." + std::to_string (id_wifi) + ".0";
      std::cout << "IP Address LAN: " << addr_base << " =====" << std::endl;
      address.SetBase (Ipv4Address (addr_base.c_str ()), Ipv4Mask ("255.255.255.0"));

      // std::cout << "AP " << m << " assigned IP Address: " << addr_base << std::endl;
      Ipv4InterfaceContainer apInterfaces = address.Assign (apDevices);
      Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

      for (uint32_t s = 0; s < numStaNodes; s++)
        {
          Ptr<WifiNetDevice> staWifi = DynamicCast<WifiNetDevice> (staDevices.Get (s));
          Mac48Address staMac = Mac48Address::ConvertFrom (staWifi->GetAddress ());

          Ipv4Address staIp = staInterfaces.GetAddress (s);

          g_macToIp[staMac] = staIp;
        }

      // Iterate through STA nodes and assign/display IP addresses
      Ptr<Node> ptr_ap = Ap.Get (m);
      Ipv4Address ApIp = ptr_ap->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();

      ipToNode[ApIp] = ptr_ap;
      ipToClusterSize[ApIp] = numStaNodes;

      for (uint32_t j = 0; j < numStaNodes; j++)
      {
        Ptr<Node> staNode = Sta.Get(j);
        Ipv4Address StaIp = staNode->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
        ipToNode[StaIp] = staNode;
        ipToClusterSize[StaIp] = numStaNodes; //------Added on 6/1/2026--- for nAp expansion --------------//
      }
      //----------------------std::cout << "AP Node " << id_ap << " assigned IP Address: " << ApIp << std::endl;
      for (NodeContainer::Iterator n = Sta.Begin (); n != Sta.End (); n++)
        {
          Ptr<Node> ptr_sta = *n;
          uint32_t id_sta = ptr_sta->GetId ();
          Ipv4Address StaIp = ptr_sta->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ();
          //-----------------std::cout << "STA Node " << id_sta << " assigned IP Address: " << StaIp << std::endl;
        }
      std::cout << " \n " << " \n " << std::endl;

      //------------for RSSI logging---------06/14/2026---------------------------//
      Ptr<WifiNetDevice> apWifi = DynamicCast<WifiNetDevice> (apDevices.Get (0));
      Ptr<WifiPhy> apPhy = apWifi->GetPhy ();
      apPhy->TraceConnectWithoutContext ("MonitorSnifferRx", MakeCallback (&ApMonitorSnifferRx));

      //------------------------UDP SERVER#1 on AP-------------------------------------------//
      uint16_t serverPort = 9;
      UdpServerHelper udpServer (serverPort);
      ApplicationContainer serverApps = udpServer.Install (Ap.Get (m));



      //-----------STA nodes::  Adding Randomness to the Start-times--- 05/05/2026----------------//
      Ptr<UniformRandomVariable> startJitter = CreateObject<UniformRandomVariable>();
      double baseStart = 5.0;
      double staggerGap = 0.2;
      double jitterMax = std::min(0.5 * pkt_itvl, 2.0); 
      startJitter->SetAttribute("Min", DoubleValue(-jitterMax));
      startJitter->SetAttribute("Max", DoubleValue( jitterMax));
      //------- Dynamic stop time for Client, Server, and Simulator-------------//
      double latestPossibleStart = baseStart + 0.2 * (nWifi[m] - 1) + jitterMax; //-----Can we change from nWifi[0] to nWifi[m], for Flexibility?
      double clientStopTime = latestPossibleStart + (max_num_pkt - 1) * pkt_itvl + 1.0;// last client's Stop Time
      double serverStopTime = clientStopTime + 5.0; // Server Stop time AFTER last client's Stop Time
      double simStopTime = serverStopTime + 5.0;   // Sim Stop Time AFTER Server Stop time

      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds (serverStopTime));
      //serverApps.Stop (Seconds (610));  
      // Server stays ready to receive, bfore Client starts sending pkt at 2sec
      
      // Simulator::Stop (Seconds (simStopTime));  //---for multicluster---going global---06/02/2026
      if (simStopTime > globalSimStopTime)
      {
        globalSimStopTime = simStopTime;
      }

      //-----------------------UDP Client on each STA: CLIENT#n ----------------------------//
      //for (NodeContainer::Iterator n = Sta.Begin(); n != Sta.End(); n++)
      for (uint32_t j = 0; j < (numStaNodes); j++)
        {
          UdpClientHelper udpClient (apInterfaces.GetAddress (0), serverPort);

          udpClient.SetAttribute ("MaxPackets", UintegerValue (max_num_pkt));
          udpClient.SetAttribute ("Interval", TimeValue (Seconds (pkt_itvl)));
          udpClient.SetAttribute ("PacketSize", UintegerValue (pkt_size));

          ApplicationContainer clientApps = udpClient.Install (Sta.Get (j));  //udpClient.Install (wifiStaNodes.Get (nWifi - 1));
          //--------------------------------------------------------------
          // clientApps.Start (Seconds (1.0 + 0.2 * j));   // stagger STA starts
          // clientApps.Start (Seconds (5.0 + 0.2 * j));   // stagger STA starts
          double baseStart = 5.0 + 0.2 * j; double jitter = startJitter->GetValue();
          double startTime = std::max(2.0, baseStart + jitter);  // stagger STA starts

          clientApps.Start (Seconds (startTime));
          // clientApps.Stop (Seconds (clientStopTime));
          double thisClientStopTime = startTime + (max_num_pkt - 1) * pkt_itvl + 1.0;
          clientApps.Stop (Seconds (thisClientStopTime));
          //clientApps.Stop (Seconds (605)); // 10, 60, 300, 600, 3600
        }
    }
  /*~~~~~~~~~~~~~~~~~~~~~~~~~~~ LOOP ENDs HERE ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
  Simulator::Stop (Seconds (globalSimStopTime));

  //----------------------------------------------------------------------------------------------------------//
  // uint32_t SentPackets = 0;
  // uint32_t ReceivedPackets = 0;
  // uint32_t LostPackets = 0;
  int j = 0; //Flow ID

  float AvgThroughput = 0;
  Time Jitter;
  Time Delay;

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  //-----------------------Adding NetAnim 09/13/2024 ------//
  NS_LOG_INFO ("Run Simulation.");
  // DISABLED for batch/parallel data-generation runs: AnimationInterface writes a
  // full XML trace of every node/packet event across the whole run and is pure
  // overhead when only the FlowMonitor CSV output is needed. Re-enable only for
  // a one-off visualization run with a tiny parameter grid.
  // AnimationInterface anim ("Node-iter-flow-module.xml");
  //-----------------------Adding NetAnim 09/13/2024 ------//

  //-----04282026--Added for tracking packet drops------------//
  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Simulator::Run ();

  std::cout << "------ RSSI sample counts by IP ------" << std::endl;
  for (std::map<Ipv4Address, uint32_t>::const_iterator it = g_rssiCount.begin ();
      it != g_rssiCount.end (); ++it)
  {
    double avg = g_rssiSumDbm[it->first] / it->second;
    std::cout << "IP " << it->first
              << " RSSI samples = " << it->second
              << " Avg RSSI = " << avg << " dBm"
              << std::endl;
  }

  //---------------------------------------------------------------------------------------------------------//
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double pkt_itvl_ms = pkt_itvl * 1000;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin ();
      iter != stats.end (); ++iter)
  {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);

    uint32_t sentPackets = iter->second.txPackets;
    uint32_t receivedPackets = iter->second.rxPackets;
    uint32_t lostPackets = sentPackets - receivedPackets;

    double pktDeliveryRate = 0.0;
    if (sentPackets > 0)
      {
        pktDeliveryRate = ((double) receivedPackets * 100.0) / (double) sentPackets;
      }

    double throughputKbps = 0.0;
    double duration =
        iter->second.timeLastRxPacket.GetSeconds () -
        iter->second.timeFirstTxPacket.GetSeconds ();

    if (duration > 0)
      {
        throughputKbps = iter->second.rxBytes * 8.0 / duration / 1024;
      }

    double delayNs = iter->second.delaySum.ToDouble (ns3::Time::NS);
    double jitterNs = iter->second.jitterSum.ToDouble (ns3::Time::NS);

    std::string delayStr = std::to_string (iter->second.delaySum.GetNanoSeconds ());
    std::string jitterStr = std::to_string (iter->second.jitterSum.GetNanoSeconds ());

    double distance = -1.0;
    if (ipToNode.find(t.sourceAddress) != ipToNode.end() &&
        ipToNode.find(t.destinationAddress) != ipToNode.end())
      {
        Ptr<MobilityModel> srcMob = ipToNode[t.sourceAddress]->GetObject<MobilityModel>();
        Ptr<MobilityModel> dstMob = ipToNode[t.destinationAddress]->GetObject<MobilityModel>();
        distance = srcMob->GetDistanceFrom(dstMob);
      }
    
    uint32_t flowClusterSize = 0;
    if (ipToClusterSize.find(t.sourceAddress) != ipToClusterSize.end())
    {
      flowClusterSize = ipToClusterSize[t.sourceAddress];
    }
    else if (ipToClusterSize.find(t.destinationAddress) != ipToClusterSize.end())
    {
      flowClusterSize = ipToClusterSize[t.destinationAddress];
    }

    //----------for RSSI Logging---06/17/2026---------------------//
    // double avgRssiDbm = 0.0;

    // if (g_rssiCount.find (t.sourceAddress) != g_rssiCount.end () &&
    //     g_rssiCount[t.sourceAddress] > 0)
    // {
    //   avgRssiDbm = g_rssiSumDbm[t.sourceAddress] / g_rssiCount[t.sourceAddress];
    // }
    // else
    // {
    //   avgRssiDbm = -999.0;  // no RSSI sample found
    // }

    double avgRssiDbm = -999.0;

    if (g_rssiCount.find (t.sourceAddress) != g_rssiCount.end () &&
        g_rssiCount[t.sourceAddress] > 0)
    {
      avgRssiDbm = g_rssiSumDbm[t.sourceAddress] / g_rssiCount[t.sourceAddress];
    }
    else if (g_lastValidRssiByIp.find (t.sourceAddress) != g_lastValidRssiByIp.end ())
    {
      avgRssiDbm = g_lastValidRssiByIp[t.sourceAddress];
    }
    else
    {
      avgRssiDbm = g_lastValidRssiGlobal;
    }
    //----------for RSSI Logging---06/17/2026---------------------//

    op_csv << t.sourceAddress << "," << t.destinationAddress << ","
       << depth_conc_inch << "," << vwc << "," << b_density << ","
       << distance << "," << flowClusterSize << ","
       << pkt_itvl_ms << "," << pkt_size << ","
       << sentPackets << "," << receivedPackets << "," << lostPackets << ","
       << avgRssiDbm << "," 
       << pktDeliveryRate << "," << throughputKbps << ","
       << delayNs << "," << jitterNs << ","
       << iter->first << "," << delayStr << "," << jitterStr
       << std::endl;
  } 

      //=====12/21/2024==== Commenting out print commanads for easy parsing====
      // NS_LOG_UNCOND ("----Flow ID:" << iter->first);
      // NS_LOG_UNCOND ("Src Addr " << t.sourceAddress << "  Dst Addr " << t.destinationAddress);
      // NS_LOG_UNCOND ("Sent Packets = " << iter->second.txPackets);
      // NS_LOG_UNCOND ("Received Packets = " << iter->second.rxPackets);
      // NS_LOG_UNCOND ("Lost Packets = " << iter->second.txPackets - iter->second.rxPackets);
      // double dlPacketRate = (double) (iter->second.rxPackets) * 100.0 / (double) (iter->second.txPackets);
      //=====12/21/2024==== Commenting out print commanads for easy parsing====
      // NS_LOG_UNCOND ("Packet delivery rate =" << dlPacketRate << "%");
      // double lostPacketRate = (double) (iter->second.txPackets - iter->second.rxPackets) * 100 / (double) (iter->second.txPackets);
      //=====12/21/2024==== Commenting out print commanads for easy parsing====
      // NS_LOG_UNCOND ("Packet loss rate =" << lostPacketRate << "%");
      // NS_LOG_UNCOND ("Delay =" << iter->second.delaySum);
      // NS_LOG_UNCOND ("Jitter =" << iter->second.jitterSum);
      // NS_LOG_UNCOND ("Throughput =" << iter->second.rxBytes * 8.0 /
      //                                      (iter->second.timeLastRxPacket.GetSeconds () -
      //                                       iter->second.timeFirstTxPacket.GetSeconds ()) /
      //                                      1024
      //                               << "Kbps");


  // NS_LOG_UNCOND ("--------Total Results of the simulation----------" << std::endl);
  // NS_LOG_UNCOND ("Total sent packets  =" << SentPackets); 
  // NS_LOG_UNCOND ("Total Received Packets =" << ReceivedPackets);
  // NS_LOG_UNCOND ("Total Lost Packets =" << LostPackets);
  // NS_LOG_UNCOND ("Packet Loss rate =" << ((double) (LostPackets) * 100) / (double) (SentPackets)
  //                                     << "%");
  // NS_LOG_UNCOND ("Packet delivery rate ="
  //                << ((double) (ReceivedPackets * 100) / (double) (SentPackets)) << "%");
  // NS_LOG_UNCOND ("Average Throughput =" << AvgThroughput << "Kbps");
  // NS_LOG_UNCOND ("End to End Delay =" << Delay);
  // NS_LOG_UNCOND ("End to End Jitter delay =" << Jitter);
  // NS_LOG_UNCOND ("Total Flow id =" << j); 


  // DISABLED for batch/parallel runs: redundant with the CSV output above, and
  // writing a full XML flow-stats file on every single experiment_1() call
  // (potentially tens of thousands of calls) adds significant I/O overhead.
  // Re-enable only for isolated debugging of a single run.
  // monitor->SerializeToXmlFile ("Node-iter-adrs-serial.xml", true, true);
  //-----------------------------------------------------------------------------------------------//

  Simulator::Destroy ();

  return 0;
}


int main (int argc, char *argv[])
{
    // ================= BATCH x DEPTH-GROUP x SHARD SELECTION =================
    // 2 BATCHES (operational params): batchIdx=0 uses the "batch 1" pkt_itvl/
    //   pkt_size arrays, batchIdx=1 uses the complementary "batch 2" arrays
    //   (the spaced-interleaved pattern from your screenshot -- every value
    //   appears in exactly one batch, so batch1+batch2 together = full coverage
    //   with zero overlap).
    // 3 DEPTH-GROUPS per batch: splits the 5 ConcreteDepth values into
    //   {4,8} / {12,16} / {20} -- gives 2 x 3 = 6 SEGMENTS total, run
    //   sequentially (you rest the laptop between segments).
    // 4 SHARDS per segment (run in PARALLEL, one per REMAINING BulkDensity
    //   value -- 0.75 is excluded since your prior run already fixed bi<1
    //   to that value).
    // VWC uses the FULL 6-value range (0.05-0.30) as a full inner loop --
    //   the prior interrupted run's partial data was discarded (action item 1),
    //   so there's no overlap to avoid here.
    //
    // segIdx (0-5) = batchIdx*3 + depthGroupIdx, for convenience in run_segment.sh:
    //   segIdx 0,1,2 -> batch 0 (itvl/pktsize "batch 1"), depth groups 0,1,2
    //   segIdx 3,4,5 -> batch 1 (itvl/pktsize "batch 2"), depth groups 0,1,2
    int segIdx = -1;      // REQUIRED: 0..5
    int shardIdx = -1;    // REQUIRED: 0..3 -> selects b_density_arr[shardIdx]
    bool calibrate = false;
    std::string outBaseDir = "scratch/Network_flow_MAME";

    CommandLine cmd (__FILE__);
    cmd.AddValue ("segIdx", "Segment index 0-5 (batchIdx*3 + depthGroupIdx)", segIdx);
    cmd.AddValue ("shardIdx", "Index into b_density_arr[] for this SHARD (0-3)", shardIdx);
    cmd.AddValue ("calibrate", "Run a tiny timing probe instead of the full grid (true/false)", calibrate);
    cmd.AddValue ("outBaseDir", "Base output directory", outBaseDir);
    cmd.Parse (argc, argv);

    // ================= PARAMETER GRID =================
    // VWC: full 6-value range. BulkDensity: REMAINING 4 values (0.75 excluded
    // -- already fixed by the prior run's bi<1). Depth keeps its full range
    // since that run never got meaningfully past depth=4.
    double vwc_arr[]       = {0.05, 0.10, 0.15, 0.20, 0.25, 0.30}; // full 6-value range, full inner loop
    double b_density_arr[] = {1.25, 1.75, 2.25, 2.75};             // 4 remaining values -> 4 shards
    double depth_groups[3][2] = { {4, 8}, {12, 16}, {20, -1} };    // -1 = unused slot (group 2 has only 1 depth)
    int depth_group_sizes[3]  = { 2, 2, 1 };

    // "Batch 1" / "Batch 2" pkt_itvl and pkt_size arrays -- the spaced,
    // non-overlapping split from your screenshot's active/commented arrays:
    //   full-10 itvl set   = {0.1,0.25,0.5,0.75,1.0,2.5,5.0,7.5,10,20}
    //   batch1 = even idx  = {0.1, 0.5, 1.0, 5.0, 10}     (already your active array)
    //   batch2 = odd idx   = {0.25, 0.75, 2.5, 7.5, 20}   (complementary remainder)
    //   full-10 pktsize set = {256,512,768,1024,2048,4096,5120,6144,8192,10240}
    //   batch1 = even idx  = {256, 768, 2048, 5120, 8192}       (already your active array)
    //   batch2 = odd idx   = {512, 1024, 4096, 6144, 10240}     (complementary remainder)
    double pkt_itvl_batches[2][5]     = { {0.1, 0.5, 1.0, 5.0, 10.0}, {0.25, 0.75, 2.5, 7.5, 20.0} };
    uint32_t pkt_size_batches[2][5]   = { {256, 768, 2048, 5120, 8192}, {512, 1024, 4096, 6144, 10240} };

    const uint32_t max_num_pkt = 50;   // fixed, matches your tested run -- not swept in this pass
    uint32_t N_REPS = 1;               // repeats per combo -- 1 for speed given the deadline;
                                        // raise to 2-3 later for a statistical-robustness top-up pass

    uint32_t n_vwc   = sizeof(vwc_arr)/sizeof(vwc_arr[0]);
    uint32_t n_bd    = sizeof(b_density_arr)/sizeof(b_density_arr[0]);
    uint32_t n_itvl  = 5;
    uint32_t n_pktsz = 5;

    if (calibrate) {
        n_vwc = 1; n_itvl = 2; n_pktsz = 2; N_REPS = 1;
    }

    if (segIdx < 0 || segIdx > 5) {
        std::cerr << "ERROR: pass --segIdx=0..5 (batchIdx*3 + depthGroupIdx)\n";
        return 1;
    }
    if (shardIdx < 0 || shardIdx >= (int)n_bd) {
        std::cerr << "ERROR: pass --shardIdx=0.." << (n_bd-1) << " to select this SHARD (BulkDensity)\n";
        return 1;
    }
    int batchIdx = segIdx / 3;
    int depthGroupIdx = segIdx % 3;
    int nDepthInGroup = calibrate ? 1 : depth_group_sizes[depthGroupIdx];
    double b_density = b_density_arr[shardIdx];

    // ================= PER-SEGMENT / PER-SHARD OUTPUT FOLDER =================
    // e.g. scratch/Network_flow_MAME/segment3_batch1_depthgrp0/shard2_bd2.250000/*.csv
    std::ostringstream segDirStream;
    segDirStream << outBaseDir << "/segment" << segIdx << "_batch" << batchIdx << "_depthgrp" << depthGroupIdx;
    std::ostringstream shardDirStream;
    shardDirStream << segDirStream.str() << "/shard" << shardIdx << "_bd"
                   << std::fixed << std::setprecision(6) << b_density;
    std::string shardDir = shardDirStream.str();
    std::string mkcmd = "mkdir -p " + shardDir;
    if (std::system(mkcmd.c_str()) != 0) {
        std::cerr << "ERROR: could not create output dir " << shardDir << "\n";
        return 1;
    }

    uint64_t totalFiles = (uint64_t)nDepthInGroup * n_vwc * n_itvl;
    uint64_t totalCalls = totalFiles * n_pktsz * N_REPS;
    std::cout << "==== SEGMENT " << segIdx << " (batch=" << batchIdx << " depthGroup=" << depthGroupIdx
              << ", " << nDepthInGroup << " depth value(s)) SHARD " << shardIdx << " (bd=" << b_density << ") "
              << (calibrate ? "[CALIBRATION MODE]" : "") << " ====\n"
              << "Output dir: " << shardDir << "\n"
              << "Files to create: " << totalFiles << "\n"
              << "experiment_1() calls: " << totalCalls << std::endl;

    auto wallStart = std::chrono::steady_clock::now();
    uint64_t callsDone = 0;

    for (int di = 0; di < nDepthInGroup; di++) {
        double depth_conc_inch = calibrate ? 4.0 : depth_groups[depthGroupIdx][di];
        for (uint32_t vi = 0; vi < n_vwc; vi++) {
            double vwc = vwc_arr[vi];
            for (uint32_t ii = 0; ii < n_itvl; ii++) {
                double pkt_itvl = calibrate ? (ii == 0 ? 0.1 : 0.5) : pkt_itvl_batches[batchIdx][ii];

                std::cout << "************ Opening new csv file: depth=" << depth_conc_inch
                          << " bd=" << b_density << " vwc=" << vwc << " pkt_itvl=" << pkt_itvl << " ***********\n";
                std::string filename =
                    "concDepth" + std::to_string(depth_conc_inch) +
                    "_vwc" + std::to_string(vwc) +
                    "_bd" + std::to_string(b_density) +
                    "_pkitvl" + std::to_string(pkt_itvl) +
                    "_op_flow_wifi.csv";
                std::string fullpath = shardDir + "/" + filename;

                op_csv.open(fullpath, std::ios::app);
                if (!op_csv.is_open()) {
                    std::cerr << "ERROR: could not open " << fullpath << std::endl;
                    return 1;
                }
                op_csv << "SourceIP,DestinationIP,"
                       << "ConcreteDepth_inch,VWC,BulkDensity,Distance_m,ClusterSize,"
                       << "Pkt_Intvl (ms),Pkt_Size (Byte),#SentPackets,#ReceivedPackets,#LostPackets,"
                       << "RSSI (dBm),"
                       << "Pkt_Deliv_Rate (%),AvgThroughput (kbps),Delay (ns),Jitter (ns),"
                       << "Flow ID,DelayStr,JitterStr\n";

                for (uint32_t j = 0; j < n_pktsz; j++) {
                    uint32_t pkt_size = calibrate ? (j == 0 ? 256 : 768) : pkt_size_batches[batchIdx][j];
                    for (uint32_t k = 0; k < N_REPS; k++) {
                        experiment_1(pkt_itvl, pkt_size, max_num_pkt, vwc, b_density, depth_conc_inch);
                        callsDone++;

                        if (callsDone % 10 == 0 || callsDone == totalCalls) {
                            auto now = std::chrono::steady_clock::now();
                            double elapsedSec = std::chrono::duration<double>(now - wallStart).count();
                            double pct = 100.0 * callsDone / totalCalls;
                            double eta = (callsDone > 0) ? (elapsedSec / callsDone) * (totalCalls - callsDone) : 0.0;
                            std::cout << "[seg=" << segIdx << " shard=" << shardIdx << "] " << callsDone << "/" << totalCalls
                                      << " (" << std::fixed << std::setprecision(1) << pct << "%) "
                                      << "elapsed=" << elapsedSec << "s  ETA=" << eta << "s" << std::endl;
                        }
                    }
                }
                op_csv.close();
            }
        }
    }

    auto wallEnd = std::chrono::steady_clock::now();
    double totalSec = std::chrono::duration<double>(wallEnd - wallStart).count();
    std::cout << "==== SEGMENT " << segIdx << " SHARD " << shardIdx << " DONE in " << totalSec << " sec ("
              << totalSec/3600.0 << " h) ====" << std::endl;

    if (calibrate) {
        const uint64_t nodesPerCall = 52;  // matches nWifi sum for this validated topology
        uint64_t pktEvents = totalCalls * nodesPerCall * max_num_pkt;
        double rate = pktEvents / totalSec;
        std::cout << "Approx packet-events this probe: " << pktEvents << "\n"
                  << "Measured rate: " << rate << " pkt-events/sec  <-- plug this into capacity_calculator.py"
                  << std::endl;
    }

    return 0;
}