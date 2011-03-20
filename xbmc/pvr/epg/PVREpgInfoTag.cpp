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

#include "guilib/LocalizeStrings.h"

#include "PVREpg.h"
#include "PVREpgInfoTag.h"
#include "pvr/PVRManager.h"
#include "pvr/timers/PVRTimers.h"
#include "pvr/timers/PVRTimerInfoTag.h"
#include "pvr/channels/PVRChannel.h"

using namespace std;

CPVREpgInfoTag::CPVREpgInfoTag(const PVR_PROGINFO &data)
{
  Reset();
  Update(data);
}

void CPVREpgInfoTag::Reset()
{
  CEpgInfoTag::Reset();

  m_isRecording = false;
  m_Timer       = NULL;
}

const CPVRChannel *CPVREpgInfoTag::ChannelTag(void) const
{
  const CPVREpg *table = (const CPVREpg *) GetTable();
  return table ? table->Channel() : NULL;
}

void CPVREpgInfoTag::SetTimer(const CPVRTimerInfoTag *newTimer)
{
  if (!newTimer)
    m_Timer = NULL;

  m_Timer = newTimer;
}

void CPVREpgInfoTag::UpdatePath(void)
{
  if (!m_Epg)
    return;

  CStdString path;
  path.Format("pvr://guide/channel-%04i/%s.epg", ((CPVREpg *)m_Epg)->Channel()->ChannelNumber(), m_startTime.GetAsDBDateTime().c_str());
  SetPath(path);
}

void CPVREpgInfoTag::Update(const PVR_PROGINFO &tag)
{
  SetStart((time_t)tag.starttime);
  SetEnd((time_t)tag.endtime);
  SetTitle(tag.title);
  SetPlotOutline(tag.subtitle);
  SetPlot(tag.description);
  SetGenre(tag.genre_type, tag.genre_sub_type);
  SetParentalRating(tag.parental_rating);
  SetUniqueBroadcastID(tag.uid);
}

const CStdString &CPVREpgInfoTag::Icon(void) const
{
  if (m_strIconPath.IsEmpty() && m_Epg)
  {
    CPVREpg *pvrEpg = (CPVREpg *) m_Epg;
    if (pvrEpg->Channel())
      return pvrEpg->Channel()->IconPath();
  }

  return m_strIconPath;
}
