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
 #include <iostream>
 #include<fstream>
 #include<map>
 #include<list>

 #include<string.h>

 #include </usr/include/python3.10/Python.h>

//#include</home/luca/ns-3-dev/libtorch/include/torch/script.h>


#include "ns3/random_noise_client.h"


#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/random-variable-stream.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RandomNoiseClientApplication");

NS_OBJECT_ENSURE_REGISTERED(RandomNoiseClient);

TypeId
RandomNoiseClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RandomNoiseClient")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<RandomNoiseClient>()
            .AddAttribute(
                "MaxPackets",
                "The maximum number of packets the application will send (zero means infinite)",
                UintegerValue(0),
                MakeUintegerAccessor(&RandomNoiseClient::m_count),
                MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval",
                          "The time to wait between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&RandomNoiseClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&RandomNoiseClient::m_peerAddress),
                          MakeAddressChecker())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RandomNoiseClient::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Tos",
                            "The Type of Service used to send IPv4 packets. "
                            "All 8 bits of the TOS byte are set (including ECN bits).",
                            UintegerValue(0),
                            MakeUintegerAccessor(&RandomNoiseClient::m_tos),
                            MakeUintegerChecker<uint8_t>())
            .AddAttribute("PacketSize",
                            "Size of echo data in outbound packets",
                            UintegerValue(100),
                            MakeUintegerAccessor(&RandomNoiseClient::SetDataSize, &RandomNoiseClient::GetDataSize),
                            MakeUintegerChecker<uint32_t>())
            .AddAttribute("PacketSizeMean",
                            "Mean packet size for the normal distribution.",
                            DoubleValue(1000),  // Default mean value
                            MakeDoubleAccessor(&RandomNoiseClient::m_packetSizeMean),
                            MakeDoubleChecker<double>())
            .AddAttribute("PacketSizeVariance",
                            "Variance of packet size for the normal distribution.",
                            DoubleValue(200),  // Default variance
                            MakeDoubleAccessor(&RandomNoiseClient::m_packetSizeVariance),
                            MakeDoubleChecker<double>())
            .AddAttribute("IntervalMean",
                            "Mean interval for the exponential distribution.",
                            DoubleValue(1.0),  // Default mean value
                            MakeDoubleAccessor(&RandomNoiseClient::m_intervalMean),
                            MakeDoubleChecker<double>())
            .AddTraceSource("Tx",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&RandomNoiseClient::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("Rx",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&RandomNoiseClient::m_rxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("TxWithAddresses",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&RandomNoiseClient::m_txTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("RxWithAddresses",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&RandomNoiseClient::m_rxTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback");
    return tid;
}

RandomNoiseClient::RandomNoiseClient()
{
    NS_LOG_FUNCTION(this);
    m_sent = 0;
    m_socket = nullptr;
    m_sendEvent = EventId();
    m_data = nullptr;
    m_dataSize = 0;

    m_normalRand = CreateObject<NormalRandomVariable>();
    m_exponentialRand = CreateObject<ExponentialRandomVariable>();
}

RandomNoiseClient::~RandomNoiseClient()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;

    delete[] m_data;
    m_data = nullptr;
    m_dataSize = 0;
}

void
RandomNoiseClient::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddress = ip;
    m_peerPort = port;
}

void
RandomNoiseClient::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddress = addr;
}

void
RandomNoiseClient::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

void
RandomNoiseClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    m_normalRand->SetAttribute("Mean", DoubleValue(m_packetSizeMean));
    m_normalRand->SetAttribute("Variance", DoubleValue(m_packetSizeVariance));
    m_exponentialRand->SetAttribute("Mean", DoubleValue(m_intervalMean));

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        if (Ipv4Address::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
            m_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (Ipv6Address::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
            m_socket->Connect(m_peerAddress);
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
        }
    }

    m_socket->SetRecvCallback(MakeCallback(&RandomNoiseClient::HandleRead, this));
    m_socket->SetAllowBroadcast(true);
    ScheduleTransmit(Seconds(0.0));
}

void
RandomNoiseClient::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket = nullptr;
    }

    Simulator::Cancel(m_sendEvent);
}

void
RandomNoiseClient::SetDataSize(uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << dataSize);

    //
    // If the client is setting the echo packet data size this way, we infer
    // that she doesn't care about the contents of the packet at all, so
    // neither will we.
    //
    delete[] m_data;
    m_data = nullptr;
    m_dataSize = 0;
    m_size = dataSize;
}

uint32_t
RandomNoiseClient::GetDataSize() const
{
    NS_LOG_FUNCTION(this);
    return m_size;
}

