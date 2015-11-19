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

#include "logic.hpp"
#include "logger.hpp"

#include "data-content.hpp"
#include "reco-data.hpp"

#include <chrono>

INIT_LOGGER("Logic")

#ifdef _DEBUG
#define _LOG_DEBUG_ID(v) _LOG_DEBUG("Instance" << m_instanceId << ": " << v)
#else
#define _LOG_DEBUG_ID(v) _LOG_DEBUG(v)
#endif

namespace chronosync {

using ndn::ConstBufferPtr;
using ndn::EventId;

const uint8_t EMPTY_DIGEST_VALUE[] = {
  0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
  0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
  0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
  0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

#ifdef _DEBUG
int Logic::m_instanceCounter = 0;


#endif

  bool partitioned = false;

void
startPartition() {
  std::cerr << ">> startPartition" << std::endl;
  partitioned=true;
}

void
stopPartition() {
  std::cerr << ">> stopPartition" << std::endl;
  partitioned=false;
}



int syncTimeouts = 0;


const ndn::Name Logic::DEFAULT_NAME;
const ndn::Name Logic::EMPTY_NAME;
const ndn::shared_ptr<ndn::Validator> Logic::DEFAULT_VALIDATOR;
const time::milliseconds Logic::DEFAULT_DATA_INTEREST_LIFETIME(1000);
const time::milliseconds Logic::DEFAULT_SYNC_INTEREST_LIFETIME(1000);
const time::milliseconds Logic::DEFAULT_DATA_FRESHNESS(1000);

const ndn::ConstBufferPtr Logic::EMPTY_DIGEST(new ndn::Buffer(EMPTY_DIGEST_VALUE, 32));

// Name components: DATA, SYNC, RECO
const ndn::name::Component Logic::DATA_INTEREST_COMPONENT("DATA");
const ndn::name::Component Logic::SYNC_INTEREST_COMPONENT("SYNC");
const ndn::name::Component Logic::RECO_INTEREST_COMPONENT("RECO");

// Delay to send Sync Interest (with the round digest), once 
// a Data for a round is received. 
const time::milliseconds Logic::DEFAULT_ROUND_DIGEST_DELAY(1000);

// Delay to stabilize cumulative digests from the m_stableRound to
// m_stabilizingRound
const time::milliseconds Logic::DEFAULT_STABILIZE_CUMULATIVE_DIGEST_DELAY(5*DEFAULT_ROUND_DIGEST_DELAY);

// Maximum number of rounds to move requesting Data Interest.
// Recovery will be requested.
const uint64_t Logic::MAX_ROUNDS_WITHOUT_RECOVERY (10);

// Maximum number of rounds to move requesting Data Interest.
// Recovery will be requested.
const uint64_t Logic::BACK_UNSTABLE_ROUNDS (5);

// If received a cumulative digest of a not stable round, wait 
// to checkRecovery in the future
const time::milliseconds Logic::DEFAULT_RETRY_CHECK_RECOVERY_DELAY (2000);

// Schedule sending a CumulativeOnly  with our cumulative
// digest to inform others. If in the meanwhile another node has
// sent it, cancel 
const int Logic::DEFAULT_DELAY_SENDING_CUMULATIVE_ONLY(1000);

// CumulativeOnlyData are stored in DiffLog using SeqNo=0
const SeqNo Logic::CUMULATIVE_ONLY_DATA(0);

// When a node does not produce, sends CumulativeOnly to inform others.
// A node waits MAX_DATA_INTEREST_TO_CUMULATIVE_ONLY Data Timeouts 
// at m_currentRound to send CumulativeOnly
const int Logic::MAX_DATA_INTEREST_TO_CUMULATIVE_ONLY(5);

// Maximum number of retries to send Data Interest
const int Logic::MAX_DATA_INTEREST_TIMEOUTS(5);

// Maximum number of retries to send Reco Interest
const int Logic::MAX_RECO_INTEREST_TIMEOUTS(5);

Logic::Logic(ndn::Face& face,
             const Name& syncPrefix,
             const Name& defaultUserPrefix,
             const UpdateCallback& onUpdate,
             const Name& defaultSigningId,
             ndn::shared_ptr<ndn::Validator> validator,
             const time::milliseconds& dataInterestLifetime,
             const time::milliseconds& syncInterestLifetime,
             const time::milliseconds& dataFreshness)
  : m_face(face)
  , m_syncPrefix(syncPrefix)
  , m_defaultUserPrefix(defaultUserPrefix)
  , m_outstandingDataInterestId(0)
  , m_pendingDataInterest(NULL)
  , m_currentRound(1)
  , m_stabilizingRound(1)
  , m_stableRound(0)
  , m_lastRecoveryRound(0)
  , m_recoveryDesired(false)
  , m_onUpdate(onUpdate)
  , m_scheduler(m_face.getIoService())
  , m_randomGenerator(static_cast<unsigned int>(std::time(0)))
  , m_rangeUniformRandom(m_randomGenerator, boost::uniform_int<>(100,500))
  , m_reexpressionJitter(m_randomGenerator, boost::uniform_int<>(100,500))
  , m_cumulativeOnlyRandom(m_randomGenerator, boost::uniform_int<>(0,DEFAULT_DELAY_SENDING_CUMULATIVE_ONLY))
  , m_dataInterestLifetime(dataInterestLifetime)
  , m_syncInterestLifetime(syncInterestLifetime)
  , m_dataFreshness(dataFreshness)
  , m_defaultSigningId(defaultSigningId)
  , m_validator(validator)
  , m_numberDataInterestTimeouts(0)
  , m_numberRecoInterestTimeouts(0)
{


#ifndef _DEBUG 
  std::cerr << "START TIME: " << std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1)  << \
    " " << std::endl;
#endif

#ifdef _DEBUG
  
  m_instanceId = m_instanceCounter++;
#endif

  _LOG_DEBUG_ID(">> Logic::Logic");
  _LOG_DEBUG_ID(">> Logic::NEW");

  m_state.reset();
  m_log.clear();

  m_oldState.reset();

  m_sessionName = m_defaultUserPrefix;
  m_sessionName.appendNumber(ndn::time::toUnixTimestamp(ndn::time::system_clock::now()).count());
  m_seqNo = 0;


  _LOG_DEBUG_ID("    Listen multicast prefix: " << m_syncPrefix);
  m_dataRegisteredPrefixId =
    m_face.setInterestFilter(m_syncPrefix,
                             bind(&Logic::onDataAndSyncInterest, this, _1, _2),
                             bind(&Logic::onDataRegisterFailed, this, _1, _2));


  m_recoPrefix = m_defaultUserPrefix;
  m_recoPrefix.append(RECO_INTEREST_COMPONENT);
  _LOG_DEBUG_ID("    Listen reco prefix: " << m_recoPrefix);
  m_recoRegisteredPrefixId =
    m_face.setInterestFilter(m_recoPrefix,
                             bind(&Logic::onRecoInterest, this, _1, _2),
                             bind(&Logic::onRecoRegisterFailed, this, _1, _2));

  m_outstandingDataInterestId = 0;

  m_reexpressingDataInterestId =
    m_scheduler.scheduleEvent(ndn::time::seconds(0),
                              bind(&Logic::sendDataInterest, this, m_currentRound, 1));
  
  m_stabilizingCumulativeDigest =
    m_scheduler.scheduleEvent(DEFAULT_STABILIZE_CUMULATIVE_DIGEST_DELAY,
                              bind(&Logic::setStableState, this));


  if (m_defaultUserPrefix == "/ndn/edu/c/c" || m_defaultUserPrefix == "/ndn/edu/e/e") {
    _LOG_DEBUG_ID("    fault injection activated from 25s to 45s");
    m_scheduler.scheduleEvent(ndn::time::seconds(15),
			      bind(&startPartition));
    
    m_scheduler.scheduleEvent(ndn::time::seconds(40),
			      bind(&stopPartition));
  }  

  
  _LOG_DEBUG_ID("<< Logic::Logic");
}

Logic::~Logic()
{
  m_scheduler.cancelAllEvents();
  m_face.shutdown();
}



const Name&
Logic::getSessionName(Name prefix)
{
  return m_sessionName;
}

const SeqNo&
Logic::getSeqNo(Name prefix)
{
  return m_seqNo;
}

void
Logic::updateSeqNo(const SeqNo& seqNo, const Name &updatePrefix)
{
  Name prefix = m_defaultUserPrefix;

  std::cerr << std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1)  <<  \
    " " << std::endl;

