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

#ifndef CHRONOSYNC_DIFF_STATE_CONTAINER_HPP
#define CHRONOSYNC_DIFF_STATE_CONTAINER_HPP

#include "mi-tag.hpp"
#include "diff-state.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace chronosync {

namespace mi = boost::multi_index;


struct RoundCompare
{
  bool
  operator()(RoundNo round1, RoundNo round2) const
  {
    return round1 < round2;
  }
};




/**
 * @brief Container for differential states
 */
struct DiffStateContainer : public mi::multi_index_container<
  DiffStatePtr,
  mi::indexed_by<

    mi::ordered_unique<
      mi::tag<ordered_round>,
      mi::const_mem_fun<DiffState, uint64_t, &DiffState::getRound>,
      RoundCompare
      >
  >
>
{
};

} // namespace chronosync

#endif // CHRONOSYNC_DIFF_STATE_CONTAINER_HPP
