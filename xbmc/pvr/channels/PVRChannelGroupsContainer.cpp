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

#include "PVRChannelGroupsContainer.h"
#include "dialogs/GUIDialogOK.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

CPVRChannelGroupsContainer::CPVRChannelGroupsContainer(void)
{

  m_groupsRadio = new CPVRChannelGroups(true);
  m_groupsTV    = new CPVRChannelGroups(false);
}

CPVRChannelGroupsContainer::~CPVRChannelGroupsContainer(void)
{
  delete m_groupsRadio;
  delete m_groupsTV;
}

bool CPVRChannelGroupsContainer::Update(void)
{
  return m_groupsRadio->Update() &&
         m_groupsTV->Update();
}

bool CPVRChannelGroupsContainer::Load(void)
{
  Unload();

  return m_groupsRadio->Load() &&
         m_groupsTV->Load();
}

void CPVRChannelGroupsContainer::Unload(void)
{
  m_groupsRadio->Clear();
  m_groupsTV->Clear();
}

const CPVRChannelGroups *CPVRChannelGroupsContainer::Get(bool bRadio) const
{
  return bRadio ? m_groupsRadio : m_groupsTV;
}

const CPVRChannelGroup *CPVRChannelGroupsContainer::GetGroupAll(bool bRadio) const
{
  const CPVRChannelGroup *group = NULL;
  const CPVRChannelGroups *groups = Get(bRadio);
  if (groups)
    group = groups->GetGroupAll();

  return group;
}

const CPVRChannelGroup *CPVRChannelGroupsContainer::GetById(bool bRadio, int iGroupId) const
{
  const CPVRChannelGroup *group = NULL;
  const CPVRChannelGroups *groups = Get(bRadio);
  if (groups)
    group = groups->GetById(iGroupId);

  return group;
}

const CPVRChannel *CPVRChannelGroupsContainer::GetChannelById(int iChannelId) const
{
  const CPVRChannel *channel = m_groupsTV->GetGroupAll()->GetByChannelID(iChannelId);

  if (!channel)
    channel = m_groupsRadio->GetGroupAll()->GetByChannelID(iChannelId);

  return channel;
}

bool CPVRChannelGroupsContainer::GetGroupsDirectory(const CStdString &strBase, CFileItemList *results, bool bRadio)
{
  const CPVRChannelGroups *channelGroups = Get(bRadio);
  CFileItemPtr item;

  /* add all groups */
  for (unsigned int ptr = 0; ptr < channelGroups->size(); ptr++)
  {
    const CPVRChannelGroup *group = channelGroups->at(ptr);
    CStdString strGroup = strBase + "/" + group->GroupName() + "/";
    item.reset(new CFileItem(strGroup, true));
    item->SetLabel(group->GroupName());
    item->SetLabelPreformated(true);
    results->Add(item);
  }

  return true;
}

const CPVRChannel *CPVRChannelGroupsContainer::GetByPath(const CStdString &strPath)
{
  const CPVRChannelGroup *channels = NULL;
  int iChannelNumber = -1;

  /* get the filename from curl */
  CURL url(strPath);
  CStdString strFileName = url.GetFileName();
  URIUtils::RemoveSlashAtEnd(strFileName);

  CStdString strCheckPath;
  for (unsigned int bRadio = 0; bRadio <= 1; bRadio++)
  {
    for (unsigned int iGroupPtr = 0; iGroupPtr < Get(bRadio)->size(); iGroupPtr++)
    {
      const CPVRChannelGroup *group = Get(bRadio)->at(iGroupPtr);
      strCheckPath.Format("channels/%s/%s/", group->IsRadio() ? "radio" : "tv", group->GroupName().c_str());

      if (strFileName.Left(strCheckPath.length()) == strCheckPath)
      {
        strFileName.erase(0, strCheckPath.length());
        channels = group;
        iChannelNumber = atoi(strFileName.c_str());
        break;
      }
    }
  }

  return channels ? channels->GetByChannelNumber(iChannelNumber) : NULL;
}

