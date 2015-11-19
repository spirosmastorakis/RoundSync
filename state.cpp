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

#include "state.hpp"

namespace chronosync {

using boost::make_tuple;


State::~State()
{
}


SeqNo
State::getSeqNo(const Name& info) {
  LeafContainer::iterator leaf = m_leaves.find(info);

  if (leaf == m_leaves.end()) {
    return 0;
  }
  else {
    return (*leaf)->getSeq(); 
  }
}


/**
 * @brief Add or update leaf to the sync tree
 *
 * @param info session name of the leaf
 * @param seq  sequence number of the leaf
 * @return 3-tuple (isInserted, isUpdated, oldSeqNo)
 */

boost::tuple<bool, bool, SeqNo>
State::update(const Name& info, const SeqNo& seq)
{
  m_wire.reset();

  LeafContainer::iterator leaf = m_leaves.find(info);

  if (leaf == m_leaves.end()) {
    m_leaves.insert(make_shared<Leaf>(info, cref(seq)));
    return make_tuple(true, false, 0);
  }
  else {
    if ((*leaf)->getSeq() == seq || seq < (*leaf)->getSeq()) {
      return make_tuple(false, false, 0);
    }

    SeqNo old = (*leaf)->getSeq();
    m_leaves.modify(leaf,
                    [=] (LeafPtr& leaf) { leaf->setSeq(seq); } );
    return make_tuple(false, true, old);
  }
}

ndn::ConstBufferPtr
State::getDigest() const
{

  m_digest.reset();
  BOOST_FOREACH (ConstLeafPtr leaf, m_leaves.get<ordered>())
    {
      BOOST_ASSERT(leaf != 0);
      m_digest.update(leaf->getDigest()->buf(), leaf->getDigest()->size());
    }

  return m_digest.computeDigest();
}



void
State::reset()
{
  m_leaves.clear();
}

State&
State::operator+=(const State& state)
{
  BOOST_FOREACH (ConstLeafPtr leaf, state.getLeaves())
    {
      BOOST_ASSERT(leaf != 0);
      update(leaf->getSessionName(), leaf->getSeq());
    }

  return *this;
}

template<bool T>
size_t
State::wireEncode(ndn::EncodingImpl<T>& block) const
{
  size_t totalLength = 0;

  BOOST_REVERSE_FOREACH (ConstLeafPtr leaf, m_leaves.get<ordered>())
    {
      size_t entryLength = 0;
      entryLength += prependNonNegativeIntegerBlock(block, tlv::SeqNo, leaf->getSeq());
      entryLength += leaf->getSessionName().wireEncode(block);
      entryLength += block.prependVarNumber(entryLength);
      entryLength += block.prependVarNumber(tlv::StateLeaf);
      totalLength += entryLength;
    }

  return totalLength;
}

template size_t
State::wireEncode<true>(ndn::EncodingImpl<true>& block) const;

template size_t
State::wireEncode<false>(ndn::EncodingImpl<false>& block) const;

const Block&
State::wireEncode() const
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
State::wireDecode(const Block& wire)
{
  if (!wire.hasWire())
    throw Error("The supplied block does not contain wire format");

  if (wire.type() != tlv::State)
    throw Error("Unexpected TLV type when decoding State: " +
                boost::lexical_cast<std::string>(m_wire.type()));


  wire.parse();
  m_wire = wire;

  for (Block::element_const_iterator it = wire.elements_begin();
       it != wire.elements_end(); it++) {
    if (it->type() == tlv::StateLeaf) {
      it->parse();

      Block::element_const_iterator val = it->elements_begin();
      Name info(*val);
      val++;

      if (val != it->elements_end())
        update(info, readNonNegativeInteger(*val));
      else
        throw Error("No seqNo when decoding SyncReply");
    }
  }
}

} // namespace chronosync
