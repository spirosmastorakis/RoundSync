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

#ifndef CHRONOSYNC_LOGIC_HPP
#define CHRONOSYNC_LOGIC_HPP

#include "boost-header.h"
#include <memory>
#include <unordered_map>

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator.hpp>

//#include "interest-table.hpp"
#include "diff-state-container.hpp"

namespace chronosync {

/**
 * @brief The missing sequence numbers for a session
 *
 * This class is used to notify the clients of Logic
 * the details of state changes.
 *
 * Instances of this class is usually used as elements of some containers
 * such as std::vector, thus it is copyable.
 */
class NodeInfo {
public:
  Name userPrefix;
  Name signingId;
  Name sessionName;
  SeqNo seqNo;
};

class MissingDataInfo
{
public:
  /// @brief session name
  Name session;
  /// @brief the lowest one of missing sequence numbers
  SeqNo low;
  /// @brief the highest one of missing sequence numbers
  SeqNo high;
};

/**
 * @brief The callback function to handle state updates
 *
 * The parameter is a set of MissingDataInfo, of which each corresponds to
 * a session that has changed its state.
 */
typedef function<void(const std::vector<MissingDataInfo>&)> UpdateCallback;

/**
 * @brief Logic of ChronoSync
 */
class Logic : noncopyable
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

public:
  static const time::milliseconds DEFAULT_DATA_INTEREST_LIFETIME;
  static const time::milliseconds DEFAULT_SYNC_INTEREST_LIFETIME;
  static const time::milliseconds DEFAULT_DATA_FRESHNESS;

  static const time::milliseconds DEFAULT_ROUND_DIGEST_DELAY;
  static const time::milliseconds DEFAULT_STABILIZE_CUMULATIVE_DIGEST_DELAY;

  static const uint64_t MAX_ROUNDS_WITHOUT_RECOVERY;
  static const uint64_t ROUNDS_STABILIZING_BEHIND_CURRENT;
  static const uint64_t BACK_UNSTABLE_ROUNDS;

  static const time::milliseconds DEFAULT_RETRY_CHECK_RECOVERY_DELAY;

  static const int DEFAULT_DELAY_SENDING_CUMULATIVE_ONLY;

  static const SeqNo CUMULATIVE_ONLY_DATA;

  static const int MAX_DATA_INTEREST_TO_CUMULATIVE_ONLY;

  static const int MAX_DATA_INTEREST_TIMEOUTS;
  static const int MAX_RECO_INTEREST_TIMEOUTS;

  /**
   * @brief Constructor
   *
   * @param face The face used to communication, will be shutdown in destructor
   * @param syncPrefix The prefix of the sync group
   * @param defaultUserPrefix The prefix of the first user added to this session
   * @param onUpdate The callback function to handle state updates
   * @param defaultSigningId The signing Id of the default user
   * @param validator The validator for packet validation
   * @param dataInterestLifetime The Lifetime of data interest
   * @param syncInterestLifetime The Lifetime of sync interest
   * @param dataFreshness The FreshnessPeriod of data
   */
  Logic(ndn::Face& face,
        const Name& syncPrefix,
        const Name& defaultUserPrefix,
        const UpdateCallback& onUpdate,
        const Name& defaultSigningId = DEFAULT_NAME,
        ndn::shared_ptr<ndn::Validator> validator = DEFAULT_VALIDATOR,
        const time::milliseconds& dataInterestLifetime = DEFAULT_DATA_INTEREST_LIFETIME,
        const time::milliseconds& syncInterestLifetime = DEFAULT_SYNC_INTEREST_LIFETIME,
        const time::milliseconds& dataFreshness = DEFAULT_DATA_FRESHNESS);

  ~Logic();



  /// @brief Get the name of default user.
  const Name&
  getDefaultUserPrefix() const
  {
    return m_defaultUserPrefix;
  }



  /**
   * @brief Get the name of the local session.
   *
   * This method gets the session name according to prefix, if prefix is not specified,
   * it returns the session name of default user. The sessionName is the user prefix
   * plus a timestamp.
   *
   * @param prefix prefix of the node
   */
  const Name&
  getSessionName(Name prefix = EMPTY_NAME);



  /**
   * @brief Get current seqNo of the local session.
   *
   * This method gets the seqNo according to prefix, if prefix is not specified,
   * it returns the seqNo of default user.
   *
   * @param prefix prefix of the node
   */
  const SeqNo&
  getSeqNo(Name prefix = EMPTY_NAME);



  /**
   * @brief Update the seqNo of the local session
   *
   * The method updates the existing seqNo with the supplied seqNo and prefix.
   *
   * @param seq The new seqNo.
   * @param updatePrefix The prefix of node to update.
   */
  void
  updateSeqNo(const SeqNo& seq, const Name& updatePrefix = EMPTY_NAME);



