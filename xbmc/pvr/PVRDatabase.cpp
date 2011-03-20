/*
 *      Copyright (C) 2005-2010 Team XBMC
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

#include "PVRDatabase.h"
#include "settings/AdvancedSettings.h"
#include "settings/VideoSettings.h"
#include "utils/log.h"

#include "PVRManager.h"
#include "channels/PVRChannelGroupsContainer.h"
#include "channels/PVRChannelGroupInternal.h"

using namespace std;
using namespace dbiplus;

CPVRDatabase::CPVRDatabase(void)
{
}

CPVRDatabase::~CPVRDatabase(void)
{
}

bool CPVRDatabase::Open()
{
  return CDatabase::Open(g_advancedSettings.m_databaseTV);
}

bool CPVRDatabase::CreateTables()
{
  bool bReturn = false;

  try
  {
    CDatabase::CreateTables();

    CLog::Log(LOGINFO, "PVRDB - %s - creating tables", __FUNCTION__);

    CLog::Log(LOGDEBUG, "PVRDB - %s - creating table 'clients'", __FUNCTION__);
    m_pDS->exec(
        "CREATE TABLE clients ("
          "idClient integer primary key, "
          "sName    varchar(64), "
          "sUid     varchar(32)"
        ");"
    );

    CLog::Log(LOGDEBUG, "PVRDB - %s - creating table 'channels'", __FUNCTION__);
    m_pDS->exec(
        "CREATE TABLE channels ("
          "idChannel            integer primary key, "
          "iUniqueId            integer, "
          "bIsRadio             bool, "
          "bIsHidden            bool, "
          "sIconPath            varchar(255), "
          "sChannelName         varchar(64), "
          "bIsVirtual           bool, "
          "bEPGEnabled          bool, "
          "sEPGScraper          varchar(32), "
          "iLastWatched         integer,"

          // TODO use mapping table
          "iClientId            integer, "
          "iClientChannelNumber integer, "
          "sInputFormat         varchar(32), "
          "sStreamURL           varchar(255), "
          "iEncryptionSystem    integer"
        ");"
    );
    m_pDS->exec("CREATE INDEX idx_channels_iClientId on channels(iClientId);");
    m_pDS->exec("CREATE INDEX idx_channels_iLastWatched on channels(iLastWatched);");
    m_pDS->exec("CREATE INDEX idx_channels_bIsRadio on channels(bIsRadio);");
    m_pDS->exec("CREATE INDEX idx_channels_bIsHidden on channels(bIsHidden);");

    // TODO use a mapping table so multiple backends per channel can be implemented
    //    CLog::Log(LOGDEBUG, "PVRDB - %s - creating table 'map_channels_clients'", __FUNCTION__);
    //    m_pDS->exec(
    //        "CREATE TABLE map_channels_clients ("
    //          "idChannel             integer primary key, "
    //          "idClient              integer, "
    //          "iClientChannelNumber  integer,"
    //          "sInputFormat          string,"
    //          "sStreamURL            string,"
    //          "iEncryptionSystem     integer"
    //        ");"
    //    );
    //    m_pDS->exec("CREATE UNIQUE INDEX idx_idChannel_idClient on map_channels_clients(idChannel, idClient);");

    CLog::Log(LOGDEBUG, "PVRDB - %s - creating table 'channelgroups'", __FUNCTION__);
    m_pDS->exec(
        "CREATE TABLE channelgroups ("
          "idGroup    integer primary key,"
          "bIsRadio   bool, "
          "sName      varchar(64),"
          "iSortOrder integer"
        ");"
    );
    m_pDS->exec("CREATE INDEX idx_channelgroups_bIsRadio on channelgroups(bIsRadio);");

    CLog::Log(LOGDEBUG, "PVRDB - %s - creating table 'map_channelgroups_channels'", __FUNCTION__);
    m_pDS->exec(
        "CREATE TABLE map_channelgroups_channels ("
          "idChannel      integer, "
          "idGroup        integer, "
          "iChannelNumber integer"
        ");"
    );
    m_pDS->exec("CREATE UNIQUE INDEX idx_idChannel_idGroup on map_channelgroups_channels(idChannel, idGroup);");
    m_pDS->exec("CREATE INDEX idx_idGroup_iChannelNumber on map_channelgroups_channels(idGroup, iChannelNumber);");

    CLog::Log(LOGDEBUG, "PVRDB - %s - creating table 'channelsettings'", __FUNCTION__);
    m_pDS->exec(
        "CREATE TABLE channelsettings ("
          "idChannel            integer primary key, "
          "iInterlaceMethod     integer, "
          "iViewMode            integer, "
          "fCustomZoomAmount    float, "
          "fPixelRatio          float, "
          "iAudioStream         integer, "
          "iSubtitleStream      integer,"
          "fSubtitleDelay       float, "
          "bSubtitles           bool, "
          "fBrightness          float, "
          "fContrast            float, "
          "fGamma               float,"
          "fVolumeAmplification float, "
          "fAudioDelay          float, "
          "bOutputToAllSpeakers bool, "
          "bCrop                bool, "
          "iCropLeft            integer, "
          "iCropRight           integer, "
          "iCropTop             integer, "
          "iCropBottom          integer, "
          "fSharpness           float, "
          "fNoiseReduction      float"
        ");"
    );

    bReturn = true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - unable to create TV tables:%i",
        __FUNCTION__, (int)GetLastError());
    bReturn = false;
  }

  return bReturn;
}

bool CPVRDatabase::UpdateOldVersion(int iVersion)
{
  bool bReturn = true;

  BeginTransaction();

  try
  {
    if (iVersion == 11)
    {
      m_pDS->exec("DROP VIEW vw_last_watched");
    }
    else
    {
      CLog::Log(LOGERROR, "PVRDB - %s - updating from this old table versions not supported. please delete '%s'",
          __FUNCTION__, GetBaseDBName());
      bReturn = false;
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "Error attempting to update the database version!");
    bReturn = false;
  }

  if (bReturn)
    CommitTransaction();
  else
    RollbackTransaction();

  return bReturn;
}

/********** Channel methods **********/

