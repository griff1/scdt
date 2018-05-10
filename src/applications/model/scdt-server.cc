/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 */
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "scdt-server.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4.h"

namespace ns3 {

const uint8_t ATTACH[] = "ATTACH";
const uint8_t PING[] = "PING";
const uint8_t PING_RESP[] = "PINGRESPONSE";
const uint8_t TRY_RESP[] = "TRY";
const uint8_t ATTACH_SUC[] = "SUCCESSATTACH";
const uint8_t NACK[] = "NACK";
const uint8_t REATTACH[] = "REATTACH";

double latencyDiff = 0;

NS_LOG_COMPONENT_DEFINE ("ScdtServerApplication");

NS_OBJECT_ENSURE_REGISTERED (ScdtServer);

TypeId
ScdtServer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ScdtServer")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<ScdtServer> ()
    .AddAttribute ("MaxPackets", 
                   "The maximum number of packets the application will send",
                   UintegerValue (100),
                   MakeUintegerAccessor (&ScdtServer::m_count),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Interval", 
                   "The time to wait between packets",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&ScdtServer::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("RemoteAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&ScdtServer::m_peerAddress),
                   MakeAddressChecker ())
    .AddAttribute ("RemotePort", 
                   "The destination port of the outbound packets",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ScdtServer::m_peerPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PacketSize", "Size of echo data in outbound packets",
                   UintegerValue (100),
                   MakeUintegerAccessor (&ScdtServer::SetDataSize,
                                         &ScdtServer::GetDataSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("IsRoot", "Boolean if node is/is not root",
                   UintegerValue (0),
                   MakeUintegerAccessor (&ScdtServer::m_isRoot),
                   MakeUintegerChecker<uint8_t> ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&ScdtServer::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}

ScdtServer::ScdtServer ()
{
  NS_LOG_FUNCTION (this << "CONSTRUCTING");
  m_sent = 0;
  m_socket = 0;
  m_sendEvent = EventId ();
  m_data = 0;
  m_dataSize = 0;

  Address m_rootIp (m_peerAddress);
  m_rootPort = m_peerPort;

  m_children = new Address[MAX_FANOUT];
  m_childrenPorts = new uint16_t[MAX_FANOUT];
  m_shortestPing = new double[MAX_FANOUT];
  m_childrenSockets = new Ptr<Socket>[MAX_FANOUT];

  m_pings = new Address[MAX_PINGS];
  m_pingStartTime = new double[MAX_PINGS];
  m_pingTime = new double[MAX_PINGS];
  m_numPings = 0;
  m_numChildren = 0;

  m_serializedChildrenSize = 1;
  m_serializedChildren = new uint8_t[1];
}

ScdtServer::~ScdtServer()
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;

  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;

  delete [] m_children;
  delete [] m_childrenPorts;
  delete [] m_shortestPing;
  delete [] m_pings;
  delete [] m_pingStartTime;
  delete [] m_pingTime;
  delete [] m_serializedChildren;
}

void
ScdtServer::SetRemote (Address rootIp, uint16_t rootPort, bool isRoot)
{
  m_rootIp = Address(rootIp);
  m_rootPort = rootPort;
  
  m_children = new Address[MAX_FANOUT];
  m_childrenPorts = new uint16_t[MAX_FANOUT];
  m_shortestPing = new double[MAX_FANOUT];

  m_isRoot = isRoot;

  m_pings = new Address[MAX_PINGS];
  m_pingStartTime = new double[MAX_PINGS];
  m_pingTime = new double[MAX_PINGS];
  m_numPings = 0;
  m_numChildren = 0;

  m_serializedChildrenSize = 1;
  m_serializedChildren = new uint8_t[1];
}

void 
ScdtServer::SetRemote (Address ip, uint16_t port)
{
  NS_LOG_FUNCTION (this << ip << port);
  m_peerAddress = ip;
  m_peerPort = port;
}

void 
ScdtServer::SetRemote (Address addr)
{
  NS_LOG_FUNCTION (this << addr);
  m_peerAddress = addr;
}

void
ScdtServer::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

void
ScdtServer::DoSetup (void)
{
  memcpy (&m_rootIp, &m_peerAddress, sizeof (Address));
  m_rootPort = m_peerPort;
  
  m_pktCntr = 0;

  m_children = new Address[MAX_FANOUT];
  m_childrenPorts = new uint16_t[MAX_FANOUT];
  m_shortestPing = new double[MAX_FANOUT];

  m_pings = new Address[MAX_PINGS];
  m_pingStartTime = new double[MAX_PINGS];
  m_pingTime = new double[MAX_PINGS];
  m_numPings = 0;
  m_numChildren = 0;

  m_cache = new uint8_t[CACHE_SIZE];
  m_cacheStarts = new int64_t[CACHE_SIZE / BLOCK_SIZE];
  for (int i = 0; i < CACHE_SIZE / BLOCK_SIZE; i++) 
    {
      m_cacheStarts[i] = -1;
    }
  m_cacheEnds = new uint32_t[CACHE_SIZE / BLOCK_SIZE];

  m_serializedChildrenSize = 1;
  m_serializedChildren = new uint8_t[1];

  m_nextPotentialParentPing = 9999999;
}

void 
ScdtServer::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  srand (time (NULL));
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      ScdtServer::DoSetup();
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (Ipv4Address::IsMatchingType(m_rootIp) == true)
        {
          InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), m_rootPort);
          if (m_socket->Bind (local) == -1) 
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
      else
        {
          uint8_t len = m_rootIp.GetLength ();
          uint8_t buf[len + 1];
          m_rootIp.CopyAllTo(buf, len);
          buf[len] = '\0';
          //NS_LOG_INFO (buf);
          NS_ASSERT_MSG (false, "Incompatible address type: " << m_rootIp);
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&ScdtServer::HandleRead, this));
  m_socket->SetAllowBroadcast (true);
  