  /// @brief Get root digest of current sync tree
  ndn::ConstBufferPtr
  getRootDigest() const;




CHRONOSYNC_PUBLIC_WITH_TESTS_ELSE_PRIVATE:
  void
  printState(std::ostream& os) const;


  void
  printState(std::ostream& os, State state) const;


  ndn::Scheduler&
  getScheduler()
  {
    return m_scheduler;
  }

  State&
  getState()
  {
    return m_state;
  }


private:
  /**
   * @brief Callback to handle Data/Sync Interest
   *
   * This method checks whether an incoming interest is a Data or Sync Interest 
   * and dispatches the incoming interest to corresponding processing methods.
   *
   * @param prefix   The prefix of the sync group.
   * @param interest The incoming sync interest.
   */
  void
  onDataAndSyncInterest(const Name& prefix, const Interest& interest);



  /**
   * @brief Callback to handle Reco Interest
   *
   * This method checks whether an incoming interest is reco interest
   * and dispatches the incoming interest to corresponding processing method.
   *
   * @param prefix   The prefix of the reco prefix.
   * @param interest The incoming reco interest.
   */
  void
  onRecoInterest(const Name& prefix, const Interest& interest);


  /**
   * @brief Callback to handle Data prefix registration failure
   *
   * This method does nothing for now.
   *
   * @param prefix The prefix of the sync group.
   * @param msg    The error message.
   */
  void
  onDataRegisterFailed(const Name& prefix, const std::string& msg);



  /**
   * @brief Callback to handle Reco prefix registration failure
   *
   * This method does nothing for now.
   *
   * @param prefix The reco prefix. 
   * @param msg    The error message.
   */
  void
  onRecoRegisterFailed(const Name& prefix, const std::string& msg);



  /**
   * @brief Callback to handle Data 
   *
   * This method calls validator to validate Data.
   * For now, validation is disabled, Logic::onDataValidated is called
   * directly.
   *
   * @param interest The Data Interest
   * @param data     The reply to the Data Interest
   */
  void
  onData(const Interest& interest, Data& data);


  /**
   * @brief Callback to handle Sync Data 
   *
   * This method does nothing. Not implemented replys to SyncData
   *
   * @param interest The Sync Interest
   * @param data     The reply to the Sync Interest
   */
  void
  onSyncData(const Interest& interest, Data& data);



  /**
   * @brief Callback to handle Reco Reply
   *
   * This method calls validator to validate Reco Reply.
   * For now, validation is disabled, Logic::onRecoDataValidated is called
   * directly.
   *
   * @param interest The Reco Interest
   * @param data     The reply to the Reco Interest
   */
  void
  onRecoData(const Interest& interest, Data& data);



  /**
   * @brief Callback to handle Data Interest timeout.
   *
   * This method sends a Data Interest
   *
   * @param interest The Data Interest
   */
  void
  onDataInterestTimeout(const Interest& interest, unsigned retries);


  /**
   * @brief Callback to handle Reco Interest timeout.
   *
   * This method does nothing
   *
   * @param interest The Reco Interest
   */
  void
  onRecoInterestTimeout(const Interest& interest);

  
  /**
   * @brief Callback to handle Sync Interest timeout.
   *
   * This method does nothing
   *
   * @param interest The Sync Interest
   */
  void
  onSyncInterestTimeout(const Interest& interest);


  /**
   * @brief Callback to invalid Data
   *
   * This method does nothing but drops the invalid data.
   *
   * @param data The invalid Data
   */
  void
  onDataValidationFailed(const shared_ptr<const Data>& data);


  /**
   * @brief Callback to invalid Reco Reply.
   *
   * This method does nothing but drops the invalid reply.
   *
   * @param data The invalid Reco Reply
   */
  void
  onRecoDataValidationFailed(const shared_ptr<const Data>& data);


  /**
   * @brief Callback to valid Data
   *
   * This method simply passes the valid reply to processData.
   *
   * @param data The valid Data
   */
  void
  onDataValidated(const shared_ptr<const Data>& data);


  /**
   * @brief Callback to valid Reco Reply.
   *
   * This method simply passes the valid reply to processRecoData.
   *
   * @param data The valid Reco Reply.
   */
  void
  onRecoDataValidated(const shared_ptr<const Data>& data);


  /**
   * @brief Process Data Interest
   *
   * This method extracts the round number from the incoming Data Interest,
   * and check if we have data for this round.
   *
   * @param interest          The incoming interest
   */
  void
  processDataInterest(const shared_ptr<const Interest>& interest);


