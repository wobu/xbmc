/*
 *      Copyright (C) 2010 Alwin Esch (Team XBMC)
 *      http://xbmc.org
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

#include "client.h"
#include "xbmc_pvr_dll.h"
#include "VNSIDemux.h"
#include "VNSIRecording.h"
#include "VNSIData.h"
#include "VNSIChannelScan.h"

using namespace std;

bool m_bCreated               = false;
bool m_connected              = false;
int  m_retries                = 0;
ADDON_STATUS m_CurStatus      = STATUS_UNKNOWN;
int g_clientID                = -1;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
CStdString    g_szHostname              = DEFAULT_HOST;
int           g_iPort                   = DEFAULT_PORT;
bool          g_bCharsetConv            = DEFAULT_CHARCONV;     ///< Convert VDR's incoming strings to UTF8 character set
bool          g_bHandleMessages         = DEFAULT_HANDLE_MSG;   ///< Send VDR's OSD status messages to XBMC OSD
int           g_iConnectTimeout         = DEFAULT_TIMEOUT;      ///< The Socket connection timeout
int           g_iPriority               = DEFAULT_PRIORITY;     ///< The Priority this client have in response to other clients
CStdString    g_szUserPath              = "";
CStdString    g_szClientPath            = "";
cHelper_libXBMC_addon *XBMC   = NULL;
cHelper_libXBMC_gui   *GUI    = NULL;
cHelper_libXBMC_pvr   *PVR    = NULL;
cVNSIDemux      *VNSIDemuxer       = NULL;
cVNSIData       *VNSIData          = NULL;
cVNSIRecording  *VNSIRecording     = NULL;

bool IsClientConnected(bool forceReconnect)
{
  if (forceReconnect)
    m_retries = 0;

  if (!m_connected && m_bCreated)
  {
    while (m_retries < 5)
    {
      m_retries++;
      if (VNSIDemuxer)
      {
        delete VNSIDemuxer;
        VNSIDemuxer = NULL;
      }
      if (VNSIRecording)
      {
        delete VNSIRecording;
        VNSIRecording = NULL;
      }
      if (VNSIData)
      {
        XBMC->Log(LOG_NOTICE, "Trying to reconnect to VNSI Server (try %i)", m_retries+1);
        sleep(2);
        if (VNSIData->Open(g_szHostname, g_iPort))
        {
          XBMC->Log(LOG_NOTICE, "Reconnect to VNSI Server succesfull");
          m_CurStatus = STATUS_OK;
          m_connected = true;
          m_retries   = 0;
          return true;
        }
      }
    }
  }
  return m_connected;
}

void SetClientConnected(bool yesNo)
{
  if (yesNo)
  {
    m_connected = true;
  }
  else
  {
    XBMC->Log(LOG_ERROR, "Lost connection to VNSI Server");

    if (VNSIData)
      VNSIData->Close();
    if (VNSIDemuxer)
      VNSIDemuxer->Close();
    if (VNSIRecording)
      VNSIRecording->Close();

    m_connected = false;
    m_CurStatus = STATUS_LOST_CONNECTION;
  }
}

extern "C" {

/***********************************************************
 * Standart AddOn related public library functions
 ***********************************************************/

