/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Jaume Nin <jaume.nin@cttc.cat>
 */

#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/error-model.h"
//#include "ns3/gtk-config-store.h"

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("Mytopo");

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, 
  	uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    ;
  return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}


static void
RxDrop (Ptr<const Packet> p)
{
	uint32_t uid = p->GetUid ();
  NS_LOG_UNCOND ("UID:"<<uid <<" RxDrop at " << Simulator::Now ().GetSeconds ());
}

int
main (int argc, char *argv[])
{
  LogComponentEnable("Mytopo", LOG_LEVEL_ALL);
  uint16_t numberOfNodes = 2;
  double simTime = 10;
  double distance = 60.0;
  double interPacketInterval = 100;
  bool useCa = false;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("numberOfNodes", "Number of eNodeBs + UE pairs", numberOfNodes);
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue("interPacketInterval", "Inter packet interval [ms])", interPacketInterval);
  cmd.AddValue("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.Parse(argc, argv);

  if (useCa)
   {
     Config::SetDefault ("ns3::LteHelper::UseCa", BooleanValue (useCa));
     Config::SetDefault ("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue (2));
     Config::SetDefault ("ns3::LteHelper::EnbComponentCarrierManager", StringValue ("ns3::RrComponentCarrierManager"));
   }

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  // Path loss model
  // lteHelper->SetAttribute ("PathlossModel", StringValue ("ns3::Cost231PropagationLossModel"));

  Ptr<PointToPointEpcHelper>  epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults();

  // parse again so you can override default values from the command line
  cmd.Parse(argc, argv);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a router and a remote host: PGW->rtr->remotehost
  Ptr<Node> nRtr = CreateObject<Node> ();
  Ptr<Node> remoteHost = CreateObject<Node> ();

  // Create p2p links
  NodeContainer nPgwRtr = NodeContainer (pgw, nRtr);
  NodeContainer nRtrRemoteHost = NodeContainer (nRtr, remoteHost);

  // Install internet stack
  InternetStackHelper internet;
  internet.Install (nRtrRemoteHost);

  // Set link attributes
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer dPgwRtr = p2ph.Install (nPgwRtr);
  NetDeviceContainer dRtrRemoteHost = p2ph.Install (nRtrRemoteHost);

  // Error model
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  RateErrorModel error_model;
  error_model.SetRandomVariable (uv);
  error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model.SetRate (0.1);
  dPgwRtr.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (&error_model));
  dRtrRemoteHost.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (&error_model));

  // Set ip address
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iPgwRtr = ipv4.Assign (dPgwRtr);
  ipv4.SetBase ("10.10.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iRtrRemoteHost = ipv4.Assign (dRtrRemoteHost);
  
  Ipv4Address remoteHostAddr = iRtrRemoteHost.GetAddress (1);

  Ptr<Ipv4> ipv4pgw = pgw->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4Rtr = nRtr->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4remoteHost = remoteHost->GetObject<Ipv4> ();

// Add rouing info!!!!!!!!!!!!!!!!
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> staticRoutingpgw = ipv4RoutingHelper.GetStaticRouting (ipv4pgw);
  Ptr<Ipv4StaticRouting> staticRoutingRtr = ipv4RoutingHelper.GetStaticRouting (ipv4Rtr);
  Ptr<Ipv4StaticRouting> staticRoutingremoteHost = ipv4RoutingHelper.GetStaticRouting (ipv4remoteHost);

  // staticRoutingRtr->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  // staticRoutingremoteHost->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  staticRoutingremoteHost->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"),
                        "10.10.1.1", 1);
  staticRoutingRtr->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"),
                        "10.1.1.1", 1);
  staticRoutingpgw->AddHostRouteTo (Ipv4Address ("10.10.1.2"), Ipv4Address ("10.1.1.2"), 1);
  staticRoutingRtr->AddHostRouteTo (Ipv4Address ("10.10.1.2"), Ipv4Address ("10.10.1.2"), 2);


  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create(numberOfNodes);
  ueNodes.Create(numberOfNodes);

  // Install Mobility Model
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint16_t i = 0; i < numberOfNodes; i++)
    {
      positionAlloc->Add (Vector(distance * i, 0, 0));
    }
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(enbNodes);
  mobility.Install(ueNodes);

  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach one UE per eNodeB
  for (uint16_t i = 0; i < numberOfNodes; i++)
      {
        lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
        // side effect: the default EPS bearer will be activated
      }


  // Install and start applications on UEs and remote host
  uint16_t dlPort = 1234;
  uint16_t ulPort = 2000;
  float start_time = 0.0;
  float end_time = 10.0;
  ApplicationContainer serverApps;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      ++dlPort;
      ++ulPort;
      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", 
                        InetSocketAddress (Ipv4Address::GetAny (), dlPort));
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", 
                        InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(u)));
      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

      // My application
      Address sinkAddress = InetSocketAddress (ueIpIface.GetAddress (u), dlPort);
      Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (remoteHost, 
      							UdpSocketFactory::GetTypeId ());
	  Ptr<MyApp> app = CreateObject<MyApp> ();
	  app->Setup (ns3TcpSocket, sinkAddress, 1024, 1000, DataRate ("1Mbps"));
	  remoteHost->AddApplication (app);
	  app->SetStartTime (Seconds (start_time));
	  app->SetStopTime (Seconds (end_time));

	  sinkAddress = InetSocketAddress (remoteHostAddr, ulPort);
	  ns3TcpSocket = Socket::CreateSocket (ueNodes.Get(u), UdpSocketFactory::GetTypeId ());
	  app = CreateObject<MyApp> ();
	  app->Setup (ns3TcpSocket, sinkAddress, 1024, 1000, DataRate ("1Mbps"));
	  ueNodes.Get(u)->AddApplication (app);
	  app->SetStartTime (Seconds (start_time));
	  app->SetStopTime (Seconds (end_time));

      NS_LOG_DEBUG("UE:"<<ueIpIface.GetAddress (u)<<' '<< dlPort);
      NS_LOG_DEBUG("Host:"<<iRtrRemoteHost.GetAddress (1)<<' '<< ulPort);
    }
    NS_LOG_DEBUG("GW:"<<iRtrRemoteHost.GetAddress (0)<< ' '<<epcHelper->GetUeDefaultGatewayAddress ());
  serverApps.Start (Seconds (0.01));

  // lteHelper->EnableTraces ();
  // Uncomment to enable PCAP tracing
  // p2ph.EnablePcapAll("lena-epc-first");
  AsciiTraceHelper ascii;
  p2ph.EnableAsciiAll (ascii.CreateFileStream ("mytopo.tr"));

  dRtrRemoteHost.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

  // Pathloss calculation

  // Flow monitor
  FlowMonitorHelper flowmonHelper;
  flowmonHelper.InstallAll ();
  

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/
  
  // print the pathloss values at the end of the simulation
  
  // flow monitor output
  flowmonHelper.SerializeToXmlFile ("mytopo.flowmon", true, true);
  Simulator::Destroy();
  return 0;

}


// label pkts and identify lost pkts
// lte lossy model
// new topology
// video transfer rate
// feedback control