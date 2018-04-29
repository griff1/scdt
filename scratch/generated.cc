#include "ns3/core-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/v4ping-helper.h"
#include "ns3/v4ping.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManualExample");

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  /* Configuration. */

  LogComponentEnable ("ScdtServerApplication", LOG_LEVEL_INFO);

  /* Build nodes. */
  NodeContainer router_0;
  router_0.Create (1);
  NodeContainer router_1;
  router_1.Create (1);
  NodeContainer router_2;
  router_2.Create (1);
  NodeContainer router_3;
  router_3.Create (1);
  NodeContainer router_4;
  router_4.Create (1);

  NodeContainer clients;
  clients.Add (router_1);
  clients.Add (router_2);
  clients.Add (router_3);
  clients.Add (router_4);

  NS_LOG_INFO ("Created nodes");

  /* Build link. */
  CsmaHelper csma_hub_0;
  csma_hub_0.SetChannelAttribute ("DataRate", DataRateValue (100000000));
  csma_hub_0.SetChannelAttribute ("Delay",  TimeValue (MilliSeconds (10000)));
  CsmaHelper csma_hub_1;
  csma_hub_1.SetChannelAttribute ("DataRate", DataRateValue (100000000));
  csma_hub_1.SetChannelAttribute ("Delay",  TimeValue (MilliSeconds (10000)));
  CsmaHelper csma_hub_2;
  csma_hub_2.SetChannelAttribute ("DataRate", DataRateValue (100000000));
  csma_hub_2.SetChannelAttribute ("Delay",  TimeValue (MilliSeconds (10000)));
  CsmaHelper csma_hub_3;
  csma_hub_3.SetChannelAttribute ("DataRate", DataRateValue (100000000));
  csma_hub_3.SetChannelAttribute ("Delay",  TimeValue (MilliSeconds (10000)));

  NS_LOG_INFO ("Created links");

  /* Build link net device container. */

  NodeContainer all_hub_0;
  all_hub_0.Add (router_0);
  all_hub_0.Add (router_1);
  NetDeviceContainer ndc_hub_0 = csma_hub_0.Install (all_hub_0);
  NodeContainer all_hub_1;
  all_hub_1.Add (router_0);
  all_hub_1.Add (router_2);
  NetDeviceContainer ndc_hub_1 = csma_hub_1.Install (all_hub_1);
  NodeContainer all_hub_2;
  all_hub_2.Add (router_3);
  all_hub_2.Add (router_0);
  NetDeviceContainer ndc_hub_2 = csma_hub_2.Install (all_hub_2);
  NodeContainer all_hub_3;
  all_hub_3.Add (router_0);
  all_hub_3.Add (router_4);
  NetDeviceContainer ndc_hub_3 = csma_hub_3.Install (all_hub_3);

  /*
  NetDeviceContainer allDevices ();
  allDevices.Add (all_hub_0);
  allDevices.Add (all_hub_1);
  allDevices.Add (all_hub_2);
  allDevices.Add (all_hub_3);
  */

  NS_LOG_INFO ("Created Net Device Containers");

  /* Install the IP stack. */
  InternetStackHelper internetStackH;
  internetStackH.Install (router_0);
  internetStackH.Install (router_1);
  internetStackH.Install (router_2);
  internetStackH.Install (router_3);
  internetStackH.Install (router_4);

  NS_LOG_INFO ("Created IP Stack");  

  /* IP assign. */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  //Ipv4InterfaceContainer allIps = ipv4.Assign (allDevices);

  
  Ipv4InterfaceContainer iface_ndc_hub_0 = ipv4.Assign (ndc_hub_0);
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_ndc_hub_1 = ipv4.Assign (ndc_hub_1);
  ipv4.SetBase ("10.0.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_ndc_hub_2 = ipv4.Assign (ndc_hub_2);
  ipv4.SetBase ("10.0.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iface_ndc_hub_3 = ipv4.Assign (ndc_hub_3);
 

  NS_LOG_INFO ("IPs assigned");

  /* Generate Route. */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Generate Application. */
  //InetSocketAddress root = InetSocketAddress (iface_ndc_hub_2.GetAddress(0));
  ScdtServerHelper rootHelper = ScdtServerHelper (iface_ndc_hub_2.GetAddress(0), 9, 1);
  ScdtServerHelper clientHelper = ScdtServerHelper (iface_ndc_hub_2.GetAddress(0), 9, 0);
  ApplicationContainer root_cont = rootHelper.Install(router_0.Get(0));
  root_cont.Start (Seconds (1.1));
  root_cont.Stop (Seconds (10.1));

  ApplicationContainer client_cont = clientHelper.Install(clients);
  client_cont.Start (Seconds (1.1));
  client_cont.Stop (Seconds (10.1));

  NS_LOG_INFO ("Applications created");

  /* Simulation. */
  /* Pcap output. */
  /* Stop the simulation after x seconds. */
  uint32_t stopTime = 11;
  Simulator::Stop (Seconds (stopTime));
  /* Start and clean simulation. */
  Simulator::Run ();
  Simulator::Destroy ();
}
