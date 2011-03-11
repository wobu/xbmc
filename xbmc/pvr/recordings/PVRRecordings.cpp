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

#include "FileItem.h"
#include "dialogs/GUIDialogOK.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "Util.h"
#include "URL.h"
#include "utils/log.h"
#include "threads/SingleLock.h"

#include "utils/URIUtils.h"
#include "pvr/PVRManager.h"
#include "PVRRecordings.h"

void CPVRRecordings::UpdateFromClients(void)
{
  CLIENTMAP *clients = CPVRManager::Get()->Clients();

  Clear();

  CLIENTMAPITR itr = clients->begin();
  while (itr != clients->end())
  {
    /* Load only if the client have Recordings */
    if ((*itr).second->GetNumRecordings() > 0)
    {
      (*itr).second->GetAllRecordings(this);
    }
    itr++;
  }
}

CStdString CPVRRecordings::TrimSlashes(const CStdString &strOrig) const
{
  CStdString strReturn(strOrig);
  while (strReturn.Left(1) == "/")
    strReturn.erase(0, 1);

  URIUtils::RemoveSlashAtEnd(strReturn);

  return strReturn;
}

const CStdString CPVRRecordings::GetDirectoryFromPath(const CStdString &strPath, const CStdString &strBase) const
{
  CStdString strReturn;
  CStdString strUsePath = TrimSlashes(strPath);
  CStdString strUseBase = TrimSlashes(strBase);

  /* strip the base or return an empty value if it doesn't fit or match */
  if (!strUseBase.IsEmpty())
  {
    if (strUsePath.GetLength() <= strUseBase.GetLength() || strUsePath.Left(strUseBase.GetLength()) != strUseBase)
      return strReturn;
    strUsePath.erase(0, strUseBase.GetLength());
  }

  /* check for more occurences */
  int iDelimiter = strUsePath.Find('/');
  if (iDelimiter > 0)
    strReturn = strUsePath.Left(iDelimiter);
  else
    strReturn = strUsePath;

  return TrimSlashes(strReturn);
}

bool CPVRRecordings::IsDirectoryMember(const CStdString &strDirectory, const CStdString &strEntryDirectory, bool bDirectMember /* = true */) const
{
  CStdString strUseDirectory = TrimSlashes(strDirectory);
  CStdString strUseEntryDirectory = TrimSlashes(strEntryDirectory);

  return strUseEntryDirectory.Left(strUseDirectory.GetLength()).Equals(strUseDirectory) &&
      (!bDirectMember || strUseEntryDirectory.Equals(strUseDirectory));
}

void CPVRRecordings::GetContents(const CStdString &strDirectory, CFileItemList *results) const
{
  for (unsigned int iRecordingPtr = 0; iRecordingPtr < size(); iRecordingPtr++)
  {
    CPVRRecording *current = at(iRecordingPtr);
    if (!IsDirectoryMember(strDirectory, current->m_strDirectory, true))
      continue;

    CFileItemPtr pFileItem(new CFileItem(*current));
    pFileItem->SetLabel2(current->m_recordingTime.GetAsLocalizedDateTime(true, false));
    pFileItem->m_dateTime = current->m_recordingTime;
    pFileItem->m_strPath.Format("pvr://recordings/%05i-%05i.pvr", current->m_clientID, current->m_clientIndex);
    results->Add(pFileItem);
  }
}

void CPVRRecordings::GetSubDirectories(const CStdString &strBase, CFileItemList *results, bool bAutoSkip /* = true */) const
{
  CStdString strUseBase = TrimSlashes(strBase);

  for (unsigned int iRecordingPtr = 0; iRecordingPtr < size(); iRecordingPtr++)
  {
    CPVRRecording *current = at(iRecordingPtr);
    const CStdString strCurrent = GetDirectoryFromPath(current->m_strDirectory, strUseBase);
    if (strCurrent.IsEmpty())
      continue;

    CStdString strFilePath;
    strFilePath.Format("pvr://recordings/%s/%s/", strUseBase.c_str(), strCurrent.c_str());

    if (!results->Contains(strFilePath))
    {
      CFileItemPtr pFileItem;
      pFileItem.reset(new CFileItem(strCurrent, true));
      pFileItem->m_strPath = strFilePath;
      pFileItem->SetLabel(strCurrent);
      pFileItem->SetLabelPreformated(true);
      results->Add(pFileItem);
    }
  }

  if (bAutoSkip && results->Size() == 1)
  {
    CStdString strGetPath;
    strGetPath.Format("%s/%s/", strUseBase.c_str(), results->Get(0)->GetLabel());

    results->Clear();

    CLog::Log(LOGDEBUG, "CPVRRecordings - %s - '%s' only has 1 subdirectory, selecting that directory ('%s')",
        __FUNCTION__, strUseBase.c_str(), strGetPath.c_str());
    GetSubDirectories(strGetPath, results, true);
    return;
  }

  if (results->Size() == 0)
  {
    GetContents(strUseBase, results);
  }
}