  _LOG_DEBUG_ID(">> Logic::updateSeqNo");
  _LOG_DEBUG_ID("    seqNo: " << seqNo << " m_seqNo: " << m_seqNo);
  if (seqNo < m_seqNo || seqNo == 0)
    return;
  
  m_seqNo = seqNo;
  _LOG_DEBUG_ID("    updateSeqNo: m_seqNo " << m_seqNo);
  
  bool isInserted = false;
  bool isUpdated = false;
  SeqNo oldSeq;
  boost::tie(isInserted, isUpdated, oldSeq) = m_state.update(m_sessionName,
                                                             m_seqNo);

  _LOG_DEBUG_ID("    Insert: " << std::boolalpha << isInserted);
  _LOG_DEBUG_ID("    Updated: " << std::boolalpha << isUpdated);
  if (isInserted || isUpdated) {
    DiffStatePtr commit = make_shared<DiffState>();
    commit->update(m_sessionName, m_seqNo);

    if (m_stableRound != 0) {
      DiffStateContainer::iterator stateIter = m_log.find(m_stableRound);
      if (stateIter != m_log.end())
	commit->setCumulativeInfo(make_shared<CumulativeInfo>(make_pair(m_stableRound, (*stateIter)->getCumulativeDigest())));
    }

    updateDiffLog(commit, m_currentRound);
    if (m_pendingDataInterest && (m_pendingDataInterest->getName().get(-1).toNumber()==m_currentRound)){
      _LOG_DEBUG_ID("    have to send Data to m_pendingDataInterest");
      sendData(prefix, m_pendingDataInterest->getName(), commit);
      m_pendingDataInterest = NULL;
    }
    else
      _LOG_DEBUG_ID("    don't have to send SyncData to m_pendingDataInterest");

    // Send round digest so everybody knows we have produced new data
    m_scheduler.scheduleEvent(ndn::time::seconds(0),
			      bind(&Logic::sendSyncInterest, this, m_currentRound));
    
    moveToNewCurrentRound(m_currentRound + 1);


    syncTimeouts = 0;


#ifdef _DEBUG    
    printRoundLog();
#endif
  _LOG_DEBUG_ID("<< Logic::updateSeqNo");

  }

}
  
ConstBufferPtr
Logic::getRootDigest() const
{
  return m_state.getDigest();
}

void
Logic::printState(std::ostream& os) const
{
  BOOST_FOREACH(ConstLeafPtr leaf, m_state.getLeaves())
    {
      os << *leaf << "\n";
    }
}


void
Logic::printState(std::ostream& os, State state) const
{
  BOOST_FOREACH(ConstLeafPtr leaf, state.getLeaves())
    {
      os << *leaf << "\n";
    }
}



void
Logic::onDataAndSyncInterest(const Name& prefix, const Interest& interest)
{
  _LOG_DEBUG_ID(">> Logic::onDataAndSyncInterest");
  Name name = interest.getName();
  _LOG_DEBUG_ID("    name PREFIX: " << name.getPrefix(5));

  if (partitioned) {
    _LOG_DEBUG_ID("    Partitioned: dropping Interest");
    return;
  }

  if (DATA_INTEREST_COMPONENT == name.get(-2)) {
    // data interest: includes roundNo
    processDataInterest(interest.shared_from_this());
  }
  else if (SYNC_INTEREST_COMPONENT == name.get(-3)){
    // sync interest: includes roundNo and round digest
    processSyncInterest(interest.shared_from_this());
  } else {
     std::cerr << "    Logic::onDataAndSyncInterest:: ERROR: Unknown component!";
     std::cerr << "    Logic::onDataAndSyncInterest:: NAME" << name;
  }


  _LOG_DEBUG_ID("<< Logic::onDataAndSyncInterest");
}

void
Logic::onDataRegisterFailed(const Name& prefix, const std::string& msg)
{
  //Data prefix registration failed
  _LOG_DEBUG_ID(">> Logic::onDataRegisterFailed");
}

void
Logic::onData(const Interest& interest, Data& data)
{
  _LOG_DEBUG_ID(">> Logic::onData");
  _LOG_DEBUG_ID("    name " << interest.getName());


  if (partitioned) {
    _LOG_DEBUG_ID("    Partitioned: dropping Data");
    return;
  }

  if (static_cast<bool>(m_validator))
    m_validator->validate(data,
                          bind(&Logic::onDataValidated, this, _1),
                          bind(&Logic::onDataValidationFailed, this, _1));
  else
    onDataValidated(data.shared_from_this());
  _LOG_DEBUG_ID("<< Logic::onData");
}


void
Logic::onSyncData(const Interest& interest, Data& data)
{
  _LOG_DEBUG_ID(">> Logic::onSyncData");
  _LOG_DEBUG_ID("    Sync Reply NOT IMPLEMENTED!");
  _LOG_DEBUG_ID("<< Logic::onSyncData");
}

void
Logic::onRecoData(const Interest& interest, Data& data)
{
  _LOG_DEBUG_ID(">> Logic::onRecoData");
  _LOG_DEBUG_ID("    name " << interest.getName());


  if (partitioned) {
    _LOG_DEBUG_ID("    Partitioned: dropping Data");
    return;
  }

  if (static_cast<bool>(m_validator)) {
    m_validator->validate(data,
                          bind(&Logic::onRecoDataValidated, this, _1),
                          bind(&Logic::onRecoDataValidationFailed, this, _1));
  }
  else {
    onRecoDataValidated(data.shared_from_this());
  }


  _LOG_DEBUG_ID("<< Logic::onRecoData");
}


void
Logic::onRecoInterest(const Name& prefix, const Interest& interest)
{
  _LOG_DEBUG_ID(">> Logic::onRecoInterest");

  Name name = interest.getName();
  _LOG_DEBUG_ID("    name: " << name);


  if (partitioned) {
    _LOG_DEBUG_ID("    Partitioned: dropping Interest");
    return;
  }

  if (RECO_INTEREST_COMPONENT == name.get(-1)){
    processRecoInterest(interest.shared_from_this());    
  }

  _LOG_DEBUG_ID("<< Logic::onRecoInterest");

}

void
Logic::onRecoRegisterFailed(const Name& prefix, const std::string& msg)
{
  //Reco prefix registration failed
  _LOG_DEBUG_ID(">> Logic::onRecoRegisterFailed");
}



