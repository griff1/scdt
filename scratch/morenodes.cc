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
#include <stdio.h>
#include <time.h>

#define NUM_NODES 40
#define SENDTCPTIME 20

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int
main (int argc, char *argv[])
{
  srand (time (NULL));
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("ScdtServerApplication", LOG_LEVEL_INFO);

  NodeContainer c;
  c.Create(NUM_NODES);

  InternetStackHelper stack;
  stack.Install (c);

  NodeContainer root = NodeContainer(c.Get(0));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");


  Ipv4InterfaceContainer interface; 
  //NodeContainer allNodes;
  //allNodes = NodeContainer(root.Get(0));
  for (int i = 1; i < NUM_NODES; i++) {
    NodeContainer treenode;
    treenode.Add(c.Get(i));
    NetDeviceContainer device;
    NodeContainer combined = NodeContainer(root.Get(0), treenode.Get(0));
    //allNodes.Add(treenode.Get(0));
    
    PointToPointHelper pointToPoint;
    char dataRate[4];
    sprintf(dataRate, "%dMbps", rand() % 100 + 1);
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
    char delay[4];
    sprintf(delay, "%dms", rand() % 100 + 1);
    pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));


    device = pointToPoint.Install (combined);
    char temp[15];     
    sprintf(temp, "10.1.%d.0", i);
    address.SetBase(temp, "255.255.255.0");
    interface = address.Assign(device); 
    ScdtServerHelper treenodes (interface.GetAddress (0), 9, 0);
    treenodes.SetAttribute ("MaxPackets", UintegerValue (1));
    treenodes.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    treenodes.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = treenodes.Install (treenode.Get(0));
    
    /*uint16_t port = 500;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    //clientApps.Add(sinkHelper.Install (treenode.Get(0)));
    ApplicationContainer tcpApp = sinkHelper.Install (treenode.Get(0));
    tcpApp.Start (Seconds (1.0));
    tcpApp.Stop (Seconds (10.0));*/

  
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (100.0));
  }
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  // tried to do this, but didn't work
  //NodeContainer temp = NodeContainer(allNodes.Get(5), allNodes.Get(1));
  //NetDeviceContainer t = pointToPoint.Install(temp);

  ScdtServerHelper rootServer (interface.GetAddress (0), 9, 1);

  ApplicationContainer serverApps = rootServer.Install (root.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (100.0));


  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
