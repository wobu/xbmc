/*
 *      Copyright (C) 2005-2011 Team XBMC
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

#include "GUIWindowPVRChannels.h"

#include "dialogs/GUIDialogFileBrowser.h"
#include "dialogs/GUIDialogNumeric.h"
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIWindowManager.h"
#include "pvr/PVRManager.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "pvr/dialogs/GUIDialogPVRGroupManager.h"
#include "pvr/windows/GUIWindowPVR.h"
#include "settings/GUISettings.h"
#include "settings/Settings.h"
#include "storage/MediaManager.h"

CGUIWindowPVRChannels::CGUIWindowPVRChannels(CGUIWindowPVR *parent, bool bRadio) :
  CGUIWindowPVRCommon(parent,
                      bRadio ? PVR_WINDOW_CHANNELS_RADIO : PVR_WINDOW_CHANNELS_TV,
                      bRadio ? CONTROL_BTNCHANNELS_RADIO : CONTROL_BTNCHANNELS_TV,
                      bRadio ? CONTROL_LIST_CHANNELS_RADIO: CONTROL_LIST_CHANNELS_TV)
{
  m_bRadio              = bRadio;
  m_selectedGroup       = NULL;
  m_bShowHiddenChannels = false;
}

void CGUIWindowPVRChannels::GetContextButtons(int itemNumber, CContextButtons &buttons) const
{
  if (itemNumber < 0 || itemNumber >= m_parent->m_vecItems->Size())
    return;
  CFileItemPtr pItem = m_parent->m_vecItems->Get(itemNumber);

  if (pItem->m_strPath == "pvr://channels/.add.channel")
  {
    /* If yes show only "New Channel" on context menu */
    buttons.Add(CONTEXT_BUTTON_ADD, 19046);                                           /* add new channel */
  }
  else
  {
    buttons.Add(CONTEXT_BUTTON_INFO, 19047);                                          /* channel info */
    buttons.Add(CONTEXT_BUTTON_FIND, 19003);                                          /* find similar program */
    buttons.Add(CONTEXT_BUTTON_PLAY_ITEM, 19000);                                     /* switch to channel */
    buttons.Add(CONTEXT_BUTTON_SET_THUMB, 20019);                                     /* change icon */
    buttons.Add(CONTEXT_BUTTON_GROUP_MANAGER, 19048);                                 /* group manager */
    buttons.Add(CONTEXT_BUTTON_HIDE, m_bShowHiddenChannels ? 19049 : 19054);          /* show/hide channel */

    if (m_parent->m_vecItems->Size() > 1 && !m_bShowHiddenChannels)
      buttons.Add(CONTEXT_BUTTON_MOVE, 116);                                          /* move channel up or down */

    if (m_bShowHiddenChannels || CPVRManager::GetChannelGroups()->GetGroupAll(false)->GetNumHiddenChannels() > 0)
      buttons.Add(CONTEXT_BUTTON_SHOW_HIDDEN, m_bShowHiddenChannels ? 19050 : 19051); /* show hidden/visible channels */

    if (CPVRManager::GetClients()->HasMenuHooks(pItem->GetPVRChannelInfoTag()->ClientID()))
      buttons.Add(CONTEXT_BUTTON_MENU_HOOKS, 19195);                                  /* PVR client specific action */
  }
}

bool CGUIWindowPVRChannels::OnContextButton(int itemNumber, CONTEXT_BUTTON button)
{
  if (itemNumber < 0 || itemNumber >= (int) m_parent->m_vecItems->Size())
    return false;
  CFileItemPtr pItem = m_parent->m_vecItems->Get(itemNumber);

  return OnContextButtonPlay(pItem.get(), button) ||
      OnContextButtonMove(pItem.get(), button) ||
      OnContextButtonHide(pItem.get(), button) ||
      OnContextButtonShowHidden(pItem.get(), button) ||
      OnContextButtonSetThumb(pItem.get(), button) ||
      OnContextButtonAdd(pItem.get(), button) ||
      OnContextButtonInfo(pItem.get(), button) ||
      OnContextButtonGroupManager(pItem.get(), button) ||
      CGUIWindowPVRCommon::OnContextButton(itemNumber, button);
}

const CPVRChannelGroup *CGUIWindowPVRChannels::SelectedGroup(void)
{
  const CPVRChannelGroup *group = m_selectedGroup;

  if (!group)
    group = CPVRManager::Get()->GetPlayingGroup(m_bRadio);

  SetSelectedGroup((CPVRChannelGroup *) group);

  return m_selectedGroup;
}