void
Logic::onRecoInterestTimeout(const Interest& interest)
{
  _LOG_DEBUG_ID(">> Logic::onRecoInterestTimeout");
  _LOG_DEBUG_ID("    Interest: " << interest.getName());

  Name nodePrefix = interest.getName().getPrefix(-1);
  m_numberRecoInterestTimeouts++;
  if (m_numberRecoInterestTimeouts >= MAX_RECO_INTEREST_TIMEOUTS) { 
    std::cerr << "onRecoInterestTimeout:: Max rec timeouts to " <<
               nodePrefix ;
    m_numberRecoInterestTimeouts = 0;
    _LOG_DEBUG_ID("    Removing from m_pendingRecoveryPrefixes: " << 
                  nodePrefix);
    m_pendingRecoveryPrefixes.erase(nodePrefix);

  } else {
    _LOG_DEBUG_ID("    Program another send Reco Interest to " << 
                  nodePrefix);

     m_scheduler.scheduleEvent
   	   (ndn::time::seconds(0),
	    bind(&Logic::sendRecoInterest, this, nodePrefix));
  }
 
  _LOG_DEBUG_ID("<< Logic::onRecoInterestTimeout");

}

void
Logic::onSyncInterestTimeout(const Interest& interest)
{
  //_LOG_DEBUG_ID(">> Logic::onSyncInterestTimeout");
  //_LOG_DEBUG_ID("<< Logic::onSyncInterestTimeout");
}

void
Logic::onDataInterestTimeout(const Interest& interest, unsigned retries)
{
  _LOG_DEBUG_ID(">> Logic::onDataInterestTimeout");

  RoundNo roundNo = interest.getName().get(-1).toNumber();
  _LOG_DEBUG_ID("    RoundNo: " << roundNo);

  if (roundNo == m_currentRound) {
    if (syncTimeouts >= 0)
      ++syncTimeouts;
    if (syncTimeouts > 20){
      printRoundLog();
      syncTimeouts = -1;
    }
  }


  m_numberDataInterestTimeouts++;
  if (m_numberDataInterestTimeouts >= MAX_DATA_INTEREST_TO_CUMULATIVE_ONLY && 
      m_stableRound == m_currentRound-1) {
    _LOG_DEBUG_ID("    Program to send my cumulative digest for the round=" 
                  << m_stableRound);
    m_numberDataInterestTimeouts = 0;
    
    ndn::ConstBufferPtr myCumulativeDigest = m_state.getDigest();
    
    ndn::EventId eventId =   
      m_scheduler.scheduleEvent 
      (ndn::time::milliseconds(m_cumulativeOnlyRandom()),
       bind(&Logic::produceCumulativeOnly, 
	    this, 
	    m_stableRound, 
	    myCumulativeDigest));
    
    m_cumulativeDigestToEventId.insert
      (std::pair<ndn::Buffer, 
       ndn::EventId>(*myCumulativeDigest, eventId));
  }
  
  // Retry if interest is being sent in a round < m_currentRound.
  DiffStateContainer::iterator stateIter = m_log.find(roundNo);
  if (roundNo < m_currentRound 
      && stateIter == m_log.end()
      && retries < MAX_DATA_INTEREST_TIMEOUTS) {
    m_scheduler.scheduleEvent(ndn::time::seconds(0),
			      bind(&Logic::sendDataInterest, this, roundNo, ++retries));
  }
  
  _LOG_DEBUG_ID("<< Logic::onDataInterestTimeout");
}

void
Logic::onDataValidationFailed(const shared_ptr<const Data>& data)
{
  // Data cannot be validated.
  _LOG_DEBUG_ID(">> Logic::onDataValidationFailed");
}


void
Logic::onRecoDataValidationFailed(const shared_ptr<const Data>& data)
{
  // RecoData cannot be validated.
  _LOG_DEBUG_ID(">> Logic::onRecoDataValidationFailed");
}



void
Logic::onDataValidated(const shared_ptr<const Data>& data)
{
  //_LOG_DEBUG_ID(">> Logic::onDataValidated");

  Name name = data->getFullName();

  if (DATA_INTEREST_COMPONENT == name.get(-3)) {
    processData(name, data->getContent().blockFromValue());
  }
  else
     std::cerr << ">>>    Logic::onDataValidated:: ERROR: DATA_INTEREST_COMPONENT != name.get(-3)";

 // _LOG_DEBUG_ID("<< Logic::onDataValidated");
}

void
Logic::onRecoDataValidated(const shared_ptr<const Data>& data)
{
  //_LOG_DEBUG_ID(">> Logic::onRecoDataValidated");
  Name name = data->getFullName();

  if (RECO_INTEREST_COMPONENT == name.get(-2)){
    processRecoData(name, data->getContent().blockFromValue());;
  }
  else
     std::cerr << ">>>    Logic::onRecoDataValidated:: ERROR: RECO_INTEREST_COMPONENT != name.get(-2)";
  //_LOG_DEBUG_ID("<< Logic::onRecoDataValidated");
}


void
Logic::processDataInterest(const shared_ptr<const Interest>& interest)
{
  _LOG_DEBUG_ID(">> Logic::processDataInterest");
  const Name& name = interest->getName();

  _LOG_DEBUG_ID("    InterestName: " << name );

  RoundNo roundNo =
    name.get(-1).toNumber();

  _LOG_DEBUG_ID("    roundNo: " << roundNo);
  _LOG_DEBUG_ID("    m_currentRound: " << m_currentRound);

#ifndef _DEBUG
  std::cerr << ">>> Received Data Interest round" << roundNo << std::endl;
#endif

  if (roundNo >= m_currentRound) {
    _LOG_DEBUG_ID("    roundNo >= m_currentRound, so let's record m_pendingDataInterest ");
    m_pendingDataInterest = make_shared<Interest>(*interest);
  }

  if (roundNo > m_currentRound) {
    // We move to the latest known round as soon as we know about its existence
    moveToNewCurrentRound(roundNo);
  }
  else if (roundNo < m_currentRound)  {
    // If we have something for that round, send it to him. 
    DiffStateContainer::iterator stateIter = m_log.find(roundNo);
    if (stateIter != m_log.end()) {

      // We only send data produced by this node. Either DataOnly,
      // DataAndCumulative, or CumulativeOnly. Other data produced in
      // this round by other nodes will be on caches or will be
      // retrieved from their producers
      bool isCumulativeOnly;
      DiffStatePtr diffState = (*stateIter)->getStateFrom(m_sessionName, isCumulativeOnly);
      if (diffState != NULL) {
        _LOG_DEBUG_ID("    We have something for requested round");
#ifdef _DEBUG      
        printState(std::cerr, *(diffState->getState()));
#endif

	CumulativeInfoPtr cumulativeInfo = diffState->getCumulativeInfo();
        if (isCumulativeOnly){
          // CumulativeOnly entry in diffState
          if (cumulativeInfo) {
            _LOG_DEBUG_ID("    Help others with CumulativeInfo stored in my diffState");
            sendCumulativeOnly(name, cumulativeInfo->first, cumulativeInfo->second);
          }
          else
            throw Error("Can't find cumulative in dataForCumulativeOnly");
        }
        else { 
          sendData(m_defaultUserPrefix, name, diffState);
	}
      }
      else 
        _LOG_DEBUG_ID("    We have NOTHING for requested round");
    }
  }
  
  _LOG_DEBUG_ID("<< Logic::processDataInterest");
}