bool CPVRChannelGroupsContainer::GetDirectory(const CStdString& strPath, CFileItemList &results)
{
  CStdString strBase(strPath);

  /* get the filename from curl */
  CURL url(strPath);
  CStdString fileName = url.GetFileName();
  URIUtils::RemoveSlashAtEnd(fileName);

  if (fileName == "channels")
  {
    CFileItemPtr item;

    /* all tv channels */
    item.reset(new CFileItem(strBase + "/tv/", true));
    item->SetLabel(g_localizeStrings.Get(19020));
    item->SetLabelPreformated(true);
    results.Add(item);

    /* all radio channels */
    item.reset(new CFileItem(strBase + "/radio/", true));
    item->SetLabel(g_localizeStrings.Get(19021));
    item->SetLabelPreformated(true);
    results.Add(item);

    return true;
  }
  else if (fileName == "channels/tv")
  {
    return GetGroupsDirectory(strBase, &results, false);
  }
  else if (fileName == "channels/radio")
  {
    return GetGroupsDirectory(strBase, &results, true);
  }
  else if (fileName.Left(12) == "channels/tv/")
  {
    CStdString strGroupName(fileName.substr(12));
    URIUtils::RemoveSlashAtEnd(strGroupName);
    const CPVRChannelGroup *group = GetTV()->GetByName(strGroupName);
    if (!group)
      group = GetGroupAllTV();
    if (group)
      group->GetMembers(&results, !fileName.Right(7).Equals(".hidden"));
    return true;
  }
  else if (fileName.Left(15) == "channels/radio/")
  {
    CStdString strGroupName(fileName.substr(15));
    URIUtils::RemoveSlashAtEnd(strGroupName);
    const CPVRChannelGroup *group = GetRadio()->GetByName(strGroupName);
    if (!group)
      group = GetGroupAllRadio();
    if (group)
      group->GetMembers(&results, !fileName.Right(7).Equals(".hidden"));
    return true;
  }

  return false;
}

int CPVRChannelGroupsContainer::GetNumChannelsFromAll()
{
  return GetGroupAllTV()->GetNumChannels() + GetGroupAllRadio()->GetNumChannels();
}

const CPVRChannel *CPVRChannelGroupsContainer::GetByUniqueID(int iClientChannelNumber, int iClientID)
{
  const CPVRChannel *channel = NULL;

  channel = GetGroupAllTV()->GetByClient(iClientChannelNumber, iClientID);

  if (channel == NULL)
    channel = GetGroupAllRadio()->GetByClient(iClientChannelNumber, iClientID);

  return channel;
}

const CPVRChannel *CPVRChannelGroupsContainer::GetByChannelIDFromAll(int iChannelID)
{
  const CPVRChannel *channel = NULL;

  channel = GetGroupAllTV()->GetByChannelID(iChannelID);

  if (channel == NULL)
    channel = GetGroupAllRadio()->GetByChannelID(iChannelID);

  return channel;
}

const CPVRChannel *CPVRChannelGroupsContainer::GetByUniqueIDFromAll(int iUniqueID)
{
  const CPVRChannel *channel;

  channel = GetGroupAllTV()->GetByUniqueID(iUniqueID);

  if (channel == NULL)
    channel = GetGroupAllRadio()->GetByUniqueID(iUniqueID);

  return NULL;
}

void CPVRChannelGroupsContainer::SearchMissingChannelIcons(void)
{
  CLog::Log(LOGINFO, "PVRChannelGroupsContainer - %s - starting channel icon search", __FUNCTION__);

  // TODO: Add Process dialog here
  ((CPVRChannelGroup *) GetGroupAllTV())->SearchAndSetChannelIcons(true);
  ((CPVRChannelGroup *) GetGroupAllRadio())->SearchAndSetChannelIcons(true);

  CGUIDialogOK::ShowAndGetInput(19103,0,20177,0);
}
