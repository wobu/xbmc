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

#include "settings/GUISettings.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogYesNo.h"
#include "dialogs/GUIDialogOK.h"
#include "utils/log.h"

#include "PVRChannelGroupInternal.h"
#include "pvr/PVRDatabase.h"
#include "pvr/PVRManager.h"

CPVRChannelGroupInternal::CPVRChannelGroupInternal(bool bRadio) :
  CPVRChannelGroup(bRadio)
{
  m_iHiddenChannels = 0;
  m_iGroupId        = bRadio ? XBMC_INTERNAL_GROUP_RADIO : XBMC_INTERNAL_GROUP_TV;
  m_strGroupName    = g_localizeStrings.Get(bRadio ? 19216 : 19217);
  m_iSortOrder      = 0;
}

int CPVRChannelGroupInternal::Load()
{
  int iChannelCount = CPVRChannelGroup::Load();
  UpdateChannelPaths();

  return iChannelCount;
}

void CPVRChannelGroupInternal::UpdateChannelPaths(void)
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < size(); iChannelPtr++)
  {
    PVRChannelGroupMember member = at(iChannelPtr);
    member.channel->UpdatePath(member.iChannelNumber);
  }
}

void CPVRChannelGroupInternal::Unload()
{
  for (unsigned int iChannelPtr = 0; iChannelPtr < size(); iChannelPtr++)
  {
    delete at(iChannelPtr).channel;
  }

  CPVRChannelGroup::Unload();
}

bool CPVRChannelGroupInternal::Update()
{
  bool          bReturn  = false;
  CPVRDatabase *database = CPVRManager::Get()->GetTVDatabase();

  if (database && database->Open())
  {
    CPVRChannelGroupInternal PVRChannels_tmp(m_bRadio);

    PVRChannels_tmp.LoadFromClients(false);
    bReturn = UpdateGroupEntries(&PVRChannels_tmp);

    database->Close();
  }

  return bReturn;
}

bool CPVRChannelGroupInternal::UpdateTimers(void)
{
  /* update the timers with the new channel numbers */
  CPVRTimers *timers = CPVRManager::GetTimers();
  for (unsigned int ptr = 0; ptr < timers->size(); ptr++)
  {
    CPVRTimerInfoTag *timer = timers->at(ptr);
    const CPVRChannel *tag = GetByClient(timer->m_iChannelNumber, timer->m_iClientID);
    if (tag)
      timer->m_iChannelNumber = tag->ChannelNumber();
  }

  return true;
}

bool CPVRChannelGroupInternal::RemoveFromGroup(CPVRChannel *channel)
{
  bool bReturn = false;

  if (!channel)
    return bReturn;

  /* check if there are active timers on this channel if we are hiding it */
  if (!channel->IsHidden() && CPVRManager::GetTimers()->ChannelHasTimers(*channel))
  {
    /* delete the timers */
    CPVRManager::GetTimers()->DeleteTimersOnChannel(*channel);
  }

  /* check if this channel is currently playing if we are hiding it */
  if (!channel->IsHidden() &&
      (CPVRManager::Get()->IsPlayingTV() || CPVRManager::Get()->IsPlayingRadio()) &&
      (CPVRManager::Get()->GetCurrentPlayingItem()->GetPVRChannelInfoTag() == channel))
  {
    CGUIDialogOK::ShowAndGetInput(19098,19101,0,19102);
    return bReturn;
  }

  /* switch the hidden flag */
  channel->SetHidden(!channel->IsHidden());

  /* update the hidden channel counter */
  if (channel->IsHidden())
    ++m_iHiddenChannels;
  else
    --m_iHiddenChannels;

  /* update the database entry */
  channel->Persist();

  /* move the channel to the end of the list */
  MoveChannel(channel->ChannelNumber(), size());

  bReturn = true;

  return bReturn;
}

int CPVRChannelGroupInternal::LoadFromDb(bool bCompress /* = false */)
{
  CPVRDatabase *database = CPVRManager::Get()->GetTVDatabase();
  if (!database || !database->Open())
    return -1;

  int iChannelCount = size();

  if (database->GetChannels(this, m_bRadio) > 0)
  {
    if (bCompress)
      database->Compress(true);
  }
  else
  {
    CLog::Log(LOGINFO, "PVRChannelGroupInternal - %s - no channels in the database",
        __FUNCTION__);
  }

  database->Close();

  SortByChannelNumber();

  return size() - iChannelCount;
}

int CPVRChannelGroupInternal::LoadFromClients(bool bAddToDb /* = true */)
{
  int iCurSize = size();

  /* no channels returned */
  if (GetFromClients() == -1)
    return -1;

  /* sort by client channel number if this is the first time */
  if (iCurSize == 0)
    SortByClientChannelNumber();
  else
    SortByChannelNumber();

  /* remove invalid channels */
  RemoveInvalidChannels();

  /* set channel numbers */
  Renumber();

  /* try to find channel icons */
  SearchAndSetChannelIcons();

  /* persist */
  if (bAddToDb)
    CPVRChannelGroupInternal::Persist();

  return size() - iCurSize;
}