void 
Logic::moveToNewCurrentRound (RoundNo newCurrentRound)
{
  _LOG_DEBUG_ID(">> Logic::moveToNewCurrentRound");

  if(newCurrentRound - m_currentRound <= MAX_ROUNDS_WITHOUT_RECOVERY){
    // Data has been produced in rounds m_currentRound
    // .. newCurrentRound, so let's fetch it
    for (auto i = m_currentRound; i<newCurrentRound; i++){
      EventId eventId =
        m_scheduler.scheduleEvent(ndn::time::seconds(0),
                                  bind(&Logic::sendDataInterest, this, i, 1));
    }
  }
  else{
    // Too far away, don't fish. Once a data with different cumulative
    // is retrieved, Recovery will be launched


    std::cerr << "Jump to a far away from " 
              <<  m_currentRound
              << "to " 
              << newCurrentRound
              << ", so DON't fish, await recovery" 
              << std::endl;

    m_recoveryDesired = true;
  }  

  
  // Move to newCurrentRound
  _LOG_DEBUG_ID("   moving from round m_currentRound: " << m_currentRound << " to " << newCurrentRound);
  m_currentRound = newCurrentRound;

  m_numberDataInterestTimeouts = 0;

  // Proceed to fish in m_currentRound
  m_scheduler.cancelEvent(m_reexpressingDataInterestId);
  m_reexpressingDataInterestId =
    m_scheduler.scheduleEvent(ndn::time::seconds(0),
                              bind(&Logic::sendDataInterest, this, m_currentRound, 1));

  _LOG_DEBUG_ID("<< Logic::moveToNewCurrentRound");  
}

void 
Logic::moveToNewCurrentRoundAfterRecovery (RoundNo newCurrentRound)
{
  _LOG_DEBUG_ID(">> Logic::moveToNewCurrentRoundAfterRecovery");

  // Move to newCurrentRound
  _LOG_DEBUG_ID("    moving from round m_currentRound: " << m_currentRound << " to " << newCurrentRound);
  m_currentRound = newCurrentRound;


  m_scheduler.cancelEvent(m_reexpressingDataInterestId);
  m_reexpressingDataInterestId =
    m_scheduler.scheduleEvent(ndn::time::seconds(0),
                              bind(&Logic::sendDataInterest, this, m_currentRound, 1));

  _LOG_DEBUG_ID("<< Logic::moveToNewCurrentRoundAfterRecovery");  

}


void 
Logic::setStableState ()
{

  _LOG_DEBUG_ID(">> Logic::setStableState");  

  RoundNo initRound;

  if (m_stableRound == 0 && m_lastRecoveryRound == 0) {
      // First stabilization from the beginning of time
      initRound = 1;

  } else if  (m_stabilizingRound == m_lastRecoveryRound) {
      // First stabilization after receiving a RECO data (m_stableRound=0)
      // we need to calculate cumulative digest in round m_lastRecoveryRound
      initRound = m_stabilizingRound;

  } else if (m_stableRound != 0) {
      // Stabilization from the next to last stable round
      initRound = m_stableRound +1;
  }
  else 
    throw Error ("Unable to stabilize cumulative digests in Logic::setStableState.");

  // From initRound to m_stabilizingRound:
  // Add m_log changes to m_oldState and calculate cumulative digests for these rounds 
  calculateStableStateAndCumulativeDigests(initRound, m_stabilizingRound);

  
  m_stableRound = m_stabilizingRound;

  m_stabilizingRound = m_stableRound + (m_currentRound - m_stableRound)/2;

  _LOG_DEBUG_ID("    new stableRound      = " << m_stableRound);  
  _LOG_DEBUG_ID("    new stabilizingRound = " << m_stabilizingRound);  
  _LOG_DEBUG_ID("    current round: = " << m_currentRound);  
  
  m_stabilizingCumulativeDigest =
    m_scheduler.scheduleEvent(DEFAULT_STABILIZE_CUMULATIVE_DIGEST_DELAY,
                              bind(&Logic::setStableState, this));
  
  _LOG_DEBUG_ID("<< Logic::setStableState");  
}

void 
Logic::calculateStableStateAndCumulativeDigests (RoundNo initRound, RoundNo endRound)
{
  _LOG_DEBUG_ID(">> Logic::calculateStableStateAndCumulativeDigests");  

  DiffStateContainer::iterator stateIter = m_log.find(initRound);
  RoundNo r = initRound;
  while (stateIter == m_log.end() && r < endRound) {
    _LOG_DEBUG_ID("      We dont have anything for the first round, go next one"); 
    r++;
    stateIter = m_log.find(r);
  }

  while ((stateIter != m_log.end() &&
         (*stateIter)->getRound() < endRound)) {
    _LOG_DEBUG_ID("      Adding state of round = " << (*stateIter)->getRound()); 
    m_oldState += *((*stateIter)->getState());
    (*stateIter)->setCumulativeDigest(m_oldState.getDigest());
    ++stateIter;
  }

  stateIter = m_log.find(endRound);
  DiffStatePtr commit;
  if (stateIter == m_log.end()) {
    commit = make_shared<DiffState>();
    updateDiffLog(commit, endRound);
  }
  else{
    commit = *stateIter;
    m_oldState += *(commit->getState());
  }
  _LOG_DEBUG_ID("      Added stable state of round = " << endRound); 
  commit->setCumulativeDigest(m_oldState.getDigest());

  _LOG_DEBUG_ID("      Print stable state"); 
  printState(std::cerr, m_oldState);
  printDigest(commit->getCumulativeDigest(), "cumulative digest of m_oldState");

  _LOG_DEBUG_ID("<< Logic::calculateStableStateAndCumulativeDigests");  
}


bool
Logic::checkRoundDigests (RoundNo roundNo, const ConstBufferPtr roundDigest)
{
  _LOG_DEBUG_ID(">> Logic::checkRoundDigests");

  bool areEqual = false;


  // Someone is sending digest interests in a previous round

  DiffStateContainer::iterator stateIter = m_log.find(roundNo);
    
    
  if (stateIter != m_log.end()) {
    // We have data in round log for roundNo
    ConstBufferPtr rd = (*stateIter)->getRoundDigest();
    assert (rd);
    _LOG_DEBUG("    Comparing round digest in round=" << roundNo);
    printDigest(roundDigest, "received round digest");
    printDigest(rd, "my round digest");
    
    if (ndn::name::Component(roundDigest) != ndn::name::Component(rd)) {
      _LOG_DEBUG_ID("    != round digests for round " << roundNo << ", go FISHING");
      // Perhaps we are missing something in roundNo, so go fishing there
      m_scheduler.scheduleEvent(ndn::time::seconds(0),
                                bind(&Logic::sendDataInterest, this, roundNo, 1));
      
      // Send round digest in the future to inform of our final round digest
      // in this round
      _LOG_DEBUG_ID("    Program sending my Round Digest in round=" << roundNo);
      EventId eventId =
        m_scheduler.scheduleEvent(DEFAULT_ROUND_DIGEST_DELAY,
                                  bind(&Logic::sendSyncInterest, this, roundNo));
      m_scheduler.cancelEvent((*stateIter)->getReexpressingSyncInterestId());
      (*stateIter)->setReexpressingSyncInterestId(eventId);
    }
    else {
      _LOG_DEBUG_ID("    EQUAL Round Digests!");
      areEqual = true;
    }
  }
  else {
    // We don't have data in round log for roundNo
    _LOG_DEBUG_ID("    we have nothing for round " << roundNo << ", go FISHING");

    // It's sure that we are missing something in roundNo, so go fishing there
    m_scheduler.scheduleEvent(ndn::time::seconds(0),
                              bind(&Logic::sendDataInterest, this, roundNo, 1));
  }
  
  _LOG_DEBUG_ID("<< Logic::checkRoundDigests");

  return areEqual;
}

  