  if (!m_isRoot) 
    {
      m_parentIp = m_rootIp;
      m_parentPort = m_rootPort;

      m_socket->SendTo (ATTACH, 7, 0, InetSocketAddress (Ipv4Address::ConvertFrom (m_rootIp), m_rootPort));
      //std::string cmd ("ATTACH");
      //ScdtServer::SetFill(cmd);
      //ScheduleTransmit (Seconds (0.), &ScdtServer::TryAttach);
    }
  else 
    {
      //Simulator::Schedule (Seconds (3.5), &ScdtServer::ChangeConfig, this);
      Simulator::Schedule (Seconds (100), &ScdtServer::SendData, this);
    }
  //NS_LOG_INFO ("Successfully started application");
}

void
ScdtServer::ChangeConfig () 
{
  //Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (0.5));
}

void
ScdtServer::SendData () 
{
  //NS_LOG_INFO("Sending data");
  uint32_t start = 0;
 // uint8_t someFrigginData[15];
  double curTime = Simulator::Now().GetSeconds();
  uint32_t dataSize = 10000;
  //NS_LOG_INFO ("bin_string: " << bin_string);
  uint8_t toSend[dataSize + sizeof (start)];
  //memcpy (&toSend[sizeof (start)], someFrigginData, dataSize);
  

  /*uint8_t toSend2[dataSize + sizeof (start)];
  uint8_t someOtherFrigginData[] = "OtherData1";
  uint32_t start2 = 10;
  uint8_t* bin_string2 = (uint8_t *)&start2;
  memcpy (toSend2, bin_string2, sizeof (start));
  memcpy (&toSend2[sizeof (start)], someOtherFrigginData, dataSize);*/
  memcpy(&toSend[sizeof(start)], &curTime, sizeof(double));
  memset (&toSend[sizeof(start) + sizeof(double)], 'b', dataSize - sizeof(double));
  for (int i = 0; i < m_numChildren; i++) 
    {
      start = 0;
      for (int j = 0; j < 10; j++) {
        memcpy(toSend, &start, sizeof(uint32_t));

        ScdtServer::UpdateCache(toSend, dataSize + sizeof (start));
        m_socket->SendTo (toSend, dataSize + sizeof (start), 0, m_children[i]);
        start += 10000;
      }
      //ScdtServer::UpdateCache (toSend2, 10 + sizeof (start));
      
      //m_socket->SendTo (toSend2, 10 + sizeof (start), 0, m_children[i]);
    }
  for (int i = 0; i < m_numChildren; i++) {
    m_socket->SendTo (toSend, dataSize + sizeof (start), 0, m_children[i]);
  }
}

