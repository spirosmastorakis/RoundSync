/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2014 University of California, Los Angeles
 *
 * This file is part of ChronoSync, synchronization library for distributed realtime
 * applications for NDN.
 *
 * ChronoSync is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * ChronoSync is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ChronoSync, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Zhenkai Zhu <http://irl.cs.ucla.edu/~zhenkai/>
 * @author Chaoyi Bian <bcy@pku.edu.cn>
 * @author Alexander Afanasyev <http://lasr.cs.ucla.edu/afanasyev/index.html>
 * @author Yingdi Yu <yingdi@cs.ucla.edu>
 * @author Pedro de las Heras Quiros <pedro.delasheras@urjc.es>
 * @author Eva M. Castro <eva.castro@urjc.es>
 */



#include "diff-state.hpp"

namespace chronosync {

ConstStatePtr
DiffState::diff() const
{
  StatePtr result = make_shared<State>();

  ConstDiffStatePtr state = m_next;
  while (static_cast<bool>(state)) {
    *result += *state;
    state = state->m_next;
  }

  return result;
}


ConstStatePtr
DiffState::getState() const
{
  StatePtr result = make_shared<State>();
  *result += *this;
  return result;
}


DiffStatePtr
DiffState::getStateFrom(Name prefix, bool& cumulativeOnly) const
{
  LeafContainer::iterator leaf = getLeaves().find(prefix);
  
  if (leaf != getLeaves().end()) {
    DiffStatePtr result = make_shared<DiffState>();
    result->update(prefix,  (*leaf)->getSeq());

    if ((*leaf)->getSeq() == 0) {
      cumulativeOnly = true;
      result->m_cumulativeInfo = m_cumulativeInfo;
    }
    else 
      cumulativeOnly = false;



    result->m_round = m_round;
    result->m_cumulativeDigest = m_cumulativeDigest;

    return result;
  }
  else 
    return NULL;
}

CumulativeInfoPtr
DiffState::getCumulativeInfo() const {
  return m_cumulativeInfo;
}

void
DiffState::setCumulativeInfo(CumulativeInfoPtr cumulativeInfo){
  m_cumulativeInfo = cumulativeInfo;
}




} // namespace chronosync
