// Copyright (c) 2019 The Crypticcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef MASTERNODES_DPOS_CONTROLLER_H
#define MASTERNODES_DPOS_CONTROLLER_H

#include "dpos_p2p_messages.h"
#include "../primitives/block.h"
#include <map>
#include <memory>

class CValidationInterface;

namespace dpos
{
class CDposVoter;
struct CDposVoterOutput;

class CDposController
{
    class Validator;

public:
    static CDposController& getInstance();
    static void runEventLoop();

    bool isEnabled() const;
    CValidationInterface* getValidator();
    void initialize();
    void updateChainTip();

    Round getCurrentVotingRound() const;

    void proceedViceBlock(const CBlock& viceBlock);
    void proceedTransaction(const CTransaction& tx);
    void proceedRoundVote(const CRoundVote_p2p& vote);
    void proceedTxVote(const CTxVote_p2p& vote);

    bool findViceBlock(const uint256& hash, CBlock* block = nullptr) const;
    bool findRoundVote(const uint256& hash, CRoundVote_p2p* vote = nullptr) const;
    bool findTxVote(const uint256& hash, CTxVote_p2p* vote = nullptr) const;

    std::vector<CBlock> listViceBlocks() const;
    std::vector<CRoundVote_p2p> listRoundVotes() const;
    std::vector<CTxVote_p2p> listTxVotes() const;

    std::vector<CTransaction> listCommittedTxs() const;
    bool isCommittedTx(const CTransaction& tx) const;
    bool isTxApprovedByMe(const CTransaction& tx) const;

    std::vector<TxId> listIntersectedTxs() const;

private:
    CDposController() = default;
    ~CDposController() = default;
    CDposController(const CDposController&) = delete;
    CDposController(CDposController&&) = delete;
    CDposController& operator =(const CDposController&) = delete;

    bool handleVoterOutput(const CDposVoterOutput& out);
    bool acceptRoundVote(const CRoundVote_p2p& vote);
    bool acceptTxVote(const CTxVote_p2p& vote);

    void removeOldVotes();

private:
    std::shared_ptr<CDposVoter> voter;
    std::shared_ptr<Validator> validator;
    std::map<uint256, CTxVote_p2p> receivedTxVotes;
    std::map<uint256, CRoundVote_p2p> receivedRoundVotes;
};


static CDposController * getController()
{
    return &CDposController::getInstance();
}

} // namespace dpos

#endif // MASTERNODES_DPOS_CONTROLLER_H