void
Logic::produceCumulativeOnly(RoundNo roundNo, ndn::ConstBufferPtr cumulativeDigest)
{
  // Send to pending interest, if it exists
  _LOG_DEBUG_ID(">> Logic::produceCumulativeOnly");
  ndn::Name name;
  if (m_pendingDataInterest && (m_pendingDataInterest->getName().get(-1).toNumber()==m_currentRound)){
    _LOG_DEBUG_ID("    have to send CumulativeOnly of round=" << roundNo << " to m_pendingDataInterest");

    name = m_pendingDataInterest->getName();
    sendCumulativeOnly(name, roundNo, cumulativeDigest);

    m_pendingDataInterest = NULL;
  }

  _LOG_DEBUG_ID("    store CumulativeOnly of round=" << roundNo << " in difflog");
  // Store in current round state
  DiffStatePtr commit = make_shared<DiffState>();
  commit->update(m_sessionName, CUMULATIVE_ONLY_DATA);

  commit->setCumulativeInfo(make_shared<CumulativeInfo>(make_pair(roundNo, cumulativeDigest)));

  updateDiffLog(commit, m_currentRound);

  // CumulativeOnly, as DataOnly and DataAndCumulative, *also*
  // consumes a round
  moveToNewCurrentRound(m_currentRound + 1);
  _LOG_DEBUG_ID("<< Logic::produceCumulativeOnly");

}

void
Logic::sendCumulativeOnly(ndn::Name name, RoundNo roundNo, ndn::ConstBufferPtr cumulativeDigest)
{
  _LOG_DEBUG_ID(">> Logic::sendCumulativeOnly");


  shared_ptr<Data> cumulativeOnlyData = make_shared<Data>(name);

  DataContent dataContent (m_sessionName, roundNo, cumulativeDigest);
  if (!dataContent.wellFormed())
    throw Error ("Malformed CumulativeOnly DataContent.");


  cumulativeOnlyData->setContent(dataContent.wireEncode());
  cumulativeOnlyData->setFreshnessPeriod(m_dataFreshness);


  if (m_defaultSigningId.empty())
    m_keyChain.sign(*cumulativeOnlyData);
  else
    m_keyChain.signByIdentity(*cumulativeOnlyData, m_defaultSigningId);
  
  // Update exclude filter in commit
  _LOG_DEBUG_ID("    Update exclude filter with: " << 
                cumulativeOnlyData->getFullName().get(-1));

  DiffStateContainer::iterator stateIter = m_log.find(roundNo);
  if (stateIter != m_log.end()) {
    (*stateIter)->appendExclude(cumulativeOnlyData->getFullName().get(-1));
  }


  m_face.put(*cumulativeOnlyData);

  // checking if our own interest got satisfied
  if (m_outstandingDataInterestName == name) {
    // remove outstanding interest
    if (m_outstandingDataInterestId != 0) {
      _LOG_DEBUG_ID("    remove pending interest");
      m_face.removePendingInterest(m_outstandingDataInterestId);
      m_outstandingDataInterestId = 0;
    }
  }

  _LOG_DEBUG_ID("<< Logic::sendCumulativeOnly");
}


void
Logic::sendRecoInterest(ndn::Name userPrefix){
  _LOG_DEBUG_ID(">> Logic::sendRecoInterest");


  if (partitioned) {
    _LOG_DEBUG_ID("    Partitioned: dropping Interest ");
    return;
  }


  Name interestName;
  interestName.append(userPrefix)
    .append(RECO_INTEREST_COMPONENT);

  Interest interest(interestName);
  interest.setMustBeFresh(true);
  interest.setInterestLifetime(m_syncInterestLifetime);

  m_face.expressInterest(interest,
                         bind(&Logic::onRecoData, this, _1, _2),
                         bind(&Logic::onRecoInterestTimeout, this, _1));
  

  
  _LOG_DEBUG_ID("    Send recovery interest: " << interest.getName());
  _LOG_DEBUG_ID("<< Logic::sendRecoInterest");
}

void
Logic::checkRecovery(ndn::Name userPrefix, 
                     RoundNo roundNoOfCumulativeDigest, 
                     ndn::ConstBufferPtr cumulativeDigest) {

  _LOG_DEBUG_ID(">> Logic::checkRecovery");
  _LOG_DEBUG_ID("    Cumulative sent from=" << userPrefix << " cumulative round=" << roundNoOfCumulativeDigest);


  // If this cumulative == the one we have scheduled to send in
  // m_cumulativeDigestToEventId, cancel it and delete entry

  std::map<ndn::Buffer, ndn::EventId>::iterator it =
    m_cumulativeDigestToEventId.find(*cumulativeDigest);
  if (it != m_cumulativeDigestToEventId.end()) {
    _LOG_DEBUG_ID("    Cancel event of sendng cumulative digest. I have received one that is equal to mine");
    ndn::EventId eventId = it->second;
    m_scheduler.cancelEvent(eventId);
    m_cumulativeDigestToEventId.erase(*cumulativeDigest);

    _LOG_DEBUG_ID("<< Logic::checkRecovery");
    return;
  }
  

  ndn::ConstBufferPtr myCumulativeDigest;
  bool doRecovery = true;
  
  // Check when doRecovery is not required
  if (roundNoOfCumulativeDigest < m_lastRecoveryRound || m_stableRound == 0) {
    // We don't have cumulative digests calculated to compare with the
    // one received
    _LOG_DEBUG_ID("    Received cumulative in round=" << roundNoOfCumulativeDigest << " my stableRound="  <<
                  m_stableRound << "  my_lasteRecoveryRound=" << m_lastRecoveryRound << " DO NOT RECOVERY");
    doRecovery = false;
  } 
  else if (roundNoOfCumulativeDigest <= m_stableRound) {
    // Compare with my cumulative for that round
    DiffStateContainer::iterator stateIter = m_log.find(roundNoOfCumulativeDigest);
    if (stateIter != m_log.end()){
      myCumulativeDigest = (*stateIter)->getCumulativeDigest();
      _LOG_DEBUG_ID("    Comparing cumulative digests");
      printDigest(cumulativeDigest, "received cumulative digest in Data");
      printDigest(myCumulativeDigest, "my cumulative digest");
      if (*cumulativeDigest == *myCumulativeDigest) {
        _LOG_DEBUG_ID("    Received cumulative in round =" << roundNoOfCumulativeDigest << ", my stableRound = "  <<
                  m_stableRound << ": same cumulative digest. DO NOT RECOVERY");
  	doRecovery = false;
      }
    } 
  } 
  else if (!m_recoveryDesired) {
    // round > stableRound, will compare with my cumulative in the future
    doRecovery = false;
    _LOG_DEBUG_ID("    Received cumulative in round=" 
		  << roundNoOfCumulativeDigest 
		  << " my stableRound="  
		  << m_stableRound 
		  << " program checkRecovery in the future to wait stabilization of that round. DO NOT RECOVERY");
    m_scheduler.scheduleEvent(DEFAULT_RETRY_CHECK_RECOVERY_DELAY,
    			      bind(&Logic::checkRecovery, this, userPrefix, roundNoOfCumulativeDigest, cumulativeDigest));
  }
  
  if (doRecovery) {
    // Send Recovery interest
    if (m_recoveryDesired)
      _LOG_DEBUG_ID("    DO RECOVERY. Program to send recovery interest, after a long partition");
    else
      _LOG_DEBUG_ID("    DO RECOVERY. Program to send recovery interest, different cumulative digest in round=" << roundNoOfCumulativeDigest);


    _LOG_DEBUG_ID("      m_currentRound      = " << m_currentRound);
    _LOG_DEBUG_ID("      m_stableRound       = " << m_stableRound);
    _LOG_DEBUG_ID("      m_lastRecoveryRound = " << m_lastRecoveryRound);
    _LOG_DEBUG_ID("      m_stabilizingRound  = " << m_stabilizingRound);

    std::set<ndn::Name>::iterator itPrefix = 
      m_pendingRecoveryPrefixes.find(userPrefix.getPrefix(-1));
    if (itPrefix == m_pendingRecoveryPrefixes.end()){
      m_scheduler.scheduleEvent
	(ndn::time::seconds(0),
	 bind(&Logic::sendRecoInterest, this, userPrefix.getPrefix(-1)));

      _LOG_DEBUG_ID("    inserting in m_pendingRecoveryPrefixes: " << userPrefix.getPrefix(-1));
      
      m_pendingRecoveryPrefixes.insert(userPrefix.getPrefix(-1));
    } else {
      _LOG_DEBUG_ID("    This prefix is already in m_pendingRecoveryPrefixes: " << userPrefix.getPrefix(-1));
    }
    
    m_recoveryDesired = false;
    
    // If we have a cumulative digest in roundNoOfCumulativeDigest,
    // schedule sending a CumulativeOnly  with our cumulative
    // digest to inform others. If in the meanwhile another node has
    // sent it, cancel (in processData)
    if (myCumulativeDigest) {
      _LOG_DEBUG_ID("    Program to send my cumulative digest for the round=" << roundNoOfCumulativeDigest);
      ndn::EventId eventId=   
	m_scheduler.scheduleEvent 
	(ndn::time::milliseconds(m_cumulativeOnlyRandom()),
	 bind(&Logic::produceCumulativeOnly, 
	      this, 
	      roundNoOfCumulativeDigest, 
	      myCumulativeDigest));
      
      m_cumulativeDigestToEventId.insert
  	(std::pair<ndn::Buffer, 
  	 ndn::EventId>(*myCumulativeDigest, eventId));
    }

  }      

  _LOG_DEBUG_ID("<< Logic::checkRecovery");
}