void CPVRChannelGroupInternal::Renumber(void)
{
  int iChannelNumber = 0;
  m_iHiddenChannels = 0;
  for (unsigned int ptr = 0; ptr < size();  ptr++)
  {
    if (at(ptr).channel->IsHidden())
    {
      m_iHiddenChannels++;
    }
    else
    {
      at(ptr).iChannelNumber = ++iChannelNumber;
      at(ptr).channel->UpdatePath(iChannelNumber);
    }
  }
}

int CPVRChannelGroupInternal::GetFromClients(void)
{
  CLIENTMAP *clients = CPVRManager::Get()->Clients();
  if (!clients)
    return 0;

  int iCurSize = size();

  /* get the channel list from each client */
  CLIENTMAPITR itrClients = clients->begin();
  while (itrClients != clients->end())
  {
    if ((*itrClients).second->ReadyToUse() && (*itrClients).second->GetNumChannels() > 0)
      (*itrClients).second->GetChannelList(*this, m_bRadio);

    itrClients++;
  }

  return size() - iCurSize;
}

bool CPVRChannelGroupInternal::UpdateChannel(const CPVRChannel &channel)
{
  CPVRChannel *updateChannel = (CPVRChannel *) GetByUniqueID(channel.UniqueID());

  if (!updateChannel)
  {
    updateChannel = new CPVRChannel(channel.IsRadio());
    PVRChannelGroupMember newMember = { updateChannel, 0 };
    push_back(newMember);
    updateChannel->SetUniqueID(channel.UniqueID());
  }
  updateChannel->UpdateFromClient(channel);

  return updateChannel->Persist(!m_bLoaded);
}

bool CPVRChannelGroupInternal::UpdateGroupEntries(CPVRChannelGroup *channels)
{
  CPVRDatabase *database = CPVRManager::Get()->GetTVDatabase();
  if(!database && !database->Open())
    return false;

  int iSize = size();
  for (int ptr = 0; ptr < iSize; ptr++)
  {
    CPVRChannel *channel = at(ptr).channel;

    /* ignore virtual channels */
    if (channel->IsVirtual())
      continue;

    /* check if this channel is still present */
    const CPVRChannel *existingChannel = channels->GetByUniqueID(channel->UniqueID());
    if (existingChannel)
    {
      /* if it's present, update the current tag */
      if (channel->UpdateFromClient(*existingChannel))
      {
        channel->Persist(true);
        CLog::Log(LOGINFO,"PVRChannelGroupInternal - %s - updated %s channel '%s'",
            __FUNCTION__, m_bRadio ? "radio" : "TV", channel->ChannelName().c_str());
      }

      /* remove this tag from the temporary channel list */
      channels->RemoveByUniqueID(channel->UniqueID());
    }
    else
    {
      /* channel is no longer present */
      CLog::Log(LOGINFO,"PVRChannelGroupInternal - %s - removing %s channel '%s'",
          __FUNCTION__, m_bRadio ? "radio" : "TV", channel->ChannelName().c_str());
      channel->Delete();
      erase(begin() + ptr);
      ptr--;
      iSize--;
    }
  }

  /* the temporary channel list only contains new channels now */
  for (unsigned int ptr = 0; ptr < channels->size(); ptr++)
  {
    CPVRChannel *channel = channels->at(ptr).channel;
    channel->Persist(true);

    PVRChannelGroupMember member = { channel, size() };
    push_back(member);

    CLog::Log(LOGINFO,"PVRChannelGroupInternal - %s - added %s channel '%s'",
        __FUNCTION__, m_bRadio ? "radio" : "TV", channel->ChannelName().c_str());
  }

  /* post the queries generated by the update */
  database->CommitInsertQueries();

  database->Close();

  Renumber();

  return true;
}


bool CPVRChannelGroupInternal::Persist(void)
{
  bool bReturn = false;
  bool bRefreshChannelList = false;
  CPVRDatabase *database = CPVRManager::Get()->GetTVDatabase();

  if (!database || !database->Open())
    return bReturn;

  CLog::Log(LOGDEBUG, "CPVRChannelGroupInternal - %s - persisting %d channels",
      __FUNCTION__, size());
  bReturn = true;
  for (unsigned int iChannelPtr = 0; iChannelPtr < size(); iChannelPtr++)
  {
    /* if this channel has an invalid ID, reload the list afterwards */
    bRefreshChannelList = at(iChannelPtr).channel->ChannelID() <= 0;

    bReturn = at(iChannelPtr).channel->Persist(true) && bReturn;
  }

  if (bReturn)
  {
    if (bRefreshChannelList)
    {
      database->CommitInsertQueries();
      CLog::Log(LOGDEBUG, "PVRChannelGroup - %s - reloading the channels list to get channel IDs", __FUNCTION__);
      Unload();
      bReturn = LoadFromDb(true) > 0;
    }

    if (bReturn)
      bReturn = CPVRChannelGroup::Persist();
  }
  database->Close();

  return bReturn;
}

