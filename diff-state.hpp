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

#ifndef CHRONOSYNC_DIFF_STATE_HPP
#define CHRONOSYNC_DIFF_STATE_HPP

#include <ndn-cxx/util/scheduler.hpp>

#include "state.hpp"

namespace chronosync {

typedef uint64_t RoundNo;

class DiffState;
typedef shared_ptr<DiffState> DiffStatePtr;
typedef shared_ptr<const DiffState> ConstDiffStatePtr;

typedef std::pair<RoundNo, ndn::ConstBufferPtr> CumulativeInfo;
typedef shared_ptr<CumulativeInfo> CumulativeInfoPtr;



/**
 * @brief Contains the diff info between two states.
 *
 */
class DiffState : public State
{
public:
  /**
   * @brief Set successor for the diff state
   *
   * @param next successor state
   */
  void
  setNext(ConstDiffStatePtr next)
  {
    m_next = next;
  }

  /**
   * @brief Set digest for the diff state (obtained from a corresponding full state)
   *
   * @param digest root digest of the full state
   */
  void
  setRootDigest(ndn::ConstBufferPtr rootDigest)
  {
    m_rootDigest = rootDigest;
  }

  /**
   * @brief Get root digest of the full state after applying the diff state
   */
  ndn::ConstBufferPtr
  getRootDigest() const
  {
    return m_rootDigest;
  }

  /**
   * @brief Update cumulative digest for this round / diff state 
   *
   * @param previousCumulativeDigest cumulative digest of the latest round before this 
   */
  void
  updateCumulativeDigest(ndn::ConstBufferPtr previousCumulativeDigest)
  {
    sha256Digest.reset();
    sha256Digest.update(previousCumulativeDigest->buf(), previousCumulativeDigest->size());
    sha256Digest.update(getRoundDigest()->buf(), getRoundDigest()->size());

    m_cumulativeDigest = sha256Digest.computeDigest();
  }

  /**
   * @brief Set cumulative digest for the diff state 
   *
   * @param cumulativeDigest 
   */
  void
  setCumulativeDigest(ndn::ConstBufferPtr cumulativeDigest)
  {
    m_cumulativeDigest = cumulativeDigest;
  }

  /**
   * @brief Get cumulative digest the Round Log 
   */
  ndn::ConstBufferPtr
  getCumulativeDigest() const
  {
    return m_cumulativeDigest;
  }


  /**
   * @brief Set round digest for the diff state 
   *
   */
  void
  updateRoundDigest()
  {
    // call inherited State::getDigest
    m_roundDigest = getDigest();
  }

  /**
   * @brief Get round digest for this diff state
   */
  ndn::ConstBufferPtr
  getRoundDigest() const
  {
    return m_roundDigest;
  }


  /**
   * @brief Set round number 
   *
   * @param round number for this diff state
   */
  void
  setRound(RoundNo round)
  {
    m_round = round;
  }


  /**
   * @brief Get round for this diff state
   */
  RoundNo
  getRound() const
  {
    return m_round;
  }


  /**
   * @brief Get exclude filter
   */
  ndn::Exclude
  getExcludeFilter() const
  {
    return m_excludeFilter;
  }


  /**
   * @brief Append exclude to m_excludeFilter
   *
   * @param exclude exclude component to be appended
   *
   */
  void
  appendExclude(const ndn::name::Component& exclude)
  {
    m_excludeFilter.appendExclude (exclude, false);
  }


  /**
   * @brief Accumulate differences from this state to the most current state
   *
   * This method assumes that the DiffState is in a log. It will iterate the all
   * the DiffState between its m_next DiffState and the last DiffState in the log,
   * and aggregate all the differences into one diff, which is represented as a
   * State object.
   *
   * @returns Accumulated differences from this state to the most current state
   */
  ConstStatePtr
  diff() const;
  
  /**
   * @brief returns the state of this round
   *
   * @returns state of this round
   */
  ConstStatePtr
  getState() const;

  /**
   * @brief returns the state of this round produced by prefix
   *
   * @param prefix the prefix for which we want to retrieve data
   * @param cumulativeOnly returns true if the state represents a CumulativeOnly Data
   *
   * @returns state of this round if there is any produced by prefix, NULL otherwise
   */
  DiffStatePtr
  getStateFrom(Name prefix, bool& cumulativeOnly) const;
  

  CumulativeInfoPtr
  getCumulativeInfo() const;

  void
  setCumulativeInfo(CumulativeInfoPtr cumulativeInfo);



  /**
   * @brief 
   */
  ndn::EventId
  getReexpressingSyncInterestId() const
  {
    return m_reexpressingSyncInterestId;
  }

  /**
   * @brief 
   */
  void
  setReexpressingSyncInterestId(ndn::EventId eventId) 
  {
    m_reexpressingSyncInterestId = eventId;
  }



private:
  mutable ndn::util::Sha256 sha256Digest; 
  ndn::ConstBufferPtr m_rootDigest; 
  ConstDiffStatePtr   m_next;

  ndn::ConstBufferPtr m_cumulativeDigest;
  ndn::ConstBufferPtr m_roundDigest; 
  RoundNo m_round;

  ndn::Exclude m_excludeFilter;

  CumulativeInfoPtr m_cumulativeInfo;


  ndn::EventId m_reexpressingSyncInterestId;

};

} // chronosync

#endif // CHRONOSYNC_DIFF_STATE_HPP