bool CPVRDatabase::DeleteChannels()
{
  CLog::Log(LOGDEBUG, "PVRDB - %s - deleting all channels from the database", __FUNCTION__);

  return DeleteValues("channels");
}

bool CPVRDatabase::DeleteClientChannels(int iClientId)
{
  /* invalid client Id */
  if (iClientId <= 0)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid client id: %i",
        __FUNCTION__, iClientId);
    return false;
  }

  CLog::Log(LOGDEBUG, "PVRDB - %s - deleting all channels from client '%i' from the database",
      __FUNCTION__, iClientId);

  CStdString strWhereClause = FormatSQL("iClientId = %u", iClientId);
  return DeleteValues("channels", strWhereClause);
}

int CPVRDatabase::Persist(const CPVRChannel &channel, bool bQueueWrite /* = false */)
{
  int iReturn = -1;

  /* invalid channel */
  if (channel.UniqueID() <= 0)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid channel uid: %d",
        __FUNCTION__, channel.UniqueID());
    return iReturn;
  }

  CStdString strQuery;

  if (channel.ChannelID() <= 0)
  {
    /* new channel */
    strQuery = FormatSQL("INSERT INTO channels ("
        "iUniqueId, bIsRadio, bIsHidden, "
        "sIconPath, sChannelName, bIsVirtual, bEPGEnabled, sEPGScraper, iLastWatched, iClientId, "
        "iClientChannelNumber, sInputFormat, sStreamURL, iEncryptionSystem) "
        "VALUES (%i, %i, %i, '%s', '%s', %i, %i, '%s', %u, %i, %i, '%s', '%s', %i);",
        channel.UniqueID(), (channel.IsRadio() ? 1 :0), (channel.IsHidden() ? 1 : 0),
        channel.IconPath().c_str(), channel.ChannelName().c_str(), (channel.IsVirtual() ? 1 : 0), (channel.EPGEnabled() ? 1 : 0), channel.EPGScraper().c_str(), channel.LastWatched(), channel.ClientID(),
        channel.ClientChannelNumber(), channel.InputFormat().c_str(), channel.StreamURL().c_str(), channel.EncryptionSystem());
  }
  else
  {
    /* update channel */
    strQuery = FormatSQL("REPLACE INTO channels ("
        "iUniqueId, bIsRadio, bIsHidden, "
        "sIconPath, sChannelName, bIsVirtual, bEPGEnabled, sEPGScraper, iLastWatched, iClientId, "
        "iClientChannelNumber, sInputFormat, sStreamURL, iEncryptionSystem, idChannel) "
        "VALUES (%i, %i, %i, '%s', '%s', %i, %i, '%s', %u, %i, %i, '%s', '%s', %i, %i);",
        channel.UniqueID(), (channel.IsRadio() ? 1 :0), (channel.IsHidden() ? 1 : 0),
        channel.IconPath().c_str(), channel.ChannelName().c_str(), (channel.IsVirtual() ? 1 : 0), (channel.EPGEnabled() ? 1 : 0), channel.EPGScraper().c_str(), channel.LastWatched(), channel.ClientID(),
        channel.ClientChannelNumber(), channel.InputFormat().c_str(), channel.StreamURL().c_str(), channel.EncryptionSystem(), channel.ChannelID());
  }

  if (bQueueWrite)
  {
    QueueInsertQuery(strQuery);
    iReturn = 0;
  }
  else if (ExecuteQuery(strQuery))
  {
    iReturn = (channel.ChannelID() <= 0) ? (int) m_pDS->lastinsertid() : channel.ChannelID();
  }

  return iReturn;
}