void CGUIWindowPVRChannels::SetSelectedGroup(CPVRChannelGroup *group)
{
  if (!group)
    return;

  m_selectedGroup = group;
  CPVRManager::Get()->SetPlayingGroup(m_selectedGroup);
}

const CPVRChannelGroup *CGUIWindowPVRChannels::SelectNextGroup(void)
{
  const CPVRChannelGroup *currentGroup = SelectedGroup();
  CPVRChannelGroup *nextGroup = (CPVRChannelGroup *) CPVRManager::GetChannelGroups()->Get(m_bRadio)->GetNextGroup(*currentGroup);
  if (nextGroup && *nextGroup != *currentGroup)
  {
    SetSelectedGroup(nextGroup);
    UpdateData();
  }

  return m_selectedGroup;
}

void CGUIWindowPVRChannels::UpdateData(void)
{
  if (m_bIsFocusing)
    return;

  CLog::Log(LOGDEBUG, "CGUIWindowPVRChannels - %s - update window '%s'. set view to %d",
      __FUNCTION__, GetName(), m_iControlList);
  m_bIsFocusing = true;
  m_bUpdateRequired = false;
  m_parent->m_vecItems->Clear();
  m_parent->m_viewControl.SetCurrentView(m_iControlList);

  const CPVRChannelGroup *currentGroup = SelectedGroup();
  if (!currentGroup)
    return;

  m_parent->m_vecItems->m_strPath.Format("pvr://channels/%s/%s/",
      m_bRadio ? "radio" : "tv",
      m_bShowHiddenChannels ? ".hidden" : currentGroup->GroupName());

  m_parent->Update(m_parent->m_vecItems->m_strPath);

  /* empty list */
  if (m_parent->m_vecItems->Size() == 0)
  {
    if (m_bShowHiddenChannels)
    {
      /* show the visible channels instead */
      m_bShowHiddenChannels = false;
      UpdateData();
      return;
    }
    else if (currentGroup->GroupID() > 0)
    {
      if (*currentGroup != *SelectNextGroup())
        return;
    }
  }

  m_parent->m_viewControl.SetItems(*m_parent->m_vecItems);
  m_parent->m_viewControl.SetSelectedItem(m_iSelected);

  m_parent->SetLabel(CONTROL_LABELHEADER, g_localizeStrings.Get(m_bRadio ? 19024 : 19023));
  if (m_bShowHiddenChannels)
    m_parent->SetLabel(CONTROL_LABELGROUP, g_localizeStrings.Get(19022));
  else
    m_parent->SetLabel(CONTROL_LABELGROUP, currentGroup->GroupName());
  m_bIsFocusing = false;
}

