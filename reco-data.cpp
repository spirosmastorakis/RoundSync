/* -*- Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2015 University of California, Los Angeles
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

#include "tlv.hpp"

#include "reco-data.hpp"


namespace chronosync {

RecoData::RecoData()
{
}

RecoData::RecoData(RoundNo roundNo, DiffStatePtr statePtr)
  :m_roundNo (roundNo)
  ,m_statePtr (statePtr)
{
}
  


template<bool T>
size_t
RecoData::wireEncode(ndn::EncodingImpl<T>& block) const
{
  size_t totalLength = 0;

  // encode state encoding if it exits
  if (m_statePtr){
    totalLength += m_statePtr->wireEncode(block);
    totalLength += block.prependVarNumber(totalLength);
    totalLength += block.prependVarNumber(tlv::State);
    //    std::cerr << "ENCODED state" << std::endl;
  }


  // encode roundNo
  totalLength += prependNonNegativeIntegerBlock(block, tlv::RoundNo, m_roundNo);
  

  // encode length 
  totalLength += block.prependVarNumber(totalLength);

  // encode type
  totalLength += block.prependVarNumber(tlv::RecoveryData);

  return totalLength;
}

template size_t
RecoData::wireEncode<true>(ndn::EncodingImpl<true>& block) const;

template size_t
RecoData::wireEncode<false>(ndn::EncodingImpl<false>& block) const;


const Block&
RecoData::wireEncode() const
{
  if (m_wire.hasWire())
    return m_wire;

  ndn::EncodingEstimator estimator;
  size_t estimatedSize = wireEncode(estimator);

  ndn::EncodingBuffer buffer(estimatedSize, 0);
  wireEncode(buffer);

  m_wire = buffer.block();
  return m_wire;
}


void
RecoData::wireDecode(const Block& wire)
{
  if (!wire.hasWire())
    throw Error("The supplied block does not contain wire format");

  m_dataType = tlv::DataType(wire.type());

  if (m_dataType != tlv::RecoveryData)
    throw Error("Unexpected TLV type when decoding RecoData: " +
                boost::lexical_cast<std::string>(m_wire.type()));


  m_wire = wire;
  m_wire.parse();

  // Decode cumulative info, if it exits
  Block::element_const_iterator it = m_wire.elements_begin();

  // Decode roundNo
  m_roundNo = readNonNegativeInteger(*it);
  it++;

  // Decode state
  m_statePtr = make_shared<DiffState>();
  m_statePtr->wireDecode(*it);


}
  


} // namespace chronosync
