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
 * @author Pedro de las Heras Quiros <pedro.delasheras@urjc.es>
 * @author Eva M. Castro <eva.castro@urjc.es>
 */

#ifndef CHRONOSYNC_DATA_CONTENT_HPP
#define CHRONOSYNC_DATA_CONTENT_HPP

#include "diff-state.hpp"

namespace chronosync {

class DataContent;
typedef shared_ptr<DataContent> DataContentPtr;
typedef shared_ptr<const DataContent> ConstDataContentPtr;


class DataContent
{
public:
  class Error : public std::runtime_error
  {
  public:
    explicit
    Error(const std::string& what)
      : std::runtime_error(what)
    {
    }
  };


  DataContent(const Name& userPrefix = Name(""),  
              RoundNo roundNo = 0,
              ndn::ConstBufferPtr cumulativeDigest = NULL,
              DiffStatePtr statePtr = NULL);

  StatePtr 
  getState(){
    return m_statePtr;
  }

RoundNo
getRoundNo(){
  return m_roundNo;
}

ndn::ConstBufferPtr 
getCumulativeDigest(){
  return m_cumulativeDigest;
}

Name
getUserPrefix(){
  return m_userPrefix;
}

  /**
   * @brief Encode to a wire format
   */
  const Block&
  wireEncode() const;

  /**
   * @brief Decode from the wire format
   */
  void
  wireDecode(const Block& wire);

  /**
   * @brief A well formed SyncReply must have either state ||
   * (userPrefix+roundNo+cumulativeDigest)+[state]
   */
  bool 
  wellFormed();


  tlv::DataType
  getDataType(){
    return m_dataType;
  };


protected:
  mutable Block m_wire;

  template<bool T>
  size_t
  wireEncode(ndn::EncodingImpl<T>& block) const;


private:
  ndn::Name m_userPrefix;
  RoundNo m_roundNo; // The round of m_cumulativeDigest
  ndn::ConstBufferPtr m_cumulativeDigest; // The cumulative digest of m_round
  DiffStatePtr m_statePtr;
  tlv::DataType m_dataType;
};

} // chronosync

#endif // CHRONOSYNC_DATA_CONTENT_HPP
