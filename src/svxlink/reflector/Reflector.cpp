/**
@file	 Reflector.cpp
@brief   A_brief_description_for_this_file
@author  Tobias Blomberg / SM0SVX
@date	 2017-02-11

A_detailed_description_for_this_file

\verbatim
SvxReflector - An audio reflector for connecting SvxLink Servers
Copyright (C) 2003-2017 Tobias Blomberg / SM0SVX

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\endverbatim
*/



/****************************************************************************
 *
 * System Includes
 *
 ****************************************************************************/

#include <cassert>


/****************************************************************************
 *
 * Project Includes
 *
 ****************************************************************************/

#include <AsyncConfig.h>
#include <AsyncTcpServer.h>
#include <AsyncUdpSocket.h>
#include <AsyncApplication.h>


/****************************************************************************
 *
 * Local Includes
 *
 ****************************************************************************/

#include "Reflector.h"
#include "ReflectorClient.h"



/****************************************************************************
 *
 * Namespaces to use
 *
 ****************************************************************************/

using namespace std;
using namespace Async;



/****************************************************************************
 *
 * Defines & typedefs
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Local class definitions
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Prototypes
 *
 ****************************************************************************/

namespace {
void delete_client(ReflectorClient *client);
};


/****************************************************************************
 *
 * Exported Global Variables
 *
 ****************************************************************************/




/****************************************************************************
 *
 * Local Global Variables
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Public member functions
 *
 ****************************************************************************/

Reflector::Reflector(void)
  : srv(0), udp_sock(0), m_talker(0),
    m_talker_timeout_timer(1000, Timer::TYPE_PERIODIC),
    m_sql_timeout(0), m_sql_timeout_cnt(0), m_sql_timeout_blocktime(60)
{
  timerclear(&m_last_talker_timestamp);
  m_talker_timeout_timer.expired.connect(
      mem_fun(*this, &Reflector::checkTalkerTimeout));
} /* Reflector::Reflector */


Reflector::~Reflector(void)
{
  delete udp_sock;
  delete srv;

  for (ReflectorClientMap::iterator it = client_map.begin();
       it != client_map.end(); ++it)
  {
    delete (*it).second;
  }
} /* Reflector::~Reflector */


bool Reflector::initialize(Async::Config &cfg)
{
    // Initialize the GCrypt library if not already initialized
  if (!gcry_control(GCRYCTL_INITIALIZATION_FINISHED_P))
  {
    gcry_check_version(NULL);
    gcry_error_t err;
    err = gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
    if (err != GPG_ERR_NO_ERROR)
    {
      cerr << "*** ERROR: Failed to initialize the Libgcrypt library: "
           << gcry_strsource(err) << "/" << gcry_strerror(err) << endl;
      return false;
    }
      // Tell Libgcrypt that initialization has completed
    err = gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    if (err != GPG_ERR_NO_ERROR)
    {
      cerr << "*** ERROR: Failed to initialize the Libgcrypt library: "
           << gcry_strsource(err) << "/" << gcry_strerror(err) << endl;
      return false;
    }
  }

  std::string listen_port("5300");
  cfg.getValue("GLOBAL", "LISTEN_PORT", listen_port);
  srv = new TcpServer(listen_port);
  srv->clientConnected.connect(
      mem_fun(*this, &Reflector::clientConnected));
  srv->clientDisconnected.connect(
      mem_fun(*this, &Reflector::clientDisconnected));

  uint16_t udp_listen_port = 5300;
  cfg.getValue("GLOBAL", "LISTEN_PORT", udp_listen_port);
  udp_sock = new UdpSocket(udp_listen_port);
  if ((udp_sock == 0) || !udp_sock->initOk())
  {
    cerr << "*** ERROR: Could not initialize UDP socket" << endl;
    return false;
  }
  udp_sock->dataReceived.connect(
      mem_fun(*this, &Reflector::udpDatagramReceived));

  if (!cfg.getValue("GLOBAL", "AUTH_KEY", m_auth_key) || m_auth_key.empty())
  {
    cerr << "*** ERROR: GLOBAL/AUTH_KEY must be specified\n";
    return false;
  }
  if (m_auth_key == "Change this key now!")
  {
    cerr << "*** ERROR: You must change GLOBAL/AUTH_KEY from the "
            "default value" << endl;
    return false;
  }

  cfg.getValue("GLOBAL", "SQL_TIMEOUT", m_sql_timeout);
  cfg.getValue("GLOBAL", "SQL_TIMEOUT_BLOCKTIME", m_sql_timeout_blocktime);
  m_sql_timeout_blocktime = max(m_sql_timeout_blocktime, 1U);

  return true;
} /* Reflector::initialize */