  /**
   * @brief Updates m_currentRound to newCurrentRound and sends Data Interests
   *                     
   * This method sends a new Data Interest from m_currentRound to the
   * newCurrentRound in order to get all produced data in these rounds.
   * Updates m_currentRound to newCurrentRound.
   *
   * @param newCurrentRound      The new current round
   *
   */
  void 
  moveToNewCurrentRound (RoundNo newCurrentRound);
  

  /**
   * @brief Updates m_currentRound to newCurrentRound 
   *                     
   * This method sends a Data Interest in newCurrentRound and
   * updates m_currentRound to newCurrentRound
   *
   * @param newCurrentRound      The new current round
   *
   */
  void 
  moveToNewCurrentRoundAfterRecovery (RoundNo newCurrentRound);


  /**
   * @brief  Called periodically, updates m_stableRound if possible
   *                     
   *
   *
   */
  void 
  setStableState ();


  /**
   * @brief              Updates m_oldState: it will contain new stable state
   *                     Updates all cumulativeDigests from initRound to m_stabilizingRound
   *
   * From initRound to endRound, add m_log to m_oldState and calculates 
   * cumulativeDigests for these rounds
   *
   */
  void
  calculateStableStateAndCumulativeDigests (RoundNo initRound,  RoundNo endRound);


  /**
   * @brief              Performs fishing in roundNo if roundDigest != the one 
   *                     in rounds log
   *
   *
   * @param roundNo      The round # where to check if fishing needed 
   *
   * @param roundDigest  round digest of remote peer for roundNo
   *
   * @return true if equal round digests, false otherwise
   */
  bool
  checkRoundDigests (RoundNo roundNo, const ndn::ConstBufferPtr roundDigest);


  /**
   * @brief produce Data which content is only the cumulative digest in a round. Update diffLog.
   *        
   *
   * @param roundNo      	The round number
   *
   * @param cumulativeDigest	The cumulative digest for roundNo
   *
   */
  void
  produceCumulativeOnly(RoundNo roundNo, ndn::ConstBufferPtr cumulativeDigest);


  /**
   * @brief send Data which content is only the cumulative digest in a round.
   *
   *
   * @param name         	The name of the data that will contain the cumulativeOnly
   *
   * @param roundNo      	The round number
   *
   * @param cumulativeDigest	The cumulative digest for roundNo
   *
   */
  void
  sendCumulativeOnly(ndn::Name name, RoundNo roundNo, ndn::ConstBufferPtr cumulativeDigest);


  /**
   * @brief compare the received cumulative digest with local one to check if recovery is required. 
   *
   *
   * @param userPrefix         	The userPrefix which sent the cumulativeDigest
   *
   * @param roundNo      	The received round number
   *
   * @param cumulativeDigest	The received cumulative digest for roundNo
   *
   */
  void
  checkRecovery(ndn::Name userPrefix, 
                RoundNo roundNoOfCumulativeDigest, 
                ndn::ConstBufferPtr cumulativeDigest);


  /**
   * @brief Send Recovery Interest to a userPrefix
   *
   *
   * @param userPrefix         	The Recovery Interest will be sent to userPrefix
   *
   */
 
  void
  sendRecoInterest(ndn::Name userPrefix);


 /**
   * @brief Process Sync Interest
   *
   * This method extracts the digest from the incoming Sync Interest,
   * compares it against current local digest, and process the Sync
   * Interest according to the comparison result.  
   *
   * @param interest          The incoming interest
   */
  
  void
  processSyncInterest(const shared_ptr<const Interest>& interest);


  /**
   * @brief Process Recovery Interest.
   *
   * This method sends a Reco Data.
   *
   * @param interest          The incoming interest.
   */
  void
  processRecoInterest(const shared_ptr<const Interest>& interest);


  /**
   * @brief Process Data.
   *
   * This method extracts state update information from Data and applies
   * it to the state and re-express Data Interest.
   *
   * @param fullName    The full data name of Data, including implicit digest
   *
   * @param dataBlock The content of the Data
   *
   */
  void
  processData(const Name& fullName,
              const Block& dataContentBlock);


  /**
   * @brief Process Reco Data.
   *
   * This method extracts state update information from Reco Data and applies
   * it to the state. If roundNo of Reco Data is greater than m_currenRound, then
   * move to this roundNo and update m_currentRound.
   *
   * @param fullName    The full data name of Data, including implicit digest
   *
   * @param dataBlock The content of the Data
   *
   */
 
  void
  processRecoData(const Name& fullName,
                  const Block& recoBlock);


  /**
   * @brief Insert state diff into log
   *
   * @param diff         The diff.
   * @param roundNo      The round to update / insert 
   */
  void
  updateDiffLog(DiffStatePtr commit, const RoundNo roundNo);


  /**
   * @brief Dumps Round Log
   *
   */
  void
  printRoundLog();


