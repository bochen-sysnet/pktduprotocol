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
#include "ns3/random-variable-stream.h"
//#include "ns3/gtk-config-store.h"
#include <mutex>
#include <string>
#include <fstream>

using namespace ns3;
using namespace std;
/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("EpcFirstExample");

// global variables
// track all events of packets 
uint32_t numberOfPackets = 1000;
float simTime = 5;
// use backup
bool useBackup = true;
// Packet loss ratio
float plr = 0.2;
// Duplicate ratio
float dupRate = 0.3;
// Retransmission time out
float retran_time_out = 0.25;
EventId* allevents;
Time playTime(Seconds(0));
bool* hasPacket;
uint32_t usedPacket;
DataRate playDataRate("3Mbps");
DataRate streamDataRate("4Mbps");
uint32_t pktSize = 1018;
Time playDuration (Seconds (pktSize * 8 / static_cast<double> (playDataRate.GetBitRate ())));

// Critical section
mutex SrcRcv;
mutex DstRcv;

// data analysis
float backup_hit = 0;
float backup_total = 0;
float major_hit = 0;
float major_total = 0;
Time finishing_time(Seconds(0));
Time buffering_time(Seconds(0));
Time waiting_time(Seconds(0));
Time src_last_time(Seconds(0));
Time dst_last_time(Seconds(0));



// static void
// RxDrop (Ptr<const Packet> p);

void
dstSocketRecv (Ptr<Socket> socket);

void
dstSocketRecv2 (Ptr<Socket> socket);

void srcSocketRecv (Ptr<Socket> socket);

void srcSocketRecv2 (Ptr<Socket> socket);

bool bDuplicate(double rate);



class YHeader : public Header
{
public:
  // must be implemented to become a valid new header.
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);
  virtual void Print (ostream &os) const;

  // allow protocol-specific access to the header data.
  void SetData (uint32_t data);
  void SetEventId (EventId event_id);
  uint32_t GetData (void) const;
  EventId GetEventId (void) const;
private:
  uint32_t m_data;
  EventId m_event_id;
};

void 
YHeader::SetEventId (EventId event_id)
{
  m_event_id = event_id;
}

EventId
YHeader::GetEventId (void) const
{
  return m_event_id;
}

void 
YHeader::SetData (uint32_t data)
{
  m_data = data;
}
uint32_t
YHeader::GetData (void) const
{
  return m_data;
}

TypeId
YHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("YHeader")
    .SetParent<Header> ()
    .AddConstructor<YHeader> ()
  ;
  return tid;
}
TypeId
YHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t 
YHeader::GetSerializedSize (void) const
{
  return 6;
}
void 
YHeader::Serialize (Buffer::Iterator start) const
{
  // The 2 byte-constant
  start.WriteU8 (0xfe);
  start.WriteU8 (0xef);
  // The data.
  start.WriteHtonU32 (m_data);
}
uint32_t 
YHeader::Deserialize (Buffer::Iterator start)
{
  uint8_t tmp;
  tmp = start.ReadU8 ();
  NS_ASSERT (tmp == 0xfe);
  tmp = start.ReadU8 ();
  NS_ASSERT (tmp == 0xef);
  m_data = start.ReadNtohU32 ();
  return 6; // the number of bytes consumed.
}