void Reflector::nodeList(std::vector<std::string>& nodes) const
{
  for (ReflectorClientMap::const_iterator it = client_map.begin();
       it != client_map.end(); ++it)
  {
    const std::string& callsign = (*it).second->callsign();
    if (!callsign.empty())
    {
      nodes.push_back(callsign);
    }
  }
} /* Reflector::nodeList */


void Reflector::broadcastMsgExcept(const ReflectorMsg& msg,
                                   ReflectorClient *client)
{
  ReflectorClientMap::const_iterator it = client_map.begin();
  for (; it != client_map.end(); ++it)
  {
    if ((*it).second != client)
    {
      //cout << "### Reflector::broadcastMsgExcept: "
      //     << (*it).second->callsign() << endl;
      (*it).second->sendMsg(msg);
    }
  }
} /* Reflector::broadcastMsgExcept */


void Reflector::sendUdpDatagram(ReflectorClient *client, const void *buf,
                                size_t count)
{
  udp_sock->write(client->remoteHost(), client->remoteUdpPort(), buf, count);
} /* Reflector::sendUdpDatagram */


/****************************************************************************
 *
 * Protected member functions
 *
 ****************************************************************************/



/****************************************************************************
 *
 * Private member functions
 *
 ****************************************************************************/

void Reflector::clientConnected(Async::TcpConnection *con)
{
  cout << "Client " << con->remoteHost() << ":" << con->remotePort()
       << " connected" << endl;
  ReflectorClient *rc = new ReflectorClient(this, con, m_auth_key);
  client_map[rc->clientId()] = rc;
  m_client_con_map[con] = rc;
} /* Reflector::clientConnected */


void Reflector::clientDisconnected(Async::TcpConnection *con,
                                   Async::TcpConnection::DisconnectReason reason)
{
  ReflectorClientConMap::iterator it = m_client_con_map.find(con);
  assert(it != m_client_con_map.end());
  ReflectorClient *client = (*it).second;

  if (!client->callsign().empty())
  {
    cout << client->callsign() << ": ";
  }
  cout << "Client " << con->remoteHost() << ":" << con->remotePort()
       << " disconnected: " << TcpConnection::disconnectReasonStr(reason)
       << endl;

  client_map.erase(client->clientId());
  m_client_con_map.erase(it);
  if (!client->callsign().empty())
  {
    broadcastMsgExcept(MsgNodeLeft(client->callsign()), client);
  }
  Application::app().runTask(sigc::bind(sigc::ptr_fun(&delete_client), client));
} /* Reflector::clientDisconnected */


void Reflector::udpDatagramReceived(const IpAddress& addr, uint16_t port,
                                    void *buf, int count)
{
  //cout << "### Reflector::udpDatagramReceived: addr=" << addr
  //     << " port=" << port << " count=" << count;

  stringstream ss;
  ss.write(reinterpret_cast<const char *>(buf), count);

  ReflectorUdpMsg header;
  if (!header.unpack(ss))
  {
    // FIXME: Disconnect
    cout << "*** ERROR: Unpacking failed for UDP message header\n";
    return;
  }

  //cout << " msg_type=" << header.type()
  //     << " client_id=" << header.clientId()
  //     << " seq=" << header.sequenceNum()
  //     << std::endl;

  ReflectorClientMap::iterator it = client_map.find(header.clientId());
  if (it == client_map.end())
  {
    cerr << "*** WARNING: Incoming UDP packet has invalid client id" << endl;
    return;
  }
  ReflectorClient *client = (*it).second;
  if (addr != client->remoteHost())
  {
    cerr << "*** WARNING[" << client->callsign()
         << "]: Incoming UDP packet has the wrong source ip" << endl;
    return;
  }
  if (client->remoteUdpPort() == 0)
  {
    client->setRemoteUdpPort(port);
    client->sendUdpMsg(MsgUdpHeartbeat());
  }
  else if (port != client->remoteUdpPort())
  {
    cerr << "*** WARNING[" << client->callsign()
         << "]: Incoming UDP packet has the wrong source UDP "
            "port number" << endl;
    return;
  }

    // Check sequence number
  uint16_t udp_rx_seq_diff = header.sequenceNum() - client->nextUdpRxSeq();
  if (udp_rx_seq_diff > 0x7fff) // Frame out of sequence (ignore)
  {
    cout << "### " << client->callsign()
         << ": Dropping out of sequence frame with seq="
         << header.sequenceNum() << ". Expected seq="
         << client->nextUdpRxSeq() << endl;
    return;
  }
  else if (udp_rx_seq_diff > 0) // Frame(s) lost
  {
    cout << "### " << client->callsign()
         << ": UDP frame(s) lost. Expected seq=" << client->nextUdpRxSeq()
         << ". Received seq=" << header.sequenceNum() << endl;
  }
  client->setNextUdpRxSeq(header.sequenceNum() + 1);

  client->udpMsgReceived(header);

  switch (header.type())
  {
    case MsgUdpHeartbeat::TYPE:
      //cout << "### " << client->callsign() << ": MsgUdpHeartbeat()" << endl;
      break;

    case MsgUdpAudio::TYPE:
    {
      if (!client->isBlocked())
      {
        MsgUdpAudio msg;
        msg.unpack(ss);
        if (!msg.audioData().empty())
        {
          if (m_talker == 0)
          {
            setTalker(client);
            cout << "### " << m_talker->callsign() << ": Talker start" << endl;
          }
          if (m_talker == client)
          {
            gettimeofday(&m_last_talker_timestamp, NULL);
            broadcastUdpMsgExcept(client, msg);
          }
          else
          {
            cout << "### " << client->callsign() << ": " << m_talker->callsign()
                 << " is already talking...\n";
          }
        }
      }
      break;
    }

    case MsgUdpFlushSamples::TYPE:
    {
      //cout << "### " << client->callsign() << ": MsgUdpFlushSamples()" << endl;
      if (client == m_talker)
      {
        cout << "### " << m_talker->callsign() << ": Talker stop" << endl;
        setTalker(0);
      }
        // To be 100% correct the reflector should wait for all connected
        // clients to send a MsgUdpAllSamplesFlushed message but that will
        // probably lead to problems, especially on reflectors with many
        // clients. We therefore acknowledge the flush immediately here to
        // the client who sent the flush request.
      client->sendUdpMsg(MsgUdpAllSamplesFlushed());
      break;
    }

    case MsgUdpAllSamplesFlushed::TYPE:
      //cout << "### " << client->callsign() << ": MsgUdpAllSamplesFlushed()"
      //     << endl;
      // Ignore
      break;

    default:
      cerr << "*** WARNING[" << client->callsign()
           << "]: Unknown UDP protocol message received: msg_type="
           << header.type() << endl;
      // FIXME: Disconnect client or ignore?
      break;
  }
} /* Reflector::udpDatagramReceived */


