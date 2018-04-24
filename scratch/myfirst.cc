/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("ScdtServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  int numNodes = 2;
  nodes.Create (numNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  ScdtServerHelper rootServer (interfaces.GetAddress (1), 9, 1);

  ApplicationContainer serverApps = rootServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  ScdtServerHelper treenodes (interfaces.GetAddress (1), 9, 0);
  treenodes.SetAttribute ("MaxPackets", UintegerValue (1));
  treenodes.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  treenodes.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = treenodes.Install (nodes.Get (0));
  
  uint16_t port = 500;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer tcpApp = sinkHelper.Install (nodes.Get(0));
  std::cout << interfaces.GetAddress(0);
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));
  tcpApp.Start (Seconds(1.0));
  tcpApp.Stop (Seconds(10.0));
    
  

  Simulator::Run ();
  Simulator::Destroy ();
  Ptr<PacketSink> sink1 = DynamicCast<PacketSink> (tcpApp.Get(0));
  std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << std::endl; 
  return 0;
}