void 
YHeader::Print (ostream &os) const
{
  os << "data=" << m_data;
}



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
  void AddHost(Ptr<Socket> socket,Address address);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);
  void ReSendPacket (uint32_t data);

  Ptr<Socket>     m_socket;
  Ptr<Socket>	  m_socket2;
  Address         m_peer;
  Address         m_peer2;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_socket2(0),
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
MyApp::Setup (Ptr<Socket> socket, Address address, 
	uint32_t packetSize, uint32_t nPackets, 
	DataRate dataRate)
{
  m_socket = socket;
  m_socket->SetRecvCallback (MakeCallback (&srcSocketRecv));
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
MyApp::ReSendPacket (uint32_t data)
{
	Ptr<Packet> packet = Create<Packet> (m_packetSize);
	YHeader yHeader;
	yHeader.SetData (data);
	// Schedule for timeout
	Time timeout (Seconds(retran_time_out));
	EventId event_id = Simulator::Schedule (timeout, &MyApp::ReSendPacket, this, data);
	// copy the new scheduled event
	uint64_t ts = event_id.GetTs();
	EventImpl * impl = event_id.PeekEventImpl();
	uint32_t context = event_id.GetContext();
	uint32_t uid = event_id.GetUid();
	allevents[data] = EventId ( impl, ts, context, uid);
	// yHeader.SetEventId(event_id);
	packet->AddHeader (yHeader);
	m_socket->Send (packet);
	major_total += 1;
	// NS_LOG_INFO ("MyApp::ReSendPacket "<<data
	// 	<<" sent "<<data<<"/"<<m_nPackets
	// 	<<" trace event id:"<< event_id.GetUid()
	// 	<<" at " << Simulator::Now ().GetSeconds ());
}

void
MyApp::SendPacket (void)
{
	
	// Create packet for the delayed one
	Ptr<Packet> packet = Create<Packet> (m_packetSize);
	YHeader yHeader;
	yHeader.SetData (m_packetsSent);
	// Schedule for timeout
	Time timeout (Seconds(0.2));
	EventId event_id = Simulator::Schedule (timeout, &MyApp::ReSendPacket, this, m_packetsSent);
	uint64_t ts = event_id.GetTs();
	EventImpl * impl = event_id.PeekEventImpl();
	uint32_t context = event_id.GetContext();
	uint32_t uid = event_id.GetUid();
	allevents[m_packetsSent] = EventId ( impl, ts, context, uid);
	// yHeader.SetEventId(event_id);
	packet->AddHeader (yHeader);
	m_socket->Send (packet);
	if (m_packetsSent == numberOfPackets - 1)
		src_last_time = Simulator::Now ();
	major_total += 1;
	// NS_LOG_INFO ("MyApp::SendPacket "<<yHeader.GetData()
	// 	<<" sent "<<m_packetsSent<<"/"<<m_nPackets
	// 	<<" trace event id:"<<event_id.GetUid()
	// 	<<" at " << Simulator::Now ().GetSeconds ());
	if (m_packetsSent%100 == 0)	
		NS_LOG_INFO(m_packetsSent<<" packets sent.");
	if (bDuplicate(dupRate))
	{
		m_socket2->Send(packet);
		backup_total += 1;
	}
	m_packetsSent += 1;
	

  if (m_packetsSent < m_nPackets)
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

void 
MyApp::AddHost(Ptr<Socket> socket, Address address)
{
	m_socket2 = socket;
	m_peer2 = address;
	m_socket2->SetRecvCallback (MakeCallback (&srcSocketRecv2));
	m_socket2->Bind ();
	m_socket2->Connect (m_peer2);
}

// Try to add this as a recv call back
// map the uid to index of packets
// make the total to be sent packets 2xN
// can timeout be implemented?
// or just recalculate the time as 
// t1---t2
// t1---t1+10ms
// label the packet which is just after the unordered ack


int
main (int argc, char *argv[])
{
  LogComponentEnable("EpcFirstExample", LOG_LEVEL_ALL);
  uint16_t numberOfNodes = 1;
  double distance = 60.0;
  bool useCa = false;
  string strpdr = "3Mbps";
  string strsdr = "4Mbps";


  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.AddValue("plr", "Packet loss ratio", plr);
  cmd.AddValue("useBackup", "Whether to use a backup server", useBackup);
  cmd.AddValue("numberOfPackets", "Number of simulated packets", numberOfPackets);
  cmd.AddValue("playDataRate", "Play data rate", strpdr);
  cmd.AddValue("streamDataRate", "Stream data rate", strsdr);
  cmd.AddValue("dupRate", "Duplicate ratio", dupRate);
  cmd.AddValue("retran_time_out", "Retransmission time out", retran_time_out);
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

  // Create some data
  allevents = new EventId[numberOfPackets];
  hasPacket = new bool[numberOfPackets];
  usedPacket = numberOfPackets+1;
  float start_time = 0.0;
  float end_time = start_time + simTime;
  for(uint16_t i=0; i<numberOfPackets;++i)
  	hasPacket[i]=false;
  DataRate playDataRate(strpdr);
  DataRate streamDataRate(strsdr);
  uint32_t pktSize = 1024;
  playDuration = Time(Seconds (pktSize * 8 / static_cast<double> (playDataRate.GetBitRate ())));

  ostringstream ss;
  ss << "result/"<<numberOfPackets<<'_'<<strpdr<<'_'<<strsdr<<'_'
  	<<plr<<'_'<<dupRate<<'_'<<retran_time_out<<'_'<<useBackup ;
  string outfilename(ss.str());


  // Topology
  Ptr<Node> pgw = epcHelper->GetPgwNode ();

   // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (2);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  Ptr<Node> remoteHost2 = remoteHostContainer.Get (1);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  p2ph.SetQueue ("ns3::DropTailQueue", "MaxPackets", UintegerValue(100000));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  NetDeviceContainer internetDevices2 = p2ph.Install (pgw, remoteHost2);

  // Error model
  // Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  // em->SetAttribute ("ErrorRate", DoubleValue (0.001));
  // internetDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (numberOfPackets);
  RateErrorModel error_model;
  error_model.SetRandomVariable (uv);
  error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model.SetRate (plr);
  internetDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (&error_model));

  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  ipv4h.SetBase ("2.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces2 = ipv4h.Assign (internetDevices2);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);
  Ipv4Address remoteHostAddr2 = internetIpIfaces2.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = 
        ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting2 = 
        ipv4RoutingHelper.GetStaticRouting (remoteHost2->GetObject<Ipv4> ());
  remoteHostStaticRouting2->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

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
  uint16_t ulPort = 2000;
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      ++ulPort;
      // Source
      Address sinkAddress, sinkAddress2;
      Ptr<Socket> srcSocket, srcSocket2;
      Ptr<MyApp> app;
      // First; remote host
	  sinkAddress = InetSocketAddress (remoteHostAddr, ulPort);
	  srcSocket = Socket::CreateSocket (ueNodes.Get(u), UdpSocketFactory::GetTypeId ());
	  app = CreateObject<MyApp> ();
	  app->Setup (srcSocket, sinkAddress, pktSize, numberOfPackets, streamDataRate);
	  ueNodes.Get(u)->AddApplication (app);
	  app->SetStartTime (Seconds (start_time));
	  app->SetStopTime (Seconds (end_time));
	  // Second remote host
	  sinkAddress2 = InetSocketAddress (remoteHostAddr2, ulPort);
	  srcSocket2 = Socket::CreateSocket (ueNodes.Get(u), UdpSocketFactory::GetTypeId ());
	  app->AddHost(srcSocket2 ,sinkAddress2);
	  
	  // Destination1
	  Ptr<Socket> dstSocket = Socket::CreateSocket (remoteHost, UdpSocketFactory::GetTypeId ());
	  dstSocket->Bind (sinkAddress);
	  dstSocket->SetRecvCallback (MakeCallback (&dstSocketRecv));
	  // Destination2
	  Ptr<Socket> dstSocket2 = Socket::CreateSocket (remoteHost2, UdpSocketFactory::GetTypeId ());
	  dstSocket2->Bind (sinkAddress2);
	  dstSocket2->SetRecvCallback (MakeCallback (&dstSocketRecv2));

      NS_LOG_DEBUG("UE:"<<ueIpIface.GetAddress (u));
      NS_LOG_DEBUG("Host:"<<internetIpIfaces.GetAddress (1)<<' '<< ulPort);
    }
    NS_LOG_DEBUG("GW:"<<internetIpIfaces.GetAddress (0)<< ' '<<epcHelper->GetUeDefaultGatewayAddress ());


  // lteHelper->EnableTraces ();
  // Uncomment to enable PCAP tracing
  // p2ph.EnablePcapAll("lena-epc-first");
  AsciiTraceHelper ascii;
  p2ph.EnableAsciiAll (ascii.CreateFileStream ("test-simple-epc.tr"));

  // internetDevices.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

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
  flowmonHelper.SerializeToXmlFile ("test-simple-epc.flowmon", true, true);
  // Data analysis
  ofstream data_analysis(outfilename);
  data_analysis<<"plr:"<<plr<<" dupRate:"<<dupRate<<" useBackup:"<<useBackup
  				<<" numberOfPackets:"<<numberOfPackets<<" playDataRate:"<<strpdr
  				<<" streamDataRate:"<<strsdr<<" retran_time_out:"<<retran_time_out<<endl;
  data_analysis<<"major hit rate = "<<(major_hit/major_total)
  								<<' '<<major_hit<<' '<<major_total<<endl;
  data_analysis<<"backup hit rate = "<<(backup_hit/backup_total)
  								<<' '<<backup_hit<<' '<<backup_total<<endl;
  data_analysis<<"Finishing time = "<<finishing_time.GetSeconds ()<<endl;
  data_analysis<<"Buffering time = "<<buffering_time.GetSeconds ()<<endl;
  data_analysis<<"Waiting time = "<<waiting_time.GetSeconds ()<<endl;
  data_analysis<<"Last packet at source = "<<src_last_time.GetSeconds()<<endl;
  data_analysis<<"Last packet at destination = "<<dst_last_time.GetSeconds()<<endl;
  Simulator::Destroy();
  return 0;

}

void
dstSocketRecv (Ptr<Socket> socket)
{
	Address from;
	Ptr<Packet> packet = socket->RecvFrom (from);
	InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
	socket->SendTo (packet, 0, InetSocketAddress (address.GetIpv4 (),address.GetPort ()));
	packet->RemoveAllPacketTags ();
	packet->RemoveAllByteTags ();
	YHeader yHeader;
	packet->RemoveHeader (yHeader);
	uint32_t data = yHeader.GetData ();
	// EventId event_id = yHeader.GetEventId ();
	packet->AddHeader (yHeader);
	Time curTime = Simulator::Now ();
	// NS_LOG_INFO ("dstSocketRecv "<< data<<' '<< packet->GetSize () << " bytes from "
 //  			 				<< address.GetIpv4 ()<< ':'<<address.GetPort ()
 //  			 				<<" at " << curTime.GetSeconds ());
	if (data==0)
		waiting_time = curTime;
	if (data == numberOfPackets - 1)
		dst_last_time = curTime;
	hasPacket[data] = true;
	DstRcv.lock();
	if ( (data==0 && usedPacket==numberOfPackets+1) || data ==  uint32_t( usedPacket+1))
	{
		// Start playing
		buffering_time = buffering_time + curTime - playTime;
		usedPacket = data;
		playTime = curTime + playDuration;
		// NS_LOG_INFO("playto: "<<usedPacket<<" at "<<playTime.GetSeconds ());
		while (usedPacket < numberOfPackets - 1 && hasPacket[usedPacket+1])
		{
			playTime = playTime + playDuration;
			usedPacket += 1;
			// NS_LOG_INFO("play buffered packets "<<usedPacket<<" at "<<playTime.GetSeconds ());
		}
		if (usedPacket == numberOfPackets - 1)
			finishing_time = playTime;
	}
	DstRcv.unlock();
}

void
dstSocketRecv2 (Ptr<Socket> socket)
{
	if (false == useBackup)
		return;
	Address from;
	Ptr<Packet> packet = socket->RecvFrom (from);
	InetSocketAddress address = InetSocketAddress::ConvertFrom (from);
	socket->SendTo (packet, 0, InetSocketAddress (address.GetIpv4 (),address.GetPort ()));
	packet->RemoveAllPacketTags ();
	packet->RemoveAllByteTags ();
	YHeader yHeader;
	packet->RemoveHeader (yHeader);
	uint32_t data = yHeader.GetData ();
	// EventId event_id = yHeader.GetEventId ();
	packet->AddHeader (yHeader);
	Time curTime = Simulator::Now ();
	// NS_LOG_INFO ("BACKUP--------dstSocketRecv2 "<< data<<' '<< packet->GetSize () << " bytes from "
 //  			 				<< address.GetIpv4 ()<< ':'<<address.GetPort ()
 //  			 				<<" at " << curTime.GetSeconds ());
	if (data==0)
		waiting_time = curTime;
	if (data == numberOfPackets - 1)
		dst_last_time = curTime;
	hasPacket[data] = true;
	DstRcv.lock();
	if ( (data==0 && usedPacket==numberOfPackets+1) || data ==  uint32_t( usedPacket+1))
	{
		// Start playing
		buffering_time = buffering_time + curTime - playTime;
		usedPacket = data;
		playTime = curTime + playDuration;
		// NS_LOG_INFO("playto: "<<usedPacket<<" at "<<playTime.GetSeconds ());
		while (usedPacket < numberOfPackets - 1 && hasPacket[usedPacket+1])
		{
			playTime = playTime + playDuration;
			usedPacket += 1;
			// NS_LOG_INFO("play buffered packets "<<usedPacket<<" at "<<playTime.GetSeconds ());
		}
		if (usedPacket == numberOfPackets - 1)
			finishing_time = playTime;
	}
	DstRcv.unlock();
}

void srcSocketRecv (Ptr<Socket> socket)
{
	Address from;
	Ptr<Packet> packet = socket->RecvFrom (from);
	YHeader yHeader;
	packet->RemoveHeader (yHeader);
	uint32_t data = yHeader.GetData ();
	// NS_LOG_INFO ("srcSocketRecv "<<"data:"<<data<<' '
	// 				<< packet->GetSize () << " bytes from " 
	// 				<<" trace event id:"<<allevents[data].GetUid()<<' '
	// 				<< InetSocketAddress::ConvertFrom (from).GetIpv4 ()<<" at " 
	// 				<< Simulator::Now ().GetSeconds ());
	// Critical section
	SrcRcv.lock();
	if (allevents[data].IsRunning ())
    {
      Simulator::Cancel (allevents[data]);
      major_hit += 1;
    }
    SrcRcv.unlock();
  // cancel its next event
}

void srcSocketRecv2 (Ptr<Socket> socket)
{
	if (false == useBackup)
		return;
	Address from;
	Ptr<Packet> packet = socket->RecvFrom (from);
	YHeader yHeader;
	packet->RemoveHeader (yHeader);
	uint32_t data = yHeader.GetData ();
	// NS_LOG_INFO ("BACKUP--------srcSocketRecv2 "<<"data:"<<data<<' '
	// 				<< packet->GetSize () << " bytes from " 
	// 				<<" trace event id:"<<allevents[data].GetUid()<<' '
	// 				<< InetSocketAddress::ConvertFrom (from).GetIpv4 ()<<" at " 
	// 				<< Simulator::Now ().GetSeconds ());
    SrcRcv.lock();
	if (allevents[data].IsRunning ())
    {
      Simulator::Cancel (allevents[data]);
      backup_hit += 1;
      // NS_LOG_INFO ("BACKUP hit "<<"data:"<<data<< ' '<< Simulator::Now ().GetSeconds ());
    }
    // else
    // {
    	// NS_LOG_INFO ("BACKUP miss "<<"data:"<<data<< ' '<< Simulator::Now ().GetSeconds ());
    // }
    SrcRcv.unlock();
}

// static void
// RxDrop (Ptr<const Packet> p)
// {
// 	uint32_t uid = p->GetUid ();
// 	// YHeader yHeader;
// 	// p->RemoveHeader (yHeader);
// 	// uint32_t netid = yHeader.GetId ();
//   NS_LOG_UNCOND (" RxDrop UID:"<<uid <<" RxDrop at " << Simulator::Now ().GetSeconds ());
// }

bool bDuplicate(double rate)
{
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
	double threshold = uv->GetMin() + (uv->GetMax()-uv->GetMin())*rate;
	return threshold > uv->GetValue();
}

// label pkts and identify lost pkts
// lte lossy model
// new topology
// add new host and dependency
// (new host send back ack to  cancel schedule) need longer delay to show
// tune parameters
// variation of transmission speed and channel delay