void 
ScdtServer::StopApplication ()
{
  //NS_LOG_INFO ("pktCntr: " << m_pktCntr);
     /*NS_LOG_INFO ("Current cache: " << m_cache[1000]);
          NS_LOG_INFO ("More cache: " << m_cache[11000]);
          NS_LOG_INFO ("More cache: " << m_cache[21000]);
          NS_LOG_INFO ("More cache: " << m_cache[31000]);
          NS_LOG_INFO ("More cache: " << m_cache[41000]);
          NS_LOG_INFO ("More cache: " << m_cache[51000]);
          NS_LOG_INFO ("More cache: " << m_cache[61000]);
          NS_LOG_INFO ("More cache: " << m_cache[71000]);
          NS_LOG_INFO ("More cache: " << m_cache[81000]);
          NS_LOG_INFO ("More cache: " << m_cache[91000]);
*/
    if (!m_isRoot) { 
    std::ofstream file;
        file.open("times.txt", std::fstream::app);
        file << latencyDiff << "\n";
        file.close();
    }

  NS_LOG_FUNCTION (this);
  //NS_LOG_INFO ("Node " << GetNode ()->GetId () << ":");
  //NS_LOG_INFO ("curAddress " << GetNode ()->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ());

  //NS_LOG_INFO ("Caches: " << m_cache);
  //NS_LOG_INFO ("Cache starts: " << m_cacheStarts);

  /*for (int i = 0; i < m_numChildren; i++) 
    {
      InetSocketAddress curChild = InetSocketAddress::ConvertFrom (m_children[i]);
      //NS_LOG_INFO ("-- " << curChild.GetIpv4 ());
    }*/

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  Simulator::Cancel (m_sendEvent);
}

void 
ScdtServer::SetDataSize (uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << dataSize);

  //
  // If the client is setting the echo packet data size this way, we infer
  // that she doesn't care about the contents of the packet at all, so 
  // neither will we.
  //
  delete [] m_data;
  m_data = 0;
  m_dataSize = 0;
  m_size = dataSize;
}

uint32_t 
ScdtServer::GetDataSize (void) const
{
  NS_LOG_FUNCTION (this);
  return m_size;
}