  /**
   * @brief Method to send Data Interest
   *
   * @param roundNo   The round that this Data Interest is sent in
   */
  void
  sendDataInterest(RoundNo roundNo, unsigned retries = 1);


   /**
   * @brief Method to send Sync Interest
   *
   * @param roundNo   The Sync Interest includes the round digest for the roundNo.
   *
   */
  void
  sendSyncInterest(RoundNo roundNo);

  
  /// @brief Helper method to send Data 
  void
  sendData(const Name& nodePrefix, 
               const Name& name, 
               DiffStatePtr diffState);


  /// @brief Helper method to send Reco Data
  void
  sendRecoData(const Name& nodePrefix, 
               const Name& name);

  
  void
  printDigest(ndn::ConstBufferPtr digest, std::string name = "digest");

  std::string
  digestToStr(ndn::ConstBufferPtr digest);

public:
  static const ndn::Name DEFAULT_NAME;
  static const ndn::Name EMPTY_NAME;
  static const ndn::shared_ptr<ndn::Validator> DEFAULT_VALIDATOR;

private:
  typedef std::unordered_map<ndn::Name, NodeInfo> NodeList;

  static const ndn::ConstBufferPtr EMPTY_DIGEST;

  // name components
  static const ndn::name::Component DATA_INTEREST_COMPONENT;
  static const ndn::name::Component SYNC_INTEREST_COMPONENT;
  static const ndn::name::Component RECO_INTEREST_COMPONENT;

  // Communication
  ndn::Face& m_face;
  Name m_syncPrefix;
  const ndn::RegisteredPrefixId* m_dataRegisteredPrefixId;
  const ndn::RegisteredPrefixId* m_recoRegisteredPrefixId;
  Name m_defaultUserPrefix;


  // State
  Name m_sessionName;
  SeqNo m_seqNo;

  // Reco prefix
  Name m_recoPrefix;

  // current state
  State m_state;

  // stable stable or candidate to stable state
  State m_oldState;

  DiffStateContainer m_log;
  //InterestTable m_interestTable;
  Name m_outstandingDataInterestName;
  //Name m_outstandingSyncInterestName;
  const ndn::PendingInterestId* m_outstandingDataInterestId;
  //const ndn::PendingInterestId* m_outstandingSyncInterestId;
  shared_ptr<const Interest> m_pendingDataInterest;

  // The greatest round in which we are waiting Data
  RoundNo m_currentRound;  

  // Candidate round to be stable. 
  // It will be stable if no Data for rounds <= m_stabilizingRoung are received 
  // during the period : DEFAULT_STABILIZE_CUMULATIVE_DIGEST_DELAY
  RoundNo m_stabilizingRound;   

  // Any round <= m_stableRound has cumulative digest and should no receive
  // any data because these rounds are old. If received it would provoke a recovery
  RoundNo m_stableRound;	

  // Last round in which we received a Recovery Data
  // Any round <= m_lastRecoveryRound could contain erroneous cumulative digests.
  RoundNo m_lastRecoveryRound;  

  // If a Data Interest is received for a round >>> my_currentRound, recovery will
  // be required. 
  bool m_recoveryDesired;


  // Map that contain cumulativeDigest and the programmed event to send this cumulativeDigest.
  std::map<ndn::Buffer, ndn::EventId> m_cumulativeDigestToEventId;

  // Callback
  UpdateCallback m_onUpdate;

  // Event
  ndn::Scheduler m_scheduler;
  ndn::EventId m_reexpressingDataInterestId;
  ndn::EventId m_reexpressingSyncInterestId;
  ndn::EventId m_stabilizingCumulativeDigest;

  // Timer
  boost::mt19937 m_randomGenerator;
  boost::variate_generator<boost::mt19937&, boost::uniform_int<> > m_rangeUniformRandom;
  boost::variate_generator<boost::mt19937&, boost::uniform_int<> > m_reexpressionJitter;
  boost::variate_generator<boost::mt19937&, boost::uniform_int<> > m_cumulativeOnlyRandom;


  /// @brief Lifetime of data interest
  time::milliseconds m_dataInterestLifetime;
  /// @brief Lifetime of sync interest
  time::milliseconds m_syncInterestLifetime;

  /// @brief FreshnessPeriod of Data
  time::milliseconds m_dataFreshness;

  // Security
  ndn::Name m_defaultSigningId;
  ndn::KeyChain m_keyChain;
  ndn::shared_ptr<ndn::Validator> m_validator;

  unsigned m_numberDataInterestTimeouts;
  unsigned m_numberRecoInterestTimeouts;

  std::set<ndn::Name>  m_pendingRecoveryPrefixes;

#ifdef _DEBUG
  int m_instanceId;
  static int m_instanceCounter;
#endif

};


} // namespace chronosync

#endif // CHRONOSYNC_LOGIC_HPP