bool CPVRDatabase::Delete(const CPVRChannel &channel)
{
  /* invalid channel */
  if (channel.ChannelID() <= 0)
    return false;

  CStdString strWhereClause = FormatSQL("idChannel = %u", channel.ChannelID());
  return DeleteValues("channels", strWhereClause);
}

int CPVRDatabase::GetChannels(CPVRChannelGroupInternal *results, bool bIsRadio)
{
  int iReturn = 0;

  CStdString strQuery = FormatSQL("SELECT channels.idChannel, channels.iUniqueId, channels.bIsRadio, channels.bIsHidden, "
      "channels.sIconPath, channels.sChannelName, channels.bIsVirtual, channels.bEPGEnabled, channels.sEPGScraper, channels.iLastWatched, channels.iClientId, "
      "channels.iClientChannelNumber, channels.sInputFormat, channels.sInputFormat, channels.sStreamURL, channels.iEncryptionSystem, map_channelgroups_channels.iChannelNumber "
      "FROM channels "
      "LEFT JOIN map_channelgroups_channels ON map_channelgroups_channels.idChannel = channels.idChannel AND map_channelgroups_channels.idGroup = %u "
      "WHERE channels.bIsRadio = %u;", bIsRadio ? XBMC_INTERNAL_GROUP_RADIO : XBMC_INTERNAL_GROUP_TV, bIsRadio ? 1 : 0);
  if (ResultQuery(strQuery))
  {
    try
    {
      while (!m_pDS->eof())
      {
        CPVRChannel *channel = new CPVRChannel();

        channel->m_iChannelId              = m_pDS->fv("idChannel").get_asInt();
        channel->m_iUniqueId               = m_pDS->fv("iUniqueId").get_asInt();
        channel->m_bIsRadio                = m_pDS->fv("bIsRadio").get_asBool();
        channel->m_bIsHidden               = m_pDS->fv("bIsHidden").get_asBool();
        channel->m_strIconPath             = m_pDS->fv("sIconPath").get_asString();
        channel->m_strChannelName          = m_pDS->fv("sChannelName").get_asString();
        channel->m_bIsVirtual              = m_pDS->fv("bIsVirtual").get_asBool();
        channel->m_bEPGEnabled             = m_pDS->fv("bEPGEnabled").get_asBool();
        channel->m_strEPGScraper           = m_pDS->fv("sEPGScraper").get_asString();
        channel->m_iLastWatched            = (time_t) m_pDS->fv("iLastWatched").get_asInt();
        channel->m_iClientId               = m_pDS->fv("iClientId").get_asInt();
        channel->m_iClientChannelNumber    = m_pDS->fv("iClientChannelNumber").get_asInt();
        channel->m_strInputFormat          = m_pDS->fv("sInputFormat").get_asString();
        channel->m_strStreamURL            = m_pDS->fv("sStreamURL").get_asString();
        channel->m_iClientEncryptionSystem = m_pDS->fv("iEncryptionSystem").get_asInt();

        CLog::Log(LOGDEBUG, "PVRDB - %s - channel '%s' loaded from the database",
            __FUNCTION__, channel->m_strChannelName.c_str());
        results->InsertInGroup(channel, m_pDS->fv("iChannelNumber").get_asInt());
        m_pDS->next();
        ++iReturn;
      }
      m_pDS->close();
    }
    catch (...)
    {
      CLog::Log(LOGERROR, "PVRDB - %s - couldn't load channels from the database", __FUNCTION__);
    }
  }

  m_pDS->close();
  return iReturn;
}