void 
ScdtServer::SetFill (std::string fill)
{
  NS_LOG_FUNCTION (this << fill);

  uint32_t dataSize = fill.size () + 1;

  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memcpy (m_data, fill.c_str (), dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
ScdtServer::SetFill (uint8_t fill, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  memset (m_data, fill, dataSize);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
ScdtServer::SetFill (uint8_t *fill, uint32_t fillSize, uint32_t dataSize)
{
  NS_LOG_FUNCTION (this << fill << fillSize << dataSize);
  if (dataSize != m_dataSize)
    {
      delete [] m_data;
      m_data = new uint8_t [dataSize];
      m_dataSize = dataSize;
    }

  if (fillSize >= dataSize)
    {
      memcpy (m_data, fill, dataSize);
      m_size = dataSize;
      return;
    }

  //
  // Do all but the final fill.
  //
  uint32_t filled = 0;
  while (filled + fillSize < dataSize)
    {
      memcpy (&m_data[filled], fill, fillSize);
      filled += fillSize;
    }

  //
  // Last fill may be partial
  //
  memcpy (&m_data[filled], fill, dataSize - filled);

  //
  // Overwrite packet size attribute.
  //
  m_size = dataSize;
}

void 
ScdtServer::ScheduleTransmit (Time dt)
{
  NS_LOG_FUNCTION (this << dt);
  m_sendEvent = Simulator::Schedule (dt, &ScdtServer::Send, this);
}

void 
ScdtServer::Send (void)
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT (m_sendEvent.IsExpired ());

  Ptr<Packet> p;
  if (m_dataSize)
    {
      //
      // If m_dataSize is non-zero, we have a data buffer of the same size that we
      // are expected to copy and send.  This state of affairs is created if one of
      // the Fill functions is called.  In this case, m_size must have been set
      // to agree with m_dataSize
      //
      NS_ASSERT_MSG (m_dataSize == m_size, "ScdtServer::Send(): m_size and m_dataSize inconsistent");
      NS_ASSERT_MSG (m_data, "ScdtServer::Send(): m_dataSize but no m_data");
      p = Create<Packet> (m_data, m_dataSize);
    }
  else
    {
      //
      // If m_dataSize is zero, the client has indicated that it doesn't care
      // about the data itself either by specifying the data size by setting
      // the corresponding attribute or by not calling a SetFill function.  In
      // this case, we don't worry about it either.  But we do allow m_size
      // to have a value different from the (zero) m_dataSize.
      //
      p = Create<Packet> (m_size);
    }
  // call to the trace sinks before the packet is actually sent,
  // so that tags added to the packet can be sent as well
  m_txTrace (p);
  m_socket->Send (p);

  ++m_sent;

  /*if (Ipv4Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv4Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }
  else if (Ipv6Address::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Ipv6Address::ConvertFrom (m_peerAddress) << " port " << m_peerPort);
    }
  else if (InetSocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () << " port " << InetSocketAddress::ConvertFrom (m_peerAddress).GetPort ());
    }
  else if (Inet6SocketAddress::IsMatchingType (m_peerAddress))
    {
      NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client sent " << m_size << " bytes to " <<
                   Inet6SocketAddress::ConvertFrom (m_peerAddress).GetIpv6 () << " port " << Inet6SocketAddress::ConvertFrom (m_peerAddress).GetPort ());
    }
*/
  if (m_sent < m_count) 
    {
      ScheduleTransmit (m_interval);
    }
}


uint32_t
ScdtServer::SendPing (Ptr<Socket> socket, Address & dest) 
{
  //NS_LOG_INFO ("Sending ping...");
  uint32_t curNumPings = m_numPings;      

  memcpy (&m_pings[m_numPings], &dest, sizeof (Address));
  m_pingStartTime[m_numPings] = Simulator::Now ().GetSeconds ();
  m_pingTime[m_numPings++] = 99999999;
  socket->SendTo (PING, 5, 0, dest);

  m_numPings = m_numPings % MAX_PINGS;

  return curNumPings;
}

void
ScdtServer::InterpretPacket (Ptr<Socket> socket, Address & from, uint8_t* contents, uint32_t size) 
{
  // Handle attach request
  if (memcmp (contents, ATTACH, 7) == 0) 
    {
      ScdtServer::SendPing (socket, from);
    }
  // Handle ping request by sending a ping response
  else if (memcmp (contents, PING, 5) == 0) 
    {
      //NS_LOG_INFO ("Returning ping...");
      socket->SendTo (PING_RESP, 13, 0, from);
    }
  // Handle response to initiated ping
  else if (memcmp (contents, PING_RESP, 13) == 0) 
    {
      //NS_LOG_LOGIC ("Received ping response");
          
      for (uint32_t i = 0; i < m_numPings; i++) 
        {
          if (m_pings[i] == from) 
            {
              // TODO: Convert to a priority queue?
              m_pingTime[i] = Simulator::Now ().GetSeconds () - m_pingStartTime[i];
              if (m_possibleParentsSet.count (i) != 0) 
                {
                  m_possibleParentsCntr--;
                  m_possibleParentsSet.erase (i);
                }
              else 
                {
                  ScdtServer::UpdateChildren (m_pings[i], m_pingTime[i]);
                  break;
                }
              if (m_possibleParentsCntr == 0) 
                {
                  int32_t curBestIndex = -1;
                  std::stack <uint32_t> discardedParents;
                  while (m_possibleParentsStk.size() != 0) 
                    {
                      uint32_t curIndex = m_possibleParentsStk.top();
                      m_possibleParentsStk.pop();
                         
                      //NS_LOG_INFO ("PING TIME: " << m_pingTime[curIndex]);

                      if (m_pingTime[curIndex] < m_nextPotentialParentPing)
                        {
                          if (curBestIndex != -1) 
                            {
                              discardedParents.push (curBestIndex);
                            }
                          curBestIndex = curIndex;
                          memcpy (&m_nextPotentialParent, &m_pings[curIndex], sizeof (Address));
                          m_nextPotentialParentPing = m_pingTime[curIndex];
                        }
                    }
                  while (discardedParents.size () != 0) 
                    {
                      m_possibleParentsStk.push (discardedParents.top());
                      discardedParents.pop();
                    }
                  m_nextPotentialParentPing = 999999;
                  socket->SendTo (ATTACH, 7, 0, m_nextPotentialParent);
                }
              break;
            }
        }
    }
  else if (memcmp (contents, REATTACH, 8) == 0) 
    {
      //NS_LOG_INFO ("Received REATTACH " << GetNode ()->GetId());
      m_parentIp = m_rootIp;
      m_parentPort = m_rootPort;
      ScdtServer::DoSetup ();
      m_socket->SendTo (ATTACH, 7, 0, InetSocketAddress (Ipv4Address::ConvertFrom (m_rootIp), m_rootPort));
    }
  // Handle addresses of additional attach points to try
  else if (memcmp (contents, TRY_RESP, 3) == 0)
    {
      // uint8_t numEntries = contents[3];
      //NS_LOG_INFO (this << "\nhandling TRY");
      uint32_t cntr = 4;
      m_possibleParentsCntr = contents[3];
      while (cntr < size) 
        {
          uint32_t childSize = contents[cntr + 1];
          Address curAddr;
          curAddr.CopyAllFrom (&contents[cntr], childSize + 2);
          cntr += childSize + 2;
          //InetSocketAddress curChild = InetSocketAddress::ConvertFrom (curAddr);
          //NS_LOG_INFO ("possible parent --" << curChild.GetIpv4 ());

          uint32_t index = ScdtServer::SendPing (m_socket, curAddr);
          m_possibleParentsStk.push (index);
          m_possibleParentsSet.insert (index);
        }
    }
  // Set our parent now that we've successfully attached
  else if (memcmp (contents, ATTACH_SUC, 14) == 0) 
    {
      memcpy(&m_parentIp, &from, sizeof (Address));
      //NS_LOG_INFO ("Received ATTACH_SUC  on node " << GetNode ()->GetId());
    }
  else if (memcmp (contents, NACK, 4) == 0) 
    {
      uint32_t start_byte;
      memcpy (&start_byte, &contents[4], sizeof (start_byte));
      uint32_t orig_start_byte = start_byte;

      // Round down to next largest block start
      start_byte = start_byte - (start_byte % BLOCK_SIZE);
      if (m_cacheStarts[(start_byte % CACHE_SIZE) / BLOCK_SIZE] == orig_start_byte) 
        {
          uint8_t resp[BLOCK_SIZE + sizeof (start_byte)];
          memcpy (resp, &orig_start_byte, sizeof (orig_start_byte));
          memcpy (&resp[sizeof(orig_start_byte)], &m_cache[start_byte], BLOCK_SIZE);
          m_socket->SendTo (resp, BLOCK_SIZE + sizeof (orig_start_byte), 0, from);
        }
      else 
        {
          m_socket->SendTo (contents, size, 0, m_parentIp);
        }
    }
  else 
    {
      // Forward packet to all children
      //NS_LOG_INFO ("Received packet to forward with contents: " << contents);
      // Drops 10% of packets
      /*uint32_t val = rand();
      if (val % 100 == 0 || val % 100 == 1 || val % 100 == 2) 
      //if (contents[4] == 'R')
        {
       //   NS_LOG_INFO ("Simulated packet drop");
        }
      else 
        {
*/
        double curTime = Simulator::Now().GetSeconds();
        double oldTime;
        ScdtServer::UpdateCache (contents, size);
        if (m_pktCntr == 10) {
          memcpy(&oldTime, &contents[sizeof(uint32_t)], sizeof(double));
          latencyDiff = curTime - oldTime;
        }
          for (int i = 0; i < m_numChildren; i++) 
            {
              m_socket->SendTo(contents, size, 0, m_children[i]);
            }
        //}
    }
}

void
ScdtServer::UpdateCache (uint8_t* contents, uint32_t size) 
{
  uint32_t start_byte;
  memcpy (&start_byte, contents, sizeof (start_byte));
  
  start_byte = start_byte - (start_byte % BLOCK_SIZE);
  uint32_t orig_start_byte = start_byte;
  start_byte = start_byte % CACHE_SIZE;
  uint32_t num_blocks = (size - sizeof (start_byte)) / BLOCK_SIZE;
  if (num_blocks == 0) 
    {
      num_blocks = 1;
    }
  // TODO: Fix allocation across cache wraparound
  memcpy (&m_cache[start_byte], &contents[sizeof (start_byte)], size - sizeof (start_byte));
  
  if (m_cacheStarts[(start_byte / BLOCK_SIZE)] == -1) 
    {
      m_pktCntr++;
    }
  for (uint8_t i = 0; i < num_blocks; i++) 
    {
      m_cacheStarts[(start_byte / BLOCK_SIZE) + i] = (int64_t) (orig_start_byte + (BLOCK_SIZE * i));
    }
  //NS_LOG_INFO ("Start byte: " << start_byte);
  //NS_LOG_INFO ("Data: " << m_cache);

  if (m_isRoot) 
    {
      return;
    }  
 
  bool wrapped = false; 
  const uint32_t NUM_BLOCKS = CACHE_SIZE / BLOCK_SIZE;
  uint8_t cntr = 1;
  for (uint32_t i = (start_byte / BLOCK_SIZE) - 1; 
       i != (start_byte / BLOCK_SIZE) % NUM_BLOCKS; 
       i--) 
    {
      if (i == 4294967295) 
        {
          i = NUM_BLOCKS - 1;
          wrapped = true;
        }
      if ((m_cacheStarts[i] == -1 && !wrapped) || 
          (m_cacheStarts[i] != m_cacheStarts [i + 1] - BLOCK_SIZE && m_cacheStarts[i] != -1)) 
        {
          //NS_LOG_INFO ("Sent NACK: ");
          uint8_t toSend[4 + sizeof(uint32_t)];
          memcpy (toSend, NACK, 4);
          uint32_t byteToReq = orig_start_byte - (cntr * BLOCK_SIZE);
          memcpy (&toSend[4], &byteToReq, sizeof(uint32_t));
          m_socket->SendTo(toSend, 4 + sizeof (uint32_t), 0, m_parentIp);
        }
      cntr++; 
    }
}

void
ScdtServer::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
    {
      uint8_t contents[packet->GetSize ()];
      packet->CopyData (contents, packet->GetSize ());
     
      //NS_LOG_INFO ("Received packet on node " << GetNode ()->GetId () << " with contents " << contents);
      //NS_LOG_INFO ("IP: " << GetNode ()->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal ()); 
      ScdtServer::InterpretPacket (socket, from, contents, packet->GetSize ()); 
   
      /*if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }
      else if (Inet6SocketAddress::IsMatchingType (from))
        {
          NS_LOG_INFO ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       Inet6SocketAddress::ConvertFrom (from).GetIpv6 () << " port " <<
                       Inet6SocketAddress::ConvertFrom (from).GetPort ());
        }*/
      //NS_LOG_INFO ("\n\n");
    }
}

void
ScdtServer::UpdateChildren (Address & addr, double pingTime) 
{
  // Add child because MAX_FANOUT not used yet
  if (m_numChildren < MAX_FANOUT) 
    {
      memcpy (&m_children[m_numChildren], &addr, sizeof (Address));
      //Address a (addr);
      //m_children[m_numChildren] = a;
      m_shortestPing[m_numChildren] = pingTime;
      m_numChildren++;
      ScdtServer::SerializeChildren ();
      m_socket->SendTo (ATTACH_SUC, 14, 0, addr);
      return;
    }

  // Update shortest ping if new ping is for existing child
  for (int i = 0; i < m_numChildren; i++) 
    {
      if (memcmp (&addr, &m_children[i], sizeof (Address)) == 0) 
        {
          if (pingTime < m_shortestPing[i]) 
            {
              m_shortestPing[i] = pingTime;
            }
          return;
        }
    }

  // Determine if other node has shorter ping and make it child
  double maxPingTime = 999999999;
  uint8_t maxPingIndex = 0;
  for (int i = 0; i < m_numChildren; i++) 
    {
      if (m_shortestPing[i] < maxPingTime) 
        {
          maxPingTime = m_shortestPing[i];
          maxPingIndex = i;
        }
    }
  if (pingTime < maxPingTime - (maxPingTime / 10)) 
    {
      Address oldAddr;
      memcpy (&oldAddr, &m_children[maxPingIndex], sizeof(Address));
      memcpy (&m_children[maxPingIndex], &addr, sizeof (Address));
      m_shortestPing[maxPingIndex] = pingTime;

      m_socket->SendTo (REATTACH, 8, 0, oldAddr);

      m_socket->SendTo (ATTACH_SUC, 14, 0, addr);
      ScdtServer::SerializeChildren ();
    }
  else 
    {
      m_socket->SendTo (m_serializedChildren, m_serializedChildrenSize, 0, addr);
    }
}

void
ScdtServer::SerializeChildren () 
{
  m_serializedChildrenSize = 4;
  for (int i = 0; i < m_numChildren; i++) 
    {
      m_serializedChildrenSize += m_children[i].GetSerializedSize();
    }
  delete [] m_serializedChildren;
  m_serializedChildren = new uint8_t[m_serializedChildrenSize];
  m_serializedChildren[0] = 'T';
  m_serializedChildren[1] = 'R';
  m_serializedChildren[2] = 'Y';
  m_serializedChildren[3] = m_numChildren;
  
  uint32_t curLoc = 4;
  for (int i = 0; i < m_numChildren; i++)
    {
      uint32_t curSize = m_children[i].GetSerializedSize ();
      uint8_t curBuf[curSize];

      m_children[i].CopyAllTo (curBuf, curSize);

      memcpy(&m_serializedChildren[curLoc], curBuf, curSize);
      curLoc += curSize;
    }
}

} // Namespace ns3