bool CGUIWindowPVRChannels::OnClickButton(CGUIMessage &message)
{
  bool bReturn = false;

  if (IsSelectedButton(message))
  {
    bReturn = true;
    SelectNextGroup();
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnClickList(CGUIMessage &message)
{
  bool bReturn = false;

  if (IsSelectedList(message))
  {
    bReturn = true;
    int iAction = message.GetParam1();
    int iItem = m_parent->m_viewControl.GetSelectedItem();

    if (iItem < 0 || iItem >= (int) m_parent->m_vecItems->Size())
      return bReturn;
    CFileItemPtr pItem = m_parent->m_vecItems->Get(iItem);

    /* process actions */
    if (iAction == ACTION_SELECT_ITEM || iAction == ACTION_MOUSE_LEFT_CLICK || iAction == ACTION_PLAY)
      ActionPlayChannel(pItem.get());
    else if (iAction == ACTION_SHOW_INFO)
      ShowEPGInfo(pItem.get());
    else if (iAction == ACTION_DELETE_ITEM)
      ActionDeleteChannel(pItem.get());
    else if (iAction == ACTION_CONTEXT_MENU || iAction == ACTION_MOUSE_RIGHT_CLICK)
      m_parent->OnPopupMenu(iItem);
    else
      bReturn = false;
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonAdd(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_ADD)
  {
    CGUIDialogOK::ShowAndGetInput(19033,0,19038,0);
    bReturn = true;
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonGroupManager(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_GROUP_MANAGER)
  {
    ShowGroupManager();
    bReturn = true;
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonHide(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_HIDE)
  {
    CPVRChannel *channel = item->GetPVRChannelInfoTag();
    if (!channel || channel->IsRadio() != m_bRadio)
      return bReturn;

    CGUIDialogYesNo* pDialog = (CGUIDialogYesNo*)g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
    if (!pDialog)
      return bReturn;

    pDialog->SetHeading(19039);
    pDialog->SetLine(0, "");
    pDialog->SetLine(1, channel->ChannelName());
    pDialog->SetLine(2, "");
    pDialog->DoModal();

    if (!pDialog->IsConfirmed())
      return bReturn;

    ((CPVRChannelGroup *) CPVRManager::Get()->GetPlayingGroup(m_bRadio))->RemoveFromGroup(channel);
    UpdateData();

    bReturn = true;
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonInfo(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_INFO)
  {
    ShowEPGInfo(item);
    bReturn = true;
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonMove(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_MOVE)
  {
    CPVRChannel *channel = item->GetPVRChannelInfoTag();
    if (!channel || channel->IsRadio() != m_bRadio)
      return bReturn;

    CStdString strIndex;
    strIndex.Format("%i", channel->ChannelNumber());
    CGUIDialogNumeric::ShowAndGetNumber(strIndex, g_localizeStrings.Get(19052));
    int newIndex = atoi(strIndex.c_str());

    if (newIndex != channel->ChannelNumber())
    {
      ((CPVRChannelGroup *) CPVRManager::Get()->GetPlayingGroup())->MoveChannel(channel->ChannelNumber(), newIndex);
      UpdateData();
    }

    bReturn = true;
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonPlay(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_PLAY_ITEM)
  {
    /* play channel */
    bReturn = PlayFile(item, g_guiSettings.GetBool("pvrplayback.playminimized"));
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonSetThumb(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_SET_THUMB)
  {
    if (g_settings.GetCurrentProfile().canWriteSources() && !g_passwordManager.IsProfileLockUnlocked())
      return bReturn;
    else if (!g_passwordManager.IsMasterLockUnlocked(true))
      return bReturn;

    /* setup our thumb list */
    CFileItemList items;
    CPVRChannel *channel = item->GetPVRChannelInfoTag();

    if (!channel->IconPath().IsEmpty())
    {
      /* add the current thumb, if available */
      CFileItemPtr current(new CFileItem("thumb://Current", false));
      current->SetThumbnailImage(channel->IconPath());
      current->SetLabel(g_localizeStrings.Get(20016));
      items.Add(current);
    }
    else if (item->HasThumbnail())
    {
      /* already have a thumb that the share doesn't know about - must be a local one, so we may as well reuse it */
      CFileItemPtr current(new CFileItem("thumb://Current", false));
      current->SetThumbnailImage(item->GetThumbnailImage());
      current->SetLabel(g_localizeStrings.Get(20016));
      items.Add(current);
    }

    /* and add a "no thumb" entry as well */
    CFileItemPtr nothumb(new CFileItem("thumb://None", false));
    nothumb->SetIconImage(item->GetIconImage());
    nothumb->SetLabel(g_localizeStrings.Get(20018));
    items.Add(nothumb);

    CStdString strThumb;
    VECSOURCES shares;
    if (g_guiSettings.GetString("pvrmenu.iconpath") != "")
    {
      CMediaSource share1;
      share1.strPath = g_guiSettings.GetString("pvrmenu.iconpath");
      share1.strName = g_localizeStrings.Get(19018);
      shares.push_back(share1);
    }
    g_mediaManager.GetLocalDrives(shares);
    if (!CGUIDialogFileBrowser::ShowAndGetImage(items, shares, g_localizeStrings.Get(1030), strThumb))
      return bReturn;

    if (strThumb != "thumb://Current")
    {
      if (strThumb == "thumb://None")
        strThumb = "";

      channel->SetIconPath(strThumb, true);
      UpdateData();
    }

    bReturn = true;
  }

  return bReturn;
}

bool CGUIWindowPVRChannels::OnContextButtonShowHidden(CFileItem *item, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  if (button == CONTEXT_BUTTON_SHOW_HIDDEN)
  {
    m_bShowHiddenChannels = !m_bShowHiddenChannels;
    UpdateData();
    bReturn = true;
  }

  return bReturn;
}

void CGUIWindowPVRChannels::ShowGroupManager(void)
{
  /* Load group manager dialog */
  CGUIDialogPVRGroupManager* pDlgInfo = (CGUIDialogPVRGroupManager*)g_windowManager.GetWindow(WINDOW_DIALOG_PVR_GROUP_MANAGER);
  if (!pDlgInfo)
    return;

  pDlgInfo->SetRadio(m_bRadio);
  pDlgInfo->DoModal();

  return;
}