bool CPVRDatabase::DeleteChannelSettings()
{
  CLog::Log(LOGDEBUG, "PVRDB - %s - deleting all channel settings from the database", __FUNCTION__);
  return DeleteValues("channelsettings");
}

bool CPVRDatabase::DeleteChannelSettings(const CPVRChannel &channel)
{
  bool bReturn = false;

  /* invalid channel */
  if (channel.ChannelID() <= 0)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid channel id: %i",
        __FUNCTION__, channel.ChannelID());
    return bReturn;
  }

  CStdString strWhereClause = FormatSQL("idChannel = %u", channel.ChannelID());
  return DeleteValues("channelsettings", strWhereClause);
}

bool CPVRDatabase::GetChannelSettings(const CPVRChannel &channel, CVideoSettings &settings)
{
  bool bReturn = false;

  /* invalid channel */
  if (channel.ChannelID() <= 0)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid channel id: %i",
        __FUNCTION__, channel.ChannelID());
    return bReturn;
  }

  CStdString strQuery = FormatSQL("SELECT * FROM channelsettings WHERE idChannel = %u;", channel.ChannelID());

  if (ResultQuery(strQuery))
  {
    try
    {
      if (m_pDS->num_rows() > 0)
      {
        settings.m_AudioDelay           = m_pDS->fv("fAudioDelay").get_asFloat();
        settings.m_AudioStream          = m_pDS->fv("iAudioStream").get_asInt();
        settings.m_Brightness           = m_pDS->fv("fBrightness").get_asFloat();
        settings.m_Contrast             = m_pDS->fv("fContrast").get_asFloat();
        settings.m_CustomPixelRatio     = m_pDS->fv("fPixelRatio").get_asFloat();
        settings.m_NoiseReduction       = m_pDS->fv("fNoiseReduction").get_asFloat();
        settings.m_Sharpness            = m_pDS->fv("fSharpness").get_asFloat();
        settings.m_CustomZoomAmount     = m_pDS->fv("fCustomZoomAmount").get_asFloat();
        settings.m_Gamma                = m_pDS->fv("fGamma").get_asFloat();
        settings.m_SubtitleDelay        = m_pDS->fv("fSubtitleDelay").get_asFloat();
        settings.m_SubtitleOn           = m_pDS->fv("bSubtitles").get_asBool();
        settings.m_SubtitleStream       = m_pDS->fv("iSubtitleStream").get_asInt();
        settings.m_ViewMode             = m_pDS->fv("iViewMode").get_asInt();
        settings.m_Crop                 = m_pDS->fv("bCrop").get_asBool();
        settings.m_CropLeft             = m_pDS->fv("iCropLeft").get_asInt();
        settings.m_CropRight            = m_pDS->fv("iCropRight").get_asInt();
        settings.m_CropTop              = m_pDS->fv("iCropTop").get_asInt();
        settings.m_CropBottom           = m_pDS->fv("iCropBottom").get_asInt();
        settings.m_InterlaceMethod      = (EINTERLACEMETHOD)m_pDS->fv("iInterlaceMethod").get_asInt();
        settings.m_VolumeAmplification  = m_pDS->fv("fVolumeAmplification").get_asFloat();
        settings.m_OutputToAllSpeakers  = m_pDS->fv("bOutputToAllSpeakers").get_asBool();
        settings.m_SubtitleCached       = false;

        bReturn = true;
      }

      m_pDS->close();
    }
    catch(...)
    {
      CLog::Log(LOGERROR, "PVRDB - %s - failed to get channel settings for channel '%s'",
          __FUNCTION__, channel.ChannelName().c_str());
    }
  }

  return bReturn;
}