ADDON_STATUS Create(void* hdl, void* props)
{
  if (!hdl || !props)
    return STATUS_UNKNOWN;

  PVR_PROPS* pvrprops = (PVR_PROPS*)props;

  XBMC = new cHelper_libXBMC_addon;
  if (!XBMC->RegisterMe(hdl))
    return STATUS_UNKNOWN;

  GUI = new cHelper_libXBMC_gui;
  if (!GUI->RegisterMe(hdl))
    return STATUS_UNKNOWN;

  PVR = new cHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
    return STATUS_UNKNOWN;

  XBMC->Log(LOG_DEBUG, "Creating VDR VNSI PVR-Client");

  m_CurStatus    = STATUS_UNKNOWN;
  g_clientID     = pvrprops->clientID;
  g_szUserPath   = pvrprops->userpath;
  g_szClientPath = pvrprops->clientpath;

  /* Read setting "host" from settings.xml */
  char * buffer;
  buffer = (char*) malloc (1024);
  buffer[0] = 0; /* Set the end of string */

  if (XBMC->GetSetting("host", buffer))
    g_szHostname = buffer;
  else
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'host' setting, falling back to '%s' as default", DEFAULT_HOST);
    g_szHostname = DEFAULT_HOST;
  }
  buffer[0] = 0; /* Set the end of string */

  /* Read setting "port" from settings.xml */
  if (!XBMC->GetSetting("port", &g_iPort))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'port' setting, falling back to '%i' as default", DEFAULT_PORT);
    g_iPort = DEFAULT_PORT;
  }

  /* Read setting "priority" from settings.xml */
  if (!XBMC->GetSetting("priority", &g_iPriority))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'priority' setting, falling back to %i as default", DEFAULT_PRIORITY);
    g_iPriority = DEFAULT_PRIORITY;
  }

  /* Read setting "convertchar" from settings.xml */
  if (!XBMC->GetSetting("convertchar", &g_bCharsetConv))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'convertchar' setting, falling back to 'false' as default");
    g_bCharsetConv = DEFAULT_CHARCONV;
  }

  /* Read setting "timeout" from settings.xml */
  if (!XBMC->GetSetting("timeout", &g_iConnectTimeout))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'timeout' setting, falling back to %i seconds as default", DEFAULT_TIMEOUT);
    g_iConnectTimeout = DEFAULT_TIMEOUT;
  }

  /* Read setting "handlemessages" from settings.xml */
  if (!XBMC->GetSetting("handlemessages", &g_bHandleMessages))
  {
    /* If setting is unknown fallback to defaults */
    XBMC->Log(LOG_ERROR, "Couldn't get 'handlemessages' setting, falling back to 'true' as default");
    g_bHandleMessages = DEFAULT_HANDLE_MSG;
  }

  VNSIData = new cVNSIData;
  if (!VNSIData->Open(g_szHostname, g_iPort))
  {
    m_CurStatus = STATUS_LOST_CONNECTION;
    return m_CurStatus;
  }

  if (!VNSIData->EnableStatusInterface(g_bHandleMessages))
    return m_CurStatus;

  m_CurStatus = STATUS_OK;
  m_bCreated = true;
  return m_CurStatus;
}

ADDON_STATUS GetStatus()
{
  return m_CurStatus;
}

void Destroy()
{
  if (m_bCreated)
  {
    delete VNSIData;
    VNSIData = NULL;
  }
  m_CurStatus = STATUS_UNKNOWN;
}

bool HasSettings()
{
  return true;
}

unsigned int GetSettings(StructSetting ***sSet)
{
  return 0;
}

ADDON_STATUS SetSetting(const char *settingName, const void *settingValue)
{
  string str = settingName;
  if (str == "host")
  {
    string tmp_sHostname;
    XBMC->Log(LOG_INFO, "Changed Setting 'host' from %s to %s", g_szHostname.c_str(), (const char*) settingValue);
    tmp_sHostname = g_szHostname;
    g_szHostname = (const char*) settingValue;
    if (tmp_sHostname != g_szHostname)
      return STATUS_NEED_RESTART;
  }
  else if (str == "port")
  {
    XBMC->Log(LOG_INFO, "Changed Setting 'port' from %u to %u", g_iPort, *(int*) settingValue);
    if (g_iPort != *(int*) settingValue)
    {
      g_iPort = *(int*) settingValue;
      return STATUS_NEED_RESTART;
    }
  }
  else if (str == "priority")
  {
    XBMC->Log(LOG_INFO, "Changed Setting 'priority' from %u to %u", g_iPriority, *(int*) settingValue);
    g_iPriority = *(int*) settingValue;
  }
  else if (str == "convertchar")
  {
    XBMC->Log(LOG_INFO, "Changed Setting 'convertchar' from %u to %u", g_bCharsetConv, *(bool*) settingValue);
    g_bCharsetConv = *(bool*) settingValue;
  }
  else if (str == "timeout")
  {
    XBMC->Log(LOG_INFO, "Changed Setting 'timeout' from %u to %u", g_iConnectTimeout, *(int*) settingValue);
    g_iConnectTimeout = *(int*) settingValue;
  }
  else if (str == "handlemessages")
  {
    XBMC->Log(LOG_INFO, "Changed Setting 'handlemessages' from %u to %u", g_bHandleMessages, *(bool*) settingValue);
    g_bHandleMessages = *(bool*) settingValue;
    if (VNSIData) VNSIData->EnableStatusInterface(g_bHandleMessages);
  }

  return STATUS_OK;
}

void Stop()
{
}

void FreeSettings()
{

}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

PVR_ERROR GetProperties(PVR_SERVERPROPS* props)
{
  props->SupportChannelLogo        = false;
  props->SupportTimeShift          = false;
  props->SupportEPG                = true;
  props->SupportRecordings         = true;
  props->SupportTimers             = true;
  props->SupportTV                 = true;
  props->SupportRadio              = true;
  props->SupportChannelSettings    = false;
  props->SupportDirector           = false;
  props->SupportBouquets           = false;
  props->HandleInputStream         = true;
  props->HandleDemuxing            = true;
  if (VNSIData && VNSIData->SupportChannelScan())
    props->SupportChannelScan      = true;
  else
    props->SupportChannelScan      = false;

  return PVR_ERROR_NO_ERROR;
}

