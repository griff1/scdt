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
#include<stdio.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("ScdtServerApplication", LOG_LEVEL_INFO);

  NodeContainer root;
  root.Create(1);
  int numNodes = 5;
  InternetStackHelper stack;
  stack.Install (root);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  
  Ipv4AddressHelper address;
  address.SetBase ("10.1.0.0", "255.255.0.0");


  
 Ipv4InterfaceContainer interface; 
  for (int i = 0; i < numNodes; i++) {
    NodeContainer treenode;
    treenode.Create(1);
    stack.Install(treenode);
    NetDeviceContainer device;
    NodeContainer combined = NodeContainer(root.Get(0), treenode.Get(0));
    
    device = pointToPoint.Install (combined);
    char temp[15];     
    sprintf(temp, "10.1.%d.0", i+1);
    address.SetBase(temp, "255.255.255.0");
    interface = address.Assign(device); 
    ScdtServerHelper treenodes (interface.GetAddress (0), 9, 0);
    treenodes.SetAttribute ("MaxPackets", UintegerValue (1));
    treenodes.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    treenodes.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = treenodes.Install (treenode.Get(0));
  
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));
  }
  ScdtServerHelper rootServer (interface.GetAddress (0), 9, 1);

  ApplicationContainer serverApps = rootServer.Install (root.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));


  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
