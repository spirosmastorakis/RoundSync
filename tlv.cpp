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
 * @author Yingdi Yu <yingdi@cs.ucla.edu>
 * @author Pedro de las Heras Quiros <pedro.delasheras@urjc.es>
 * @author Eva M. Castro <eva.castro@urjc.es>
 *
 */

#include "tlv.hpp"

namespace chronosync {
namespace tlv {

std::ostream &
operator<<(std::ostream &o, const DataType t) {
  switch (t) {
    case DataOnly: return o << "DataOny";   
    case CumulativeOnly: return o << "CumulativeOnly";   
    case DataAndCumulative: return o << "DataAndCumulative";   
    case StateLeaf: return o << "StateLeaf";   
    case SeqNo: return o << "SeqNo";   
    case RoundNo: return o << "RoundNo";   
    case State: return o << "State";   
    case CumulativeInfo: return o << "CumulativeInfo";   
    case RecoveryData: return o << "RecoveryData";   
    default: return o<<"(invalid value)"; 
  }
}

} // namespace tlv
} // namespace chronosync