void CPVRRecordings::Process()
{
  CSingleLock lock(m_critSection);
  UpdateFromClients();
}

void CPVRRecordings::Unload()
{
  Clear();
}

bool CPVRRecordings::Update(bool bAsync /* = false */)
{
  if (!bAsync)
  {
    Process();
    return true;
  }
  else
  {
    Create();
    SetName("PVR Recordings Update");
    SetPriority(-1);
  }
  return false;
}

int CPVRRecordings::GetNumRecordings()
{
  CSingleLock lock(m_critSection);
  return size();
}

int CPVRRecordings::GetRecordings(CFileItemList* results)
{
  CSingleLock lock(m_critSection);

  for (unsigned int iRecordingPtr = 0; iRecordingPtr < size(); iRecordingPtr++)
  {
    CFileItemPtr pFileItem(new CFileItem(*at(iRecordingPtr)));
    results->Add(pFileItem);
  }

  return size();
}

bool CPVRRecordings::DeleteRecording(const CFileItem &item)
{
  bool bReturn = false;

  if (!item.IsPVRRecording())
  {
    CLog::Log(LOGERROR, "CPVRRecordings - %s - cannot delete file: no valid recording tag", __FUNCTION__);
    return bReturn;
  }

  const CPVRRecording* tag = item.GetPVRRecordingInfoTag();
  CSingleLock lock(m_critSection);
  if (tag->Delete())
  {
    bReturn = true;

    for (unsigned int iRecordingPtr = 0; iRecordingPtr < size(); iRecordingPtr++)
    {
      if (*at(iRecordingPtr) == *tag)
      {
        delete at(iRecordingPtr);
        erase(begin() + iRecordingPtr);
        break;
      }
    }
  }

  return bReturn;
}

bool CPVRRecordings::RenameRecording(CFileItem &item, CStdString &strNewName)
{
  bool bReturn = false;

  if (!item.IsPVRRecording())
  {
    CLog::Log(LOGERROR, "CPVRRecordings - %s - cannot rename file: no valid recording tag", __FUNCTION__);
    return bReturn;
  }

  CPVRRecording* tag = item.GetPVRRecordingInfoTag();
  return tag->Rename(strNewName);
}

bool CPVRRecordings::GetDirectory(const CStdString& strPath, CFileItemList &items)
{
  CSingleLock lock(m_critSection);

  CStdString strBase(strPath);
  URIUtils::RemoveSlashAtEnd(strBase);

  CURL url(strPath);
  CStdString strFileName = url.GetFileName();
  URIUtils::RemoveSlashAtEnd(strFileName);

  if (strFileName.Left(10) == "recordings")
  {
    strFileName.erase(0, 10);
    GetSubDirectories(strFileName, &items, true);

    return true;
  }

  return false;
}

CPVRRecording *CPVRRecordings::GetByPath(CStdString &path)
{
  CPVRRecording *tag = NULL;
  CSingleLock lock(m_critSection);

  CURL url(path);
  CStdString fileName = url.GetFileName();
  URIUtils::RemoveSlashAtEnd(fileName);

  if (fileName.Left(11) == "recordings/")
  {
    fileName.erase(0,11);
    int iClientID = atoi(fileName.c_str());
    fileName.erase(0,6);

    if (fileName.IsEmpty())
      return tag;

    int iClientIndex = atoi(fileName.c_str());

    for (unsigned int iRecordingPtr = 0; iRecordingPtr < size(); iRecordingPtr++)
    {
      CPVRRecording *recording = at(iRecordingPtr);

      if (recording->m_clientID == iClientID && recording->m_clientIndex == iClientIndex)
      {
        tag = recording;
        break;
      }
    }
  }

  return tag;
}

void CPVRRecordings::Clear()
{
  CSingleLock lock(m_critSection);

  for (unsigned int iRecordingPtr = 0; iRecordingPtr < size(); iRecordingPtr++)
    delete at(iRecordingPtr);
  erase(begin(), end());
}

void CPVRRecordings::UpdateEntry(const CPVRRecording &tag)
{
  bool bFound = false;
  CSingleLock lock(m_critSection);

  for (unsigned int iRecordingPtr = 0; iRecordingPtr < size(); iRecordingPtr++)
  {
    CPVRRecording *currentTag = at(iRecordingPtr);
    if (currentTag->m_clientID == tag.m_clientID &&
        currentTag->m_clientIndex == tag.m_clientIndex)
    {
      currentTag->Update(tag);
      bFound = true;
      break;
    }
  }

  if (!bFound)
  {
    CPVRRecording *newTag = new CPVRRecording();
    newTag->Update(tag);
    push_back(newTag);
  }
}