void
Logic::processSyncInterest(const shared_ptr<const Interest>& interest)
{
  _LOG_DEBUG_ID(">> Logic::processSyncInterest");
  const Name name = interest->getName();
  //_LOG_DEBUG_ID("  InterestName: " << name);

  RoundNo roundNo          = name.get(-2).toNumber();
  _LOG_DEBUG_ID("    roundNo: " << roundNo);

  ConstBufferPtr roundDigest =
    make_shared<ndn::Buffer>(name.get(-1).value(), 
                             name.get(-1).value_size());


  // We move to the latest known round as soon as we know about its existence
  if (roundNo >= m_currentRound) {
    moveToNewCurrentRound(roundNo + 1);
  }
  else if (roundNo <= m_lastRecoveryRound) {
    // roundNo <= m_lastRecoveryRound is too old, do nothing. 
    // We don't have reliable round digest after a recovery 
    _LOG_DEBUG_ID("    RoundNo= " << roundNo << " is less than m_lastRecoveryRound=" <<
                  m_lastRecoveryRound << " DON'T CHECK ROUND DIGEST");
  }
  else {
    checkRoundDigests(roundNo, roundDigest);
  }
  
  _LOG_DEBUG_ID("<< Logic::processSyncInterest");
}
  

void
Logic::processRecoInterest(const shared_ptr<const Interest>& interest)
{
  _LOG_DEBUG_ID(">> Logic::processRecoInterest");
  const Name name = interest->getName();
  _LOG_DEBUG_ID("    InterestName: " << name);


  std::cerr << ">>> Received Reco Interest " << interest->getName() << std::endl;

  sendRecoData(m_defaultUserPrefix, name);

  _LOG_DEBUG_ID("<< Logic::processRecoInterest");
}

  
void
Logic::processData(const Name& fullName,
		   const Block& dataContentBlock)
{
  _LOG_DEBUG_ID(">> Logic::processData");


  // round comes in latest component of name but fullName includes de implicit digest
  // (explicitly :-)
  RoundNo roundNo = fullName.get(-2).toNumber();

  if (roundNo == m_currentRound) 
    syncTimeouts = 0;

  _LOG_DEBUG_ID("    roundNo:" << roundNo);

  if (roundNo <= m_stableRound) {
    // This is a very old data Can't update m_log in rounds <=
    // m_stableRound So do nothing. In the future there will be a
    // recovery.
    _LOG_DEBUG_ID("    Very old round=" << roundNo << " minor than m_stableRound=" << m_stableRound <<
                  " Do nothing.");
    _LOG_DEBUG_ID("<< Logic::processData");
    return;
  }

  // Do we have already an entry in round log for this round? If not create new commit.
  DiffStatePtr commit;
  DiffStateContainer::iterator stateIter = m_log.find(roundNo);
  if (stateIter != m_log.end()) {
    _LOG_DEBUG_ID("    We already have something for that round so don't create new entry");
    commit = *stateIter;
  }
  else{
    commit = make_shared<DiffState>();
  }

  // Update exclude filter in commit
  _LOG_DEBUG_ID("    Update exclude filter with: " << fullName.get(-1));
  commit->appendExclude(fullName.get(-1));
  
  try {
    DataContent dataContent;
    dataContent.wireDecode(dataContentBlock);

    tlv::DataType dataType = dataContent.getDataType();

    _LOG_DEBUG_ID("    Received Data Type = " << dataType);

    if (dataType == tlv::CumulativeOnly) {
      // Add to commit [dataContent.getUserPrefix(), CUMULATIVE_ONLY_DATA] 
      commit->update(dataContent.getUserPrefix(), CUMULATIVE_ONLY_DATA);
      _LOG_DEBUG_ID("    Data from " << dataContent.getUserPrefix());
    }

    // Process cumulative digest
    if (dataType == tlv::CumulativeOnly ||
        dataType == tlv::DataAndCumulative) {
      ndn::Name           userPrefix = dataContent.getUserPrefix();
      RoundNo             roundNoOfCumulativeDigest = dataContent.getRoundNo();
      ndn::ConstBufferPtr cumulativeDigest = dataContent.getCumulativeDigest();

      checkRecovery(userPrefix, roundNoOfCumulativeDigest, cumulativeDigest);
    }



    if (dataType == tlv::DataOnly ||
        dataType == tlv::DataAndCumulative) {

      StatePtr reply = dataContent.getState();

      std::vector<MissingDataInfo> v;
      BOOST_FOREACH(ConstLeafPtr leaf, reply->getLeaves().get<ordered>())
        {
          BOOST_ASSERT(leaf != 0);
          
          const Name& info = leaf->getSessionName();
          SeqNo seq = leaf->getSeq();

	  _LOG_DEBUG_ID("    Received Data from " << info << " " << seq);
          
          bool isInserted = false;
          bool isUpdated = false;
          SeqNo oldSeq;


	  // If we are in the state of having received a recovery and
	  // not yet stabilized, apply received data to m_oldState
	  if (roundNo <= m_lastRecoveryRound && m_stableRound == 0)
	    m_oldState.update(info, seq);

          boost::tie(isInserted, isUpdated, oldSeq) = m_state.update(info, seq);
          if (isInserted || isUpdated) {
            oldSeq++;
            MissingDataInfo mdi = {info, oldSeq, seq};
            v.push_back(mdi);
          }
          // Either if we already knew this info or not, update the
          // round log entry
          commit->update(info, seq);

        }
      
      if (!v.empty()) {
        // call app's callback
        _LOG_DEBUG_ID("    call app's callback with new data");
        m_onUpdate(v);    
      }
      else 
        _LOG_DEBUG_ID("    don't call app's callback: nothing new");  
    }
    
    if (roundNo == m_currentRound)
      // we received data (either DataOnly, CumulativeOnly or
      // DataAndCumulative) for our round, so move to new round
      moveToNewCurrentRound(m_currentRound + 1);

    _LOG_DEBUG_ID("    update round log for round " << roundNo << " with received data");
    updateDiffLog(commit, roundNo);


    // New data received for roundNo <= m_stabilizingRound, so delay
    // the calculation of the cumulative digest of m_stabilizingRound
    // (i.e. don't stabilize it)
    if (roundNo <= m_stabilizingRound) {
      _LOG_DEBUG_ID("    Received data, delay stabilization");
      m_scheduler.cancelEvent(m_stabilizingCumulativeDigest);
      m_stabilizingCumulativeDigest =
        m_scheduler.scheduleEvent(DEFAULT_STABILIZE_CUMULATIVE_DIGEST_DELAY,
                                  bind(&Logic::setStableState, this));
    }



    // Send round digest in the future, so it covers everything we
    // have fished (either DataOnly, CumulativeOnly or
    // DataAndCumulative) in this round
    EventId eventId =
      m_scheduler.scheduleEvent(DEFAULT_ROUND_DIGEST_DELAY,
                                bind(&Logic::sendSyncInterest, this, roundNo));
    m_scheduler.cancelEvent(commit->getReexpressingSyncInterestId());
    commit->setReexpressingSyncInterestId(eventId);
    

#ifdef _DEBUG    
    printRoundLog();
#endif

  }
  catch (State::Error&) {
    _LOG_DEBUG_ID("    Something really fishy happened during state decoding");
    // Something really fishy happened during state decoding;
    commit.reset();
    return;
  }
}
  