bool CPVRDatabase::PersistChannelSettings(const CPVRChannel &channel, const CVideoSettings &settings)
{
  /* invalid channel */
  if (channel.ChannelID() <= 0)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid channel id: %i",
        __FUNCTION__, channel.ChannelID());
    return false;
  }

  CStdString strQuery = FormatSQL(
      "REPLACE INTO channelsettings "
        "(idChannel, iInterlaceMethod, iViewMode, fCustomZoomAmount, fPixelRatio, iAudioStream, iSubtitleStream, fSubtitleDelay, "
         "bSubtitles, fBrightness, fContrast, fGamma, fVolumeAmplification, fAudioDelay, bOutputToAllSpeakers, bCrop, iCropLeft, "
         "iCropRight, iCropTop, iCropBottom, fSharpness, fNoiseReduction) VALUES "
         "(%i, %i, %i, %f, %f, %i, %i, %f, %i, %f, %f, %f, %f, %f, %i, %i, %i, %i, %i, %i, %f, %f);",
       channel.ChannelID(), settings.m_InterlaceMethod, settings.m_ViewMode, settings.m_CustomZoomAmount, settings.m_CustomPixelRatio,
       settings.m_AudioStream, settings.m_SubtitleStream, settings.m_SubtitleDelay, settings.m_SubtitleOn,
       settings.m_Brightness, settings.m_Contrast, settings.m_Gamma, settings.m_VolumeAmplification, settings.m_AudioDelay,
       settings.m_OutputToAllSpeakers, settings.m_Crop, settings.m_CropLeft, settings.m_CropRight, settings.m_CropTop,
       settings.m_CropBottom, settings.m_Sharpness, settings.m_NoiseReduction);

  return ExecuteQuery(strQuery);
}

/********** Channel group methods **********/

bool CPVRDatabase::RemoveChannelsFromGroup(int iGroupId)
{
  CLog::Log(LOGDEBUG, "PVRDB - %s - deleting all channels from group %d from the database", __FUNCTION__, iGroupId);

   CStdString strWhereClause = FormatSQL("idGroup = %u", iGroupId);
   return DeleteValues("map_channelgroups_channels", strWhereClause);
}

bool CPVRDatabase::DeleteChannelGroups(bool bRadio /* = false */)
{
  CLog::Log(LOGDEBUG, "PVRDB - %s - deleting all channel groups from the database", __FUNCTION__);

  CStdString strWhereClause = FormatSQL("bIsRadio = %u", (bRadio ? 1 : 0));
  return DeleteValues("channelgroups", strWhereClause);
}

bool CPVRDatabase::DeleteChannelGroup(int iGroupId, bool bRadio /* = false */)
{
  /* invalid group id */
  if (iGroupId <= 0)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid group id: %d",
        __FUNCTION__, iGroupId);
    return false;
  }

  CStdString strWhereClause = FormatSQL("idGroup = %u AND bIsRadio = %u", iGroupId, bRadio);
  return DeleteValues("channelgroups", strWhereClause);
}

bool CPVRDatabase::GetChannelGroupList(CPVRChannelGroups &results, bool bRadio)
{
  bool bReturn = false;
  CStdString strQuery = FormatSQL("SELECT * from channelgroups WHERE bIsRadio = %u ORDER BY idGroup;", bRadio);

  if (ResultQuery(strQuery))
  {
    try
    {
      while (!m_pDS->eof())
      {
        CPVRChannelGroup data(m_pDS->fv("bIsRadio").get_asBool());

        data.SetGroupID(m_pDS->fv("idGroup").get_asInt());
        data.SetGroupName(m_pDS->fv("sName").get_asString());
        data.SetSortOrder(m_pDS->fv("iSortOrder").get_asInt());

        results.Update(data);
        m_pDS->next();
      }
      m_pDS->close();
      bReturn = true;
    }
    catch (...)
    {
      CLog::Log(LOGERROR, "%s - couldn't load channels from the database", __FUNCTION__);
    }
  }

  return bReturn;
}

int CPVRDatabase::GetChannelsInGroup(CPVRChannelGroup *group)
{
  int iReturn = -1;

  /* invalid group id */
  if (group->GroupID() < 0)
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid group id: %d",
        __FUNCTION__, group->GroupID());
    return -1;
  }

  CStdString strQuery = FormatSQL("SELECT idChannel, iChannelNumber FROM map_channelgroups_channels WHERE idGroup = %u ORDER BY iChannelNumber", group->GroupID());
  if (ResultQuery(strQuery))
  {
    iReturn = 0;

    try
    {
      while (!m_pDS->eof())
      {
        int iChannelId = m_pDS->fv("idChannel").get_asInt();
        int iChannelNumber = m_pDS->fv("iChannelNumber").get_asInt();
        CPVRChannel *channel = (CPVRChannel *) CPVRManager::GetChannelGroups()->GetByChannelIDFromAll(iChannelId);

        if (channel && group->AddToGroup(channel, iChannelNumber))
          ++iReturn;

        m_pDS->next();
      }
      m_pDS->close();
    }
    catch(...)
    {
      CLog::Log(LOGERROR, "PVRDB - %s - failed to get channels", __FUNCTION__);
    }
  }

  return iReturn;
}

