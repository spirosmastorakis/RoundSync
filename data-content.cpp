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

#include "tlv.hpp"

#include "data-content.hpp"


namespace chronosync {

void
pD(ndn::ConstBufferPtr digest, std::string name)
{
  using namespace CryptoPP;

  if (digest){
    std::string hash;
    StringSource(digest->buf(), digest->size(), true,
                 new HexEncoder(new StringSink(hash), false));    
    std::cerr << "  " << name << ": " << hash << std::endl;
  }
  else
    std::cerr << "  " << name << ": " << "NULL" << std::endl;
}




DataContent::DataContent(const Name& userPrefix,   
		         RoundNo roundNo,
		         ndn::ConstBufferPtr cumulativeDigest,
		         DiffStatePtr statePtr)
  :m_userPrefix (userPrefix)
  ,m_roundNo (roundNo)
  ,m_cumulativeDigest (cumulativeDigest)
  ,m_statePtr (statePtr)
{
}
  




template<bool T>
size_t
DataContent::wireEncode(ndn::EncodingImpl<T>& block) const
{
  size_t totalLength = 0;

  // encode state encoding if it exits
  if (m_statePtr){
    totalLength += m_statePtr->wireEncode(block);
    totalLength += block.prependVarNumber(totalLength);
    totalLength += block.prependVarNumber(tlv::State);
  }

  // encode prefix|roundNo|cumulativeDigest if they exist
  if (m_cumulativeDigest){
    size_t length = 0;
    // encode cumulative digest
    length +=  ndn::name::Component(m_cumulativeDigest).wireEncode(block);
    
    // encode roundNo
    length += prependNonNegativeIntegerBlock(block, tlv::RoundNo, m_roundNo);
    
    // encode user prefix 
    length += m_userPrefix.wireEncode(block);

    length += block.prependVarNumber(length);
    length += block.prependVarNumber(tlv::CumulativeInfo);

    totalLength += length;

  }

  totalLength += block.prependVarNumber(totalLength);

  tlv::DataType kind = tlv::DataAndCumulative;
  if (!(m_statePtr && m_cumulativeDigest)) {
    if (m_statePtr) {
      kind = tlv::DataOnly;
    }
    else 
      if (m_cumulativeDigest)
	kind = tlv::CumulativeOnly;
  }
  
  totalLength += block.prependVarNumber(kind);

  return totalLength;
}

template size_t
DataContent::wireEncode<true>(ndn::EncodingImpl<true>& block) const;

template size_t
DataContent::wireEncode<false>(ndn::EncodingImpl<false>& block) const;


const Block&
DataContent::wireEncode() const
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
DataContent::wireDecode(const Block& wire)
{
  if (!wire.hasWire())
    throw Error("The supplied block does not contain wire format");

  m_dataType = tlv::DataType(wire.type());

  if (m_dataType != tlv::DataAndCumulative &&
      m_dataType != tlv::DataOnly &&
      m_dataType != tlv::CumulativeOnly)
    throw Error("Unexpected TLV type when decoding DataContent: " +
                boost::lexical_cast<std::string>(m_wire.type()));


  m_wire = wire;
  m_wire.parse();

  // Decode cumulative info, if it exits
  Block::element_const_iterator it = m_wire.elements_begin();

  if (it != m_wire.elements_end() && it->type() == tlv::CumulativeInfo) {
    it->parse();

    // Decode userPrefix
    Block::element_const_iterator it1 = it->elements_begin();
    m_userPrefix.wireDecode(*it1);
    it1++;
    
    // Decode roundNo
    m_roundNo = readNonNegativeInteger(*it1);
    it1++;
    
    // Decode cumulative digest
    name::Component cd (*it1);
    m_cumulativeDigest =
      make_shared<ndn::Buffer>(cd.value(), 
                               cd.value_size());
    
    it++;
  }
  

  // Decode state, if it exists
  if (it != m_wire.elements_end() && it->type() == tlv::State) {
    m_statePtr = make_shared<DiffState>();
    m_statePtr->wireDecode(*it);
  }

}
  
bool 
DataContent::wellFormed()
  {
    return ((m_userPrefix != Name("") && m_cumulativeDigest != NULL) ||  
	    (m_roundNo == 0 && m_cumulativeDigest == NULL && m_statePtr != NULL));
  }


} // namespace chronosync