#if 0
void Reflector::sendUdpMsg(ReflectorClient *client,
                           const ReflectorUdpMsg &msg)
{
  if (client->remoteUdpPort() == 0)
  {
    return;
  }

  //cout << "### Reflector::sendUdpMsg: " << client->remoteHost() << ":"
  //     << client->remoteUdpPort() << endl;

  ReflectorUdpMsg header(msg.type(), client->clientId(), client->nextUdpTxSeq());
  ostringstream ss;
  if (!header.pack(ss) || !msg.pack(ss))
  {
    // FIXME: Better error handling
    cerr << "*** ERROR: Failed to pack reflector UDP message\n";
    return;
  }
  udp_sock->write(client->remoteHost(), client->remoteUdpPort(),
                  ss.str().data(), ss.str().size());
} /* ReflectorLogic::sendUdpMsg */
#endif


void Reflector::broadcastUdpMsgExcept(const ReflectorClient *client,
                                      const ReflectorUdpMsg& msg)
{
  for (ReflectorClientMap::iterator it = client_map.begin();
       it != client_map.end(); ++it)
  {
    if ((*it).second != client)
    {
      (*it).second->sendUdpMsg(msg);
    }
  }
} /* Reflector::broadcastUdpMsgExcept */


void Reflector::checkTalkerTimeout(Async::Timer *t)
{
  //cout << "### Reflector::checkTalkerTimeout\n";

  if (m_talker != 0)
  {
    struct timeval now, diff;
    gettimeofday(&now, NULL);
    timersub(&now, &m_last_talker_timestamp, &diff);
    if (diff.tv_sec > 3)
    {
      cout << "### " << m_talker->callsign() << ": Talker audio timeout"
           << endl;
      setTalker(0);
    }

    if ((m_sql_timeout_cnt > 0) && (--m_sql_timeout_cnt == 0))
    {
      cout << "### " << m_talker->callsign() << ": Talker squelch timeout"
           << endl;
      m_talker->setBlock(m_sql_timeout_blocktime);
      setTalker(0);
    }
  }
} /* Reflector::checkTalkerTimeout */


void Reflector::setTalker(ReflectorClient *client)
{
  if (client == m_talker)
  {
    return;
  }

  if (client == 0)
  {
    broadcastMsgExcept(MsgTalkerStop(m_talker->callsign()));
    broadcastUdpMsgExcept(client, MsgUdpFlushSamples());
    m_sql_timeout_cnt = 0;
    m_talker = 0;
  }
  else
  {
    assert(m_talker == 0);
    m_sql_timeout_cnt = m_sql_timeout;
    m_talker = client;
    broadcastMsgExcept(MsgTalkerStart(m_talker->callsign()));
  }
} /* Reflector::setTalker */


namespace {
void delete_client(ReflectorClient *client) { delete client; }
};


/*
 * This file has not been truncated
 */