void
RandomNoiseClient::SetFill(std::string fill)
{
    NS_LOG_FUNCTION(this << fill);

    uint32_t dataSize = fill.size() + 1;

    if (dataSize != m_dataSize)
    {
        delete[] m_data;
        m_data = new uint8_t[dataSize];
        m_dataSize = dataSize;
    }

    memcpy(m_data, fill.c_str(), dataSize);

    //
    // Overwrite packet size attribute.
    //
    m_size = dataSize;
}

void
RandomNoiseClient::SetFill(uint8_t fill, uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << fill << dataSize);
    if (dataSize != m_dataSize)
    {
        delete[] m_data;
        m_data = new uint8_t[dataSize];
        m_dataSize = dataSize;
    }

    memset(m_data, fill, dataSize);

    //
    // Overwrite packet size attribute.
    //
    m_size = dataSize;
}

void
RandomNoiseClient::SetFill(uint8_t* fill, uint32_t fillSize, uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << fill << fillSize << dataSize);
    if (dataSize != m_dataSize)
    {
        delete[] m_data;
        m_data = new uint8_t[dataSize];
        m_dataSize = dataSize;
    }

    if (fillSize >= dataSize)
    {
        memcpy(m_data, fill, dataSize);
        m_size = dataSize;
        return;
    }

    //
    // Do all but the final fill.
    //
    uint32_t filled = 0;
    while (filled + fillSize < dataSize)
    {
        memcpy(&m_data[filled], fill, fillSize);
        filled += fillSize;
    }

    //
    // Last fill may be partial
    //
    memcpy(&m_data[filled], fill, dataSize - filled);

    //
    // Overwrite packet size attribute.
    //
    m_size = dataSize;
}

void
RandomNoiseClient::ScheduleTransmit(Time dt)
{
    NS_LOG_FUNCTION(this << dt);
    m_sendEvent = Simulator::Schedule(dt, &RandomNoiseClient::Send, this);
}

void
RandomNoiseClient::Send()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT(m_sendEvent.IsExpired());

    uint32_t packetSize = std::abs(static_cast<int>(m_normalRand->GetValue()));
    std::cout << packetSize << "bytes"<< std::endl;
    Ptr<Packet> p = Create<Packet>(packetSize);


    Address localAddress;
    m_socket->GetSockName(localAddress);
    // call to the trace sinks before the packet is actually sent,
    // so that tags added to the packet can be sent as well
    m_txTrace(p);
    if (Ipv4Address::IsMatchingType(m_peerAddress))
    {
        m_txTraceWithAddresses(
            p,
            localAddress,
            InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
    }
    else if (Ipv6Address::IsMatchingType(m_peerAddress))
    {
        m_txTraceWithAddresses(
            p,
            localAddress,
            Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
    }
    m_socket->Send(p);
    double send_time = Now().GetSeconds();
    messageTimings[p->GetUid()] = send_time;
    std::cout << "send msg " << p->GetUid() << " at " << send_time << std::endl;

    ++m_sent;

    if (Ipv4Address::IsMatchingType(m_peerAddress))
    {
        NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " client sent " << m_size
                               << " bytes to " << Ipv4Address::ConvertFrom(m_peerAddress)
                               << " port " << m_peerPort);
    }
    else if (Ipv6Address::IsMatchingType(m_peerAddress))
    {
        NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " client sent " << m_size
                               << " bytes to " << Ipv6Address::ConvertFrom(m_peerAddress)
                               << " port " << m_peerPort);
    }
    else if (InetSocketAddress::IsMatchingType(m_peerAddress))
    {
        NS_LOG_INFO(
            "At time " << Simulator::Now().As(Time::S) << " client sent " << m_size << " bytes to "
                       << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4() << " port "
                       << InetSocketAddress::ConvertFrom(m_peerAddress).GetPort());
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
    {
        NS_LOG_INFO(
            "At time " << Simulator::Now().As(Time::S) << " client sent " << m_size << " bytes to "
                       << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetIpv6() << " port "
                       << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetPort());
    }

    if (m_intervalMean == 0){
        act_as_noise_client = false;
    }
    if (m_sent < m_count || m_count == 0)
    {
      if (act_as_noise_client == true){
        float randInterval =  m_exponentialRand->GetValue();
        Time nextInterval(Seconds(randInterval));
        ScheduleTransmit(nextInterval);
      }else{
        //double delay_untill_next_package_send = optimal_speed * predicted_bandwith_ratio + slowest_speed * (1-predicted_bandwith_ratio);
        if (predicted_bandwith_ratio < 0){predicted_bandwith_ratio = 0;}
        if (predicted_bandwith_ratio > 1){predicted_bandwith_ratio = 1;}
        double inverted_ratio = 1 - predicted_bandwith_ratio;
        double delay_untill_next_package_send = inverted_ratio * inverted_ratio * inverted_ratio * inverted_ratio * inverted_ratio * inverted_ratio;
        Time nextInterval(Seconds(delay_untill_next_package_send));
        ScheduleTransmit(nextInterval);
        std::cout << " * Current bandwith ratio: " << predicted_bandwith_ratio << std::endl;
        std::cout << " * Delay untill next send: " << delay_untill_next_package_send << " s" << std::endl;
      }



    }
}