const char * GetBackendName()
{
  static CStdString BackendName = VNSIData ? VNSIData->GetServerName() : "unknown";
  return BackendName.c_str();
}

const char * GetBackendVersion()
{
  static CStdString BackendVersion;
  if (VNSIData)
    BackendVersion.Format("%s (Protocol: %i)", VNSIData->GetVersion(), VNSIData->GetProtocol());
  return BackendVersion.c_str();
}

const char * GetConnectionString()
{
  static CStdString ConnectionString;
  if (VNSIData)
    ConnectionString.Format("%s:%i%s", g_szHostname.c_str(), g_iPort, IsClientConnected() ? "" : " (Not connected!)");
  else
    ConnectionString.Format("%s:%i (addon error!)", g_szHostname.c_str(), g_iPort);
  return ConnectionString.c_str();
}

PVR_ERROR GetDriveSpace(long long *total, long long *used)
{
  if (IsClientConnected() && VNSIData && VNSIData->GetDriveSpace(total, used))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR GetBackendTime(time_t *localTime, int *gmtOffset)
{
  if (IsClientConnected() && VNSIData && VNSIData->GetTime(localTime, gmtOffset))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR DialogChannelScan()
{
  cVNSIChannelScan scanner;
  scanner.Open();
  return PVR_ERROR_NO_ERROR;
}

/*******************************************/
/** PVR EPG Functions                     **/

PVR_ERROR RequestEPGForChannel(PVRHANDLE handle, const PVR_CHANNEL &channel, time_t start, time_t end)
{
  if (IsClientConnected() && VNSIData && VNSIData->GetEPGForChannel(handle, channel, start, end))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}


/*******************************************/
/** PVR Channel Functions                 **/

int GetNumChannels()
{
  if (!IsClientConnected(true) || !VNSIData)
    return 0;

  return VNSIData->GetChannelsCount();
}

PVR_ERROR RequestChannelList(PVRHANDLE handle, int radio)
{
  if (IsClientConnected(true) && VNSIData && VNSIData->GetChannelsList(handle, radio))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}


/*******************************************/
/** PVR Timer Functions                   **/

int GetNumTimers(void)
{
  if (!IsClientConnected() || !VNSIData)
    return 0;

  return VNSIData->GetTimersCount();
}

PVR_ERROR RequestTimerList(PVRHANDLE handle)
{
  if (IsClientConnected() && VNSIData && VNSIData->GetTimersList(handle))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}

PVR_ERROR AddTimer(const PVR_TIMERINFO &timerinfo)
{
  if (!IsClientConnected() || !VNSIData)
    return PVR_ERROR_SERVER_ERROR;

  return VNSIData->AddTimer(timerinfo);
}

PVR_ERROR DeleteTimer(const PVR_TIMERINFO &timerinfo, bool force)
{
  if (!IsClientConnected() || !VNSIData)
    return PVR_ERROR_SERVER_ERROR;

  return VNSIData->DeleteTimer(timerinfo, force);
}

PVR_ERROR RenameTimer(const PVR_TIMERINFO &timerinfo, const char *newname)
{
  if (!IsClientConnected() || !VNSIData)
    return PVR_ERROR_SERVER_ERROR;

  return VNSIData->RenameTimer(timerinfo, newname);
}

PVR_ERROR UpdateTimer(const PVR_TIMERINFO &timerinfo)
{
  if (!IsClientConnected() || !VNSIData)
    return PVR_ERROR_SERVER_ERROR;

  return VNSIData->UpdateTimer(timerinfo);
}


/*******************************************/
/** PVR Recording Functions               **/

int GetNumRecordings(void)
{
  if (!IsClientConnected() || !VNSIData)
    return 0;

  return VNSIData->GetRecordingsCount();
}

PVR_ERROR RequestRecordingsList(PVRHANDLE handle)
{
  if (!VNSIData || !IsClientConnected())
    return PVR_ERROR_SERVER_ERROR;

  return VNSIData->GetRecordingsList(handle);
}

PVR_ERROR DeleteRecording(const PVR_RECORDINGINFO &recinfo)
{
  if (IsClientConnected() && VNSIData && VNSIData->DeleteRecording(VNSIData->GetRecordingPath(recinfo.index)))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}

/*******************************************/
/** PVR Live Stream Functions             **/

bool OpenLiveStream(const PVR_CHANNEL &channelinfo)
{
  if (!IsClientConnected(true))
    return false;

  CloseLiveStream();

  VNSIDemuxer = new cVNSIDemux;
  return VNSIDemuxer->Open(channelinfo);
}

void CloseLiveStream()
{
  if (IsClientConnected() && VNSIDemuxer)
  {
    VNSIDemuxer->Close();
    delete VNSIDemuxer;
    VNSIDemuxer = NULL;
  }
}

PVR_ERROR GetStreamProperties(PVR_STREAMPROPS* props)
{
  if (IsClientConnected() && VNSIDemuxer && VNSIDemuxer->GetStreamProperties(props))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}

void DemuxAbort()
{
  if (IsClientConnected() && VNSIDemuxer) VNSIDemuxer->Abort();
}

DemuxPacket* DemuxRead()
{
  if (!IsClientConnected())
    return NULL;

  return VNSIDemuxer->Read();
}

int GetCurrentClientChannel()
{
  if (IsClientConnected() && VNSIDemuxer)
    return VNSIDemuxer->CurrentChannel();

  return -1;
}

bool SwitchChannel(const PVR_CHANNEL &channelinfo)
{
  if (IsClientConnected() && VNSIDemuxer)
    return VNSIDemuxer->SwitchChannel(channelinfo);

  return false;
}

PVR_ERROR SignalQuality(PVR_SIGNALQUALITY &qualityinfo)
{
  if (IsClientConnected() && VNSIDemuxer && VNSIDemuxer->GetSignalStatus(qualityinfo))
    return PVR_ERROR_NO_ERROR;

  return PVR_ERROR_SERVER_ERROR;
}


/*******************************************/
/** PVR Recording Stream Functions        **/

bool OpenRecordedStream(const PVR_RECORDINGINFO &recinfo)
{
  if (!IsClientConnected(true))
    return false;

  CloseRecordedStream();

  const CStdString& name = VNSIData->GetRecordingPath(recinfo.index);
  VNSIRecording = new cVNSIRecording;
  return VNSIRecording->Open(name);
}

void CloseRecordedStream(void)
{
  if (IsClientConnected() && VNSIRecording)
  {
    VNSIRecording->Close();
    delete VNSIRecording;
    VNSIRecording = NULL;
  }
}

int ReadRecordedStream(unsigned char* buf, int buf_size)
{
  if (!IsClientConnected())
    return -1;

  return VNSIRecording->Read(buf, buf_size);
}

long long SeekRecordedStream(long long pos, int whence)
{
  if (IsClientConnected() && VNSIRecording)
    return VNSIRecording->Seek(pos, whence);

  return -1;
}

long long PositionRecordedStream(void)
{
  if (IsClientConnected() && VNSIRecording)
    return VNSIRecording->Position();

  return 0;
}

long long LengthRecordedStream(void)
{
  if (IsClientConnected() && VNSIRecording)
    return VNSIRecording->Length();

  return 0;
}



/** UNUSED API FUNCTIONS */
PVR_ERROR MenuHook(const PVR_MENUHOOK &menuhook) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetNumBouquets() { return 0; }
PVR_ERROR RequestBouquetsList(PVRHANDLE handle, int radio) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(unsigned int number) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(unsigned int number, const char *newname) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR MoveChannel(unsigned int number, unsigned int newnumber) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DialogChannelSettings(const PVR_CHANNEL &channelinfo) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DialogAddChannel(const PVR_CHANNEL &channelinfo) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDINGINFO &recinfo, const char *newname) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool HaveCutmarks() { return false; }
PVR_ERROR RequestCutMarksList(PVRHANDLE handle) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR AddCutMark(const PVR_CUT_MARK &cutmark) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteCutMark(const PVR_CUT_MARK &cutmark) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR StartCut() { return PVR_ERROR_NOT_IMPLEMENTED; }
bool SwapLiveTVSecondaryStream() { return false; }
bool OpenSecondaryStream(const PVR_CHANNEL &channelinfo) { return false; }
void CloseSecondaryStream() {}
int ReadSecondaryStream(unsigned char* buf, int buf_size) { return 0; }
void DemuxReset(){}
void DemuxFlush(){}
int ReadLiveStream(unsigned char* buf, int buf_size) { return 0; }
long long SeekLiveStream(long long pos, int whence) { return -1; }
long long PositionLiveStream(void) { return -1; }
long long LengthLiveStream(void) { return -1; }
const char * GetLiveStreamURL(const PVR_CHANNEL &channelinfo) { return ""; }

}