int CPVRDatabase::GetChannelGroupId(const CStdString &strGroupName, bool bRadio /* = false */)
{
  CStdString strWhereClause = FormatSQL("sName LIKE '%s' AND bIsRadio = %u", strGroupName.c_str(), (bRadio ? 1 : 0));
  CStdString strReturn = GetSingleValue("channelgroups", "idGroup", strWhereClause);

  m_pDS->close();

  return atoi(strReturn);
}

int CPVRDatabase::Persist(CPVRChannelGroup *group)
{
  int iReturn = -1;

  CStdString strQuery;

  if (group->GroupID() <= 0)
  {
    /* new group */
    strQuery = FormatSQL("INSERT INTO channelgroups ("
        "bIsRadio, sName, iSortOrder) "
        "VALUES (%i, '%s', %i);",
        (group->IsRadio() ? 1 :0), group->GroupName().c_str(), group->SortOrder());
  }
  else
  {
    /* update group */
    strQuery = FormatSQL("REPLACE INTO channelgroups ("
        "idGroup, bIsRadio, sName, iSortOrder) "
        "VALUES (%i, %i, '%s', %i);",
        group->GroupID(), (group->IsRadio() ? 1 :0), group->GroupName().c_str(), group->SortOrder());
  }

  if (ExecuteQuery(strQuery))
  {
    if (group->GroupID() <= 0)
      group->m_iGroupId = (int) m_pDS->lastinsertid();

    RemoveChannelsFromGroup(group->GroupID());

    for (unsigned int iChannelPtr = 0; iChannelPtr < group->size(); iChannelPtr++)
    {
      PVRChannelGroupMember member = group->at(iChannelPtr);
      strQuery = FormatSQL("REPLACE INTO map_channelgroups_channels ("
          "idGroup, idChannel, iChannelNumber) "
          "VALUES (%i, %i, %i);",
          group->GroupID(), member.channel->ChannelID(), member.iChannelNumber);
      QueueInsertQuery(strQuery);
    }

    CommitInsertQueries();

    iReturn = group->GroupID();
  }

  return iReturn;
}

/********** Client methods **********/

bool CPVRDatabase::DeleteClients()
{
  CLog::Log(LOGDEBUG, "PVRDB - %s - deleting all clients from the database", __FUNCTION__);

  return DeleteValues("clients") &&
      DeleteValues("map_channels_clients");
}

int CPVRDatabase::AddClient(const CStdString &strClientName, const CStdString &strClientUid)
{
  int iReturn = -1;

  /* invalid client uid or name */
  if (strClientName.IsEmpty() || strClientUid.IsEmpty())
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid client uid or name", __FUNCTION__);
    return iReturn;
  }

  /* only add this client if it's not already in the database */
  iReturn = GetClientId(strClientUid);
  if (iReturn <= 0)
  {
    CStdString strQuery = FormatSQL("INSERT INTO clients (sName, sUid) VALUES ('%s', '%s');",
        strClientName.c_str(), strClientUid.c_str());

    if (ExecuteQuery(strQuery))
    {
      iReturn = (int) m_pDS->lastinsertid();
    }
  }

  return iReturn;
}

bool CPVRDatabase::DeleteClient(const CStdString &strClientUid)
{
  /* invalid client uid */
  if (strClientUid.IsEmpty())
  {
    CLog::Log(LOGERROR, "PVRDB - %s - invalid client uid", __FUNCTION__);
    return false;
  }

  CStdString strWhereClause = FormatSQL("sUid = '%s'", strClientUid.c_str());
  return DeleteValues("clients", strWhereClause);
}

int CPVRDatabase::GetClientId(const CStdString &strClientUid)
{
  CStdString strWhereClause = FormatSQL("sUid = '%s'", strClientUid.c_str());
  CStdString strValue = GetSingleValue("clients", "idClient", strWhereClause);

  if (strValue.IsEmpty())
    return -1;

  return atol(strValue.c_str());
}