void
RandomNoiseClient::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " client received "
                                   << packet->GetSize() << " bytes from "
                                   << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(from).GetPort());
        }
        else if (Inet6SocketAddress::IsMatchingType(from))
        {
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " client received "
                                   << packet->GetSize() << " bytes from "
                                   << Inet6SocketAddress::ConvertFrom(from).GetIpv6() << " port "
                                   << Inet6SocketAddress::ConvertFrom(from).GetPort());
        }
        socket->GetSockName(localAddress);
        m_rxTrace(packet);
        m_rxTraceWithAddresses(packet, from, localAddress);


        if (act_as_noise_client == false){
          // Calculate statistics
          uint32_t uid_received = packet->GetUid();
          double send_time = messageTimings[uid_received];
          double receive_time = Now().GetSeconds();
          double delay = receive_time - send_time;
          messageTimings.erase(uid_received);

          latencies.push_back(delay);
          if (latencies.size() > view_size){
            latencies.pop_front();
          }



          double mean = 0;
          for (std::list<double>::iterator it=latencies.begin(); it != latencies.end(); ++it){
            mean += *it;
          }
          mean = mean / latencies.size();

          mean_latencies.push_back(mean);
          if (mean_latencies.size() > view_size){
            mean_latencies.pop_front();
          }

          double standard_deviation = 0;
          for (std::list<double>::iterator it=latencies.begin(); it != latencies.end(); ++it){
            standard_deviation += (*it - mean) * (*it - mean);
          }
          standard_deviation = sqrt(standard_deviation/latencies.size());

          stdev_latency.push_back(standard_deviation);
          //std::cout << standard_deviation << :: std::endl;
          if (stdev_latency.size() > view_size){
            stdev_latency.pop_front();
          }

          current_mean_latency = mean;
          current_stdev_latency = standard_deviation;
          current_latency = delay;
          current_latency_smoothed = mean;
          current_first_order_deriv = 0;
          current_second_order_deriv = 0;
          current_packet_loss = 0;

          //std::cout << "received msg " << uid_received << std::endl;
          //std::cout << "  * mean_latency: " << current_mean_latency << "\n  * stdev_latency: " << current_stdev_latency << "\n  * latency: " << current_latency << std::endl;

          uint32_t testint = 13;
          char command[1000];
          std::string a = std::to_string(testint);
          strcpy(command, "import os;os.system(\'python3 /home/luca/ns-3-dev/masticc/useLSTM.py " );

          if (mean_latencies.size() == view_size){
            strcat(command, std::to_string(view_size).c_str());
            strcat(command, " ");
            for(int i = 0; i < view_size; i++){
                int counter = 0;
                for (std::list<double>::iterator it=mean_latencies.begin(); it != mean_latencies.end(); ++it){
                  if (counter == i){
                    double value = *it;
                    strcat(command, std::to_string(value).c_str());
                    break;
                  }
                  counter+=1;
                }
                strcat(command, " ");
                counter = 0;
                for (std::list<double>::iterator it=stdev_latency.begin(); it != stdev_latency.end(); ++it){
                  if (counter == i){
                    double value = *it;
                    strcat(command, std::to_string(value).c_str());
                    break;
                  }
                  counter+=1;
                }
                strcat(command, " ");
                for (std::list<double>::iterator it=latencies.begin(); it != latencies.end(); ++it){
                  if (counter == i){
                    double value = *it;
                    strcat(command, std::to_string(value).c_str());
                    break;
                  }
                  counter+=1;
                }
                for (int j = 0; j < 4; j++){
                  strcat(command, " 0");
                }
                strcat(command, " ");
            }
          }




        //  strcat(command, a.c_str());
          strcat(command, "\')");
        //  std::cout << command << std::endl;
          //std::cout << command << std::endl;
          Py_Initialize();

          PyRun_SimpleString(command);
          Py_Finalize();

          std::ifstream inputFile("pythonResult.txt");
          if (!inputFile.is_open()){
            std::cout << "INPUT FILE WAS NOT OPENED" << std::endl;
          }else{
            std::string line;
            getline(inputFile, line);
            predicted_bandwith_ratio = std::stod(line);

          //  std::cout << " * Predicted bandwith ratio: " << predicted_bandwith_ratio << std::endl;
          }
        }
    }
}

} // Namespace ns3
// ./ns3 build -I/usr/include/python3.10
