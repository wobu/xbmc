#pragma once

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

#include "StdString.h"
#include "threads/CriticalSection.h"

class Observable;

class Observer
{
public:
  virtual void Notify(const Observable &obs, const CStdString& msg) = 0;
};

class Observable
{
public:
  Observable();
  virtual ~Observable();
  Observable &operator=(const Observable &observable);

  void AddObserver(Observer *o);
  void RemoveObserver(Observer *o);
  void NotifyObservers(const CStdString& msg = CStdString());
  void SetChanged(bool bSetTo = true);

private:
  bool                    m_bObservableChanged;
  std::vector<Observer *> m_observers;
  CCriticalSection        m_critSection;
};
