/*
 *      Copyright (C) 2010 Alwin Esch (Team XBMC)
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "VNSISession.h"
#include "client.h"
#ifdef _MSC_VER
#include <winsock2.h>
#define SHUT_RDWR SD_BOTH
#define ETIMEDOUT WSAETIMEDOUT
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "responsepacket.h"
#include "requestpacket.h"
#include "vdrcommand.h"
#include "tools.h"

/* Needed on Mac OS/X */
 
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
using namespace std;

cVNSISession::cVNSISession()
  : m_fd(INVALID_SOCKET)
  , m_protocol(0)
{
}

cVNSISession::~cVNSISession()
{
  Close();
}

void cVNSISession::Abort()
{
  shutdown(m_fd, SHUT_RDWR);
}

void cVNSISession::Close()
{
  if(m_fd != INVALID_SOCKET)
  {
    closesocket(m_fd);
    m_fd = INVALID_SOCKET;
  }
}

bool cVNSISession::Open(const CStdString& hostname, int port, const char *name)
{
  struct hostent hostbuf, *hp;
  int herr, fd, r, res, err;
  struct sockaddr_in6 in6;
  struct sockaddr_in in;
  socklen_t errlen  = sizeof(int);
  size_t hstbuflen  = 1024;
  char *tmphstbuf   = (char*)malloc(hstbuflen);

  if (port == 0)
    port = 34890;

  while((res = gethostbyname_r(hostname.c_str(), &hostbuf, tmphstbuf, hstbuflen, &hp, &herr)) == ERANGE)
  {
    hstbuflen *= 2;
    tmphstbuf = (char*)realloc(tmphstbuf, hstbuflen);
  }

  if(res != 0)
  {
    XBMC->Log(LOG_ERROR, "cVNSISession::Open - Resolver internal error");
    free(tmphstbuf);
    return false;
  }
  else if(herr != 0)
  {
    switch(herr)
    {
      case HOST_NOT_FOUND:
        XBMC->Log(LOG_ERROR, "cVNSISession::Open - The specified host is unknown");
        break;
      case NO_ADDRESS:
        XBMC->Log(LOG_ERROR, "cVNSISession::Open - The requested name is valid but does not have an IP address");
        break;
      case NO_RECOVERY:
        XBMC->Log(LOG_ERROR, "cVNSISession::Open - A non-recoverable name server error occurred");
        break;
      case TRY_AGAIN:
        XBMC->Log(LOG_ERROR, "cVNSISession::Open - A temporary error occurred on an authoritative name server");
        break;
      default:
        XBMC->Log(LOG_ERROR, "cVNSISession::Open - Unknown error");
        break;
    }

    free(tmphstbuf);
    return false;
  }
  else if(hp == NULL)
  {
    XBMC->Log(LOG_ERROR, "cVNSISession::Open - Resolver internal error");
    free(tmphstbuf);
    return false;
  }

  fd = socket(hp->h_addrtype, SOCK_STREAM, 0);
  if (fd == -1)
  {
    XBMC->Log(LOG_ERROR, "cVNSISession::Open - Unable to create socket: %s", strerror(errno));
    free(tmphstbuf);
    return false;
  }

  /**
   * Switch to nonblocking
   */
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

  switch(hp->h_addrtype)
  {
    case AF_INET:
      memset(&in, 0, sizeof(in));
      in.sin_family = AF_INET;
      in.sin_port = htons(port);
      memcpy(&in.sin_addr, hp->h_addr_list[0], sizeof(struct in_addr));
      r = connect(fd, (struct sockaddr *)&in, sizeof(struct sockaddr_in));
      break;

    case AF_INET6:
      memset(&in6, 0, sizeof(in6));
      in6.sin6_family = AF_INET6;
      in6.sin6_port = htons(port);
      memcpy(&in6.sin6_addr, hp->h_addr_list[0], sizeof(struct in6_addr));
      r = connect(fd, (struct sockaddr *)&in, sizeof(struct sockaddr_in6));
      break;

    default:
      XBMC->Log(LOG_ERROR, "cVNSISession::Open - Invalid protocol family");
      free(tmphstbuf);
      return false;
  }

  free(tmphstbuf);

  if (r == -1)
  {
    if (errno == EINPROGRESS)
    {
      struct pollfd pfd;

      pfd.fd = fd;
      pfd.events = POLLOUT;
      pfd.revents = 0;

      r = poll(&pfd, 1, g_iConnectTimeout*1000);
      if (r == 0) /* Timeout */
        XBMC->Log(LOG_ERROR, "Connection attempt timed out %i", g_iConnectTimeout);

      if (r == -1)
      {
        XBMC->Log(LOG_ERROR, "poll() error: %s", strerror(errno));
        return false;
      }

      getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen);
    }
    else
    {
      err = errno;
    }
  }
  else
  {
    err = 0;
  }

  if (err != 0)
  {
    XBMC->Log(LOG_ERROR, "%s", strerror(err));
    return false;
  }

  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

  int val = 1;
  setsockopt(fd, SOL_TCP, TCP_NODELAY, &val, sizeof(val));

  try
  {
    m_fd = fd;
    if (m_fd == INVALID_SOCKET)
      throw "Can't connect to VSNI Server";

    cRequestPacket vrp;
    if (!vrp.init(VDR_LOGIN))                 throw "Can't init cRequestPacket";
    if (!vrp.add_U32(VNSIProtocolVersion))    throw "Can't add protocol version to RequestPacket";
    if (!vrp.add_U8(false))                   throw "Can't add netlog flag";
    if (name && strlen(name) > 0)
    {
      if (!vrp.add_String(name))                throw "Can't add client name to RequestPacket";
    }
    else
    {
      if (!vrp.add_String("XBMC Media Center")) throw "Can't add client name to RequestPacket";
    }

    // read welcome
    cResponsePacket* vresp = ReadResult(&vrp);
    if (!vresp)
      throw "failed to read greeting from server";

    uint32_t    protocol      = vresp->extract_U32();
    uint32_t    vdrTime       = vresp->extract_U32();
    int32_t     vdrTimeOffset = vresp->extract_S32();
    const char *ServerName    = vresp->extract_String();
    const char *ServerVersion = vresp->extract_String();

    m_server    = ServerName;
    m_version   = ServerVersion;
    m_protocol  = protocol;

    if (!name || strlen(name) <= 0)
      XBMC->Log(LOG_NOTICE, "Logged in at '%lu+%lu' to '%s' Version: '%s' with protocol version '%lu'", vdrTime, vdrTimeOffset, ServerName, ServerVersion, protocol);

    delete vresp;
  }
  catch (const char * str)
  {
    XBMC->Log(LOG_ERROR, "cVNSISession::Open - %s", str);
    close(m_fd);
    m_fd = INVALID_SOCKET;
    return false;
  }

  return true;
}

