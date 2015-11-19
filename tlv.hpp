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
 */

#ifndef CHRONOSYNC_TLV_HPP
#define CHRONOSYNC_TLV_HPP

#include <iostream>
#include <string>

namespace chronosync {
namespace tlv {

/**
 * @brief Type value of sync reply related TLVs
 * @sa docs/design.rst
 */
enum DataType {
  DataOnly           = 128, // 0x80
  CumulativeOnly     = 129, // 0x81
  DataAndCumulative  = 130, // 0x82
  StateLeaf          = 131, // 0x83
  SeqNo              = 132, // 0x84
  RoundNo            = 133, // 0x85
  State              = 134, // 0x86
  CumulativeInfo     = 135, // 0x87
  RecoveryData       = 136  // 0x88
};

std::ostream & operator<<(std::ostream &o, const DataType t);

} // namespace tlv
} // namespace chronosync

#endif // CHRONOSYNC_TLV_HPP