void
Logic::processRecoData(const Name& fullName,
                       const Block& recoBlock)
{
  _LOG_DEBUG_ID(">> Logic::processRecoData");

  m_pendingRecoveryPrefixes.erase(fullName.getPrefix(-2));
  _LOG_DEBUG_ID("    Removing from m_pendingRecoveryPrefixes: " << fullName.getPrefix(-2));

  try {
    RecoData recoData;
    recoData.wireDecode(recoBlock);

    tlv::DataType dataType = recoData.getDataType();

    if (dataType != tlv::RecoveryData)
      throw new Error("");

    RoundNo roundNoOfState = recoData.getRoundNo();

    StatePtr receivedState = recoData.getState();

    std::vector<MissingDataInfo> v;
    BOOST_FOREACH(ConstLeafPtr leaf, receivedState->getLeaves().get<ordered>())
      {
        BOOST_ASSERT(leaf != 0);
        
        const Name& info = leaf->getSessionName();
        SeqNo seq = leaf->getSeq();
        
        bool isInserted = false;
        bool isUpdated = false;
        SeqNo oldSeq;

        boost::tie(isInserted, isUpdated, oldSeq) = m_state.update(info, seq);
        if (isInserted || isUpdated) {
          oldSeq++;
          MissingDataInfo mdi = {info, oldSeq, seq};
          v.push_back(mdi);
        }
      }
      
      if (!v.empty()) {
        // call app's callback
        _LOG_DEBUG_ID("    call app's callback with new data");
        printState(std::cerr, m_state);	
        m_onUpdate(v);    
      }
      else 
        _LOG_DEBUG_ID("    don't call app's callback: nothing new");  

      // We have received a recovery data, we have to wait some
      // time to stabilize a new cd
      if (roundNoOfState >= m_currentRound) {
        // RecoveryRound is the roundNo received, 
        // because it is the last round we know there have been produced data
        m_lastRecoveryRound = roundNoOfState;
        // ... and move current to next one, to request new data
        // data has already been produced in roundNoState
        moveToNewCurrentRoundAfterRecovery(roundNoOfState + 1);
      }
      else {
        // RecoveryRound is the mcurrentRound -1, 
        // because it is the last round we know there have been produced data
        m_lastRecoveryRound = m_currentRound - 1;
      }

      _LOG_DEBUG_ID("    Update lastRecoveryRound = " << m_lastRecoveryRound);  

      // Proceed to fish in m_currentRound and some previous rounds
      RoundNo initRound;
      if (m_currentRound <= BACK_UNSTABLE_ROUNDS) {
          initRound = 1;
      } else {
          initRound = m_currentRound-BACK_UNSTABLE_ROUNDS;
      }

      _LOG_DEBUG_ID("    BACK unstable rouds -> sendDataInterest from: " << initRound  
                    << " to: " << m_currentRound);
      for (auto i = initRound; i< m_currentRound; i++){
          EventId eventId =
            m_scheduler.scheduleEvent(ndn::time::seconds(0),
                                      bind(&Logic::sendDataInterest, this, i, 1));
      }


      // the recovery data invalidates current stabilizing
      // round. After recovery, we begin to stabilize the round to
      // which we have applied the reco data
      m_stabilizingRound = m_lastRecoveryRound;

      // we don't have a stable round after a recovery
      m_stableRound = 0;
      // But we want to remember the state in the moment of recovery
      m_oldState.reset();
      m_oldState += m_state;


      // Reschedule calculation of stable state
      m_scheduler.cancelEvent(m_stabilizingCumulativeDigest);
      m_stabilizingCumulativeDigest =
      m_scheduler.scheduleEvent(DEFAULT_STABILIZE_CUMULATIVE_DIGEST_DELAY,
				    bind(&Logic::setStableState, this));
      
  }

  catch (State::Error&) {
    _LOG_DEBUG_ID("    Something really fishy happened during state decoding");
    // Something really fishy happened during state decoding;
    return;
  }

}


void
Logic::printRoundLog()
{
  _LOG_DEBUG_ID(">> Logic::printRoundLog");
  _LOG_DEBUG_ID("    m_state: ");
  printState(std::cerr, m_state);

  _LOG_DEBUG_ID("    round log: ");
  std::cerr << std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1)  <<  \
    " " << std::endl;

  std::cerr << std::endl << std::endl << std::endl;
  std::cerr << "=======================================" << std::endl;


  DiffStateContainer::iterator stateIter = m_log.begin();

  while ((stateIter != m_log.end() &&
         (*stateIter)->getRound() < m_currentRound)) {
    std::cerr << "    round: " << (*stateIter)->getRound() << std::endl;
    printDigest((*stateIter)->getCumulativeDigest(), "cd");
    printDigest((*stateIter)->getRoundDigest(), "rd");
    printState(std::cerr, *(*stateIter)->getState());
    ++stateIter;
  }


  std::cerr << "=======================================" << std::endl;

  _LOG_DEBUG_ID("<< Logic::printRoundLog");
}

 

void
Logic::updateDiffLog(DiffStatePtr commit, const RoundNo round)
{

  RoundNo roundNo = round;

  _LOG_DEBUG_ID(">> Logic::updateDiffLog");
  _LOG_DEBUG_ID("    roundNo: " << roundNo);

  _LOG_DEBUG_ID("    commit: ");
#ifdef _DEBUG_
  printState(std::cerr, *commit);
#endif

  // Set round, round digest
  commit->setRound(roundNo);
  commit->updateRoundDigest();

  DiffStateContainer::iterator stateIter = m_log.find(roundNo);
  if (stateIter == m_log.end())
    m_log.insert(commit);

  _LOG_DEBUG_ID("     Round: " << commit->getRound());
  _LOG_DEBUG_ID("     Round Digest     : " << digestToStr(commit->getRoundDigest()));

  _LOG_DEBUG_ID("<< Logic::updateDiffLog");
}