cResponsePacket* cVNSISession::ReadMessage()
{
  uint32_t channelID = 0;
  uint32_t requestID;
  uint32_t userDataLength = 0;
  uint8_t* userData = NULL;
  uint32_t streamID;
  uint32_t duration;
  uint32_t opCodeID;
  int64_t  dts = 0;
  int64_t  pts = 0;

  cResponsePacket* vresp = NULL;

  bool readSuccess = readData((uint8_t*)&channelID, sizeof(uint32_t)) > 0;
  if (!readSuccess)
    return NULL;

  // Data was read

  channelID = ntohl(channelID);
  if (channelID == CHANNEL_REQUEST_RESPONSE)
  {
    if (!readData((uint8_t*)&m_responsePacketHeader, sizeof(m_responsePacketHeader))) return NULL;

    requestID = ntohl(m_responsePacketHeader.requestID);
    userDataLength = ntohl(m_responsePacketHeader.userDataLength);

    if (userDataLength > 5000000) return NULL; // how big can these packets get?
    userData = NULL;
    if (userDataLength > 0)
    {
      userData = (uint8_t*)malloc(userDataLength);
      if (!userData) return NULL;
      if (!readData(userData, userDataLength)) {
        free(userData);
        return NULL;
      }
    }

    vresp = new cResponsePacket();
    vresp->setResponse(requestID, userData, userDataLength);
    DEVDBG("cVNSISession::ReadMessage() - Rxd a response packet, requestID=%lu, len=%lu", requestID, userDataLength);
  }
  else if (channelID == CHANNEL_STREAM)
  {
    if (!readData((uint8_t*)&m_streamPacketHeader, sizeof(m_streamPacketHeader))) return NULL;

    opCodeID = ntohl(m_streamPacketHeader.opCodeID);
    streamID = ntohl(m_streamPacketHeader.streamID);
    duration = ntohl(m_streamPacketHeader.duration);
    pts = ntohll(*(int64_t*)m_streamPacketHeader.pts);
    dts = ntohll(*(int64_t*)m_streamPacketHeader.dts);
    userDataLength = ntohl(m_streamPacketHeader.userDataLength);

    if(opCodeID == VDR_STREAM_MUXPKT) {
      DemuxPacket* p = PVR->AllocateDemuxPacket(userDataLength);
      userData = (uint8_t*)p;
      if (userDataLength > 0)
      {
        if (!userData) return NULL;
        if (!readData(p->pData, userDataLength))
        {
          PVR->FreeDemuxPacket(p);
          return NULL;
        }
      }
    }
    else if (userDataLength > 0) {
      userData = (uint8_t*)malloc(userDataLength);
      if (!userData) return NULL;
      if (!readData(userData, userDataLength))
      {
        free(userData);
        return NULL;
      }
    }

    vresp = new cResponsePacket();
    vresp->setStream(opCodeID, streamID, duration, dts, pts, userData, userDataLength);
  }
  else
  {
    XBMC->Log(LOG_ERROR, "cVNSISession::ReadMessage() - Rxd a response packet on channel %lu !!", channelID);
  }

  return vresp;
}

