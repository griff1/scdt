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
 *
 */

#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/brite-module.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BriteExample");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_ALL);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_ALL);

  LogComponentEnable ("BriteExample", LOG_LEVEL_ALL);

  // BRITE needs a configuration file to build its graph. By default, this
  // example will use the TD_ASBarabasi_RTWaxman.conf file. There are many others
  // which can be found in the BRITE/conf_files directory
  std::string confFile = "src/brite/examples/conf_files/TD_ASBarabasi_RTWaxman.conf";
  bool tracing = false;
  bool nix = false;

  CommandLine cmd;
  cmd.AddValue ("confFile", "BRITE conf file", confFile);
  cmd.AddValue ("tracing", "Enable or disable ascii tracing", tracing);
  cmd.AddValue ("nix", "Enable or disable nix-vector routing", nix);

  cmd.Parse (argc,argv);

  nix = false;

  // Invoke the BriteTopologyHelper and pass in a BRITE
  // configuration file and a seed file. This will use
  // BRITE to build a graph from which we can build the ns-3 topology
  BriteTopologyHelper bth (confFile);
  bth.AssignStreams (3);

  PointToPointHelper p2p;


  InternetStackHelper stack;

  if (nix)
    {
      Ipv4NixVectorHelper nixRouting;
      stack.SetRoutingHelper (nixRouting);
    }

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.252");

  bth.BuildBriteTopology (stack);
  bth.AssignIpv4Addresses (address);

  NS_LOG_INFO ("Number of AS created " << bth.GetNAs ());

  // Install scdt software on random nodes
  int maxOverlays = 100;
  Address overlays[maxOverlays];
  int numOverlays = 0;
  NodeContainer overlayContainer;

  for (int i = 0; i < bth.getNAs(); i++) {
    for (int j = 0; j < bth.getNNodesForAs(i)) {
      if (rand() > RAND_MAX / 4) {
        continue;
      }
      if (numOverlays == maxOverlays) {
        break;
      }
      Ptr<Node> curNode = bth.GetNodeForAs(i, j);
       
      Ptr<NetDevice> netdiv = curNode->GetDevice(0);
      Address ip = netdiv->GetAddress();
      overlays[numOverlays] = ip;   
 
      overlayContainer.add(curNode);
   
      numOverlays++;
    }
    if (numOverlays == maxOverlays) {
      break;
    }
  }  
  ScdtServerHelper scdtServer (overlays);
  scdtServer.Install(overlayContainer);

  if (!nix)
    {
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  if (tracing)
    {
      AsciiTraceHelper ascii;
      p2p.EnableAsciiAll (ascii.CreateFileStream ("briteLeaves.tr"));
    }
  // Run the simulator
  Simulator::Stop (Seconds (6.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