void
Logic::sendDataInterest(RoundNo roundNo, unsigned retries)
{
  _LOG_DEBUG_ID(">> Logic::sendDataInterest for round " << roundNo);


  if (partitioned) {
    _LOG_DEBUG_ID("   Partitioned: dropping Interest ");
    return;
  }


  Name interestName;
  interestName.append(m_syncPrefix)
    .append(DATA_INTEREST_COMPONENT)
    .append(ndn::name::Component::fromNumber(roundNo));

  Interest interest(interestName);
  //  interest.setMustBeFresh(true);
  interest.setMustBeFresh(false);
  interest.setInterestLifetime(m_dataInterestLifetime);
  

  // Add exclude filter if stored in round log
  DiffStateContainer::iterator stateIter = m_log.find(roundNo);
  if (stateIter != m_log.end())
    interest.setExclude((*stateIter)->getExcludeFilter());

  
  const ndn::PendingInterestId* pid =
    m_face.expressInterest (interest,
                            bind(&Logic::onData, this, _1, _2),
                            bind(&Logic::onDataInterestTimeout, this, _1, retries));

  // register info about outstanding interest of current round, so it
  // can be removed from face in case our application produces data
  // and our own interest is satisfied
  if (roundNo == m_currentRound) {
    m_outstandingDataInterestName = interestName;
    m_outstandingDataInterestId = pid;
  }


  if (roundNo == m_currentRound) {
    // Program periodic sending of Data Interest
    EventId eventId =
      m_scheduler.scheduleEvent(m_dataInterestLifetime +
                                ndn::time::milliseconds(m_reexpressionJitter()),
                                bind(&Logic::sendDataInterest, this, roundNo, 1));
    m_scheduler.cancelEvent(m_reexpressingDataInterestId);
    m_reexpressingDataInterestId = eventId;
  }


  _LOG_DEBUG_ID("<< Logic::sendDataInterest");
}
  

void
Logic::sendSyncInterest(RoundNo roundNo)
{
  _LOG_DEBUG_ID(">> Logic::sendSyncInterest for round " << roundNo);

  if (partitioned) {
    _LOG_DEBUG_ID("    Partitioned: dropping Interest ");
    return;
  }


  Name interestName;
  interestName.append(m_syncPrefix)
    .append(SYNC_INTEREST_COMPONENT)
    .append(ndn::name::Component::fromNumber(roundNo));

  // Append round digest 
  DiffStateContainer::iterator stateIter = m_log.find(roundNo);

  if (stateIter != m_log.end()) 
    interestName.append(ndn::name::Component((*stateIter)->getRoundDigest()));
  else {
    _LOG_DEBUG_ID("    we don't have an entry for that round, so add EMPTY round digest");
    interestName.append(ndn::name::Component(EMPTY_DIGEST));
  }
  
  _LOG_DEBUG_ID("    name: " << interestName);
  
  // Only digest interest for (current_round - 1) is resent periodically
  //if (roundNo == m_currentRound - 1) {
  //  m_outstandingSyncInterestName = interestName;

  //  EventId eventId =
  //    m_scheduler.scheduleEvent(m_syncInterestLifetime +
  //                              ndn::time::milliseconds(m_reexpressionJitter()),
  //                              bind(&Logic::sendSyncInterest, this, roundNo));
  //  m_scheduler.cancelEvent(m_reexpressingSyncInterestId);
  //  m_reexpressingSyncInterestId = eventId;
 // }

  Interest interest(interestName);
  interest.setMustBeFresh(true);
  interest.setInterestLifetime(m_syncInterestLifetime);

  //m_outstandingSyncInterestId = m_face.expressInterest(interest,
  //                                                       bind(&Logic::onSyncData, this, _1, _2),
  //                                                       bind(&Logic::onSyncInterestTimeout, this, _1));

  m_face.expressInterest(interest, bind(&Logic::onSyncData, this, _1, _2),
                         bind(&Logic::onSyncInterestTimeout, this, _1));

  
  _LOG_DEBUG_ID("    Send sync interest PREFIX: " << interest.getName().getPrefix(5));
  _LOG_DEBUG_ID("<< Logic::sendSyncInterest");
}




void
Logic::sendData(const Name& nodePrefix, 
                const Name& name,                
                DiffStatePtr diffState)
{
  _LOG_DEBUG_ID(">> Logic::sendData");
  _LOG_DEBUG_ID("    nodePrefix: " << nodePrefix);
  _LOG_DEBUG_ID("    name: " << name);

  shared_ptr<Data> data = make_shared<Data>(name);

  // Add cumulative info to data packet, if it exists
  ConstBufferPtr cd;
  RoundNo roundNo = 0;
  CumulativeInfoPtr cumulativeInfo = diffState->getCumulativeInfo();
  if (cumulativeInfo) {
    cd = cumulativeInfo->second;
    roundNo = cumulativeInfo->first;
    printDigest(cd, "  Adding cumulative digest of round " + std::to_string(roundNo));
  }

  DataContent dataContent (m_sessionName, roundNo, cd, diffState);
  if (!dataContent.wellFormed())
    throw Error ("Logic::sendData:: Malformed DataContent.");

  data->setContent(dataContent.wireEncode());

  data->setFreshnessPeriod(m_dataFreshness);


  if (m_defaultSigningId.empty())
    m_keyChain.sign(*data);
  else
    m_keyChain.signByIdentity(*data, m_defaultSigningId);
  
  // Update exclude filter in commit
  _LOG_DEBUG_ID("    Update exclude filter with: " << 
                data->getFullName().get(-1));
  diffState->appendExclude(data->getFullName().get(-1));


  m_face.put(*data);


  // checking if our own interest got satisfied
  if (m_outstandingDataInterestName == name) {
    // remove outstanding interest
    if (m_outstandingDataInterestId != 0) {
      _LOG_DEBUG_ID("    remove pending interest");
      m_face.removePendingInterest(m_outstandingDataInterestId);
      m_outstandingDataInterestId = 0;
    }
  }

  _LOG_DEBUG_ID("<< Logic::sendData");
}

void
Logic::sendRecoData(const Name& nodePrefix, 
                    const Name& name)             
{
  _LOG_DEBUG_ID(">> Logic::sendRecoData");
  _LOG_DEBUG_ID("    nodePrefix: " << nodePrefix);
  _LOG_DEBUG_ID("    name: " << name);

  shared_ptr<Data> recoData = make_shared<Data>(name);

  // Send latest state corresponding to  m_currentRound - 1
  DiffStatePtr state = make_shared<DiffState>();
  *state += m_state;
  RecoData sr (m_currentRound - 1, state);

  recoData->setContent(sr.wireEncode());
  recoData->setFreshnessPeriod(m_dataFreshness);

  if (m_defaultSigningId.empty())
    m_keyChain.sign(*recoData);
  else
    m_keyChain.signByIdentity(*recoData, m_defaultSigningId);
  
  m_face.put(*recoData);

  _LOG_DEBUG_ID("<< Logic::sendRecoData");
}


void
Logic::printDigest(ndn::ConstBufferPtr digest, std::string name)
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

std::string
Logic::digestToStr(ndn::ConstBufferPtr digest)
{
  using namespace CryptoPP;

  std::string hash;
  StringSource(digest->buf(), digest->size(), true,
               new HexEncoder(new StringSink(hash), false));
  return hash;
}

} // namespace chronosync