bool cVNSISession::SendMessage(cRequestPacket* vrp)
{
  if (sendData(vrp->getPtr(), vrp->getLen()) != vrp->getLen())
    return false;

  return true;
}

cResponsePacket* cVNSISession::ReadResult(cRequestPacket* vrp, bool sequence)
{
  if(!SendMessage(vrp))
    return NULL;

  cResponsePacket *pkt = NULL;

  while((pkt = ReadMessage()))
  {
    /* Discard everything other as response packets until it is received */
    if (pkt->getChannelID() == CHANNEL_REQUEST_RESPONSE
        && (!sequence || pkt->getRequestID() == vrp->getSerial()))
    {
      return pkt;
    }
    else
      delete pkt;
  }
  return NULL;
}

bool cVNSISession::ReadSuccess(cRequestPacket* vrp, bool sequence)
{
  cResponsePacket *pkt = NULL;
  if((pkt = ReadResult(vrp, sequence)) == NULL)
  {
    DEVDBG("cVNSISession::ReadSuccess - failed");
    return false;
  }
  uint32_t retCode = pkt->extract_U32();
  delete pkt;

  if(retCode != VDR_RET_OK)
  {
    XBMC->Log(LOG_ERROR, "cVNSISession::ReadSuccess - failed with error code '%i'", retCode);
    return false;
  }
  return true;
}

int cVNSISession::sendData(void* bufR, size_t count)
{
  size_t bytes_sent = 0;
  int this_write;
  int temp_write;

  unsigned char* buf = (unsigned char*)bufR;

  while (bytes_sent < count)
  {
#ifndef WIN32
    do
    {
      temp_write = this_write = write(m_fd, buf, count - bytes_sent);
    } while ( (this_write < 0) && (errno == EINTR) );
#else
    do
    {
      temp_write = this_write = send(m_fd,(char*) buf, count- bytes_sent,0);
    } while ( (this_write == SOCKET_ERROR) && (WSAGetLastError() == WSAEINTR) );
#endif
    if (this_write <= 0)
    {
      return(this_write);
    }
    bytes_sent += this_write;
    buf += this_write;
  }

  return(count);
}

int cVNSISession::readData(uint8_t* buffer, int totalBytes)
{
  int bytesRead = 0;
  int thisRead;
  int success;
  fd_set readSet;
  struct timeval timeout;

  while(1)
  {
    FD_ZERO(&readSet);
    FD_SET(m_fd, &readSet);
    timeout.tv_sec = g_iConnectTimeout;
    timeout.tv_usec = 0;
    success = select(m_fd + 1, &readSet, NULL, NULL, &timeout);
    if (success < 1)
    {
      return 0;  // error, or timeout
    }
#ifndef WIN32
    thisRead = read(m_fd, &buffer[bytesRead], totalBytes - bytesRead);
#else
    thisRead = recv(m_fd, (char*)&buffer[bytesRead], totalBytes - bytesRead, 0);
#endif
    if (!thisRead)
    {
      // if read returns 0 then connection is closed
      // in non-blocking mode if read is called with no data available, it returns -1
      // and sets errno to EGAGAIN. but we use select so it wouldn't do that anyway.
      XBMC->Log(LOG_ERROR, "cVNSISession::readData - Detected connection closed");
      SetClientConnected(false);
      return -1;
    }
    bytesRead += thisRead;
    if (bytesRead == totalBytes)
    {
      return 1;
    }
  }
}
