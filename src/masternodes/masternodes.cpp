// Copyright (c) 2019 The Crypticcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternodes.h"

#include "primitives/block.h"
#include "txdb.h"

#include <algorithm>
#include <functional>

static const std::vector<unsigned char> MnTxMarker = {'M', 'n', 'T', 'x'};
static const std::map<char, MasternodesTxType> MasternodesTxTypeToCode =
{
    {'a', MasternodesTxType::AnnounceMasternode },
    {'A', MasternodesTxType::ActivateMasternode },
    {'O', MasternodesTxType::SetOperatorReward },
    {'V', MasternodesTxType::DismissVote },
    {'v', MasternodesTxType::DismissVoteRecall },
    {'F', MasternodesTxType::FinalizeDismissVoting }
    // Without CollateralSpent
};


/*
 * Checks if given tx is probably one of 'MasternodeTx', returns tx type and serialized metadata in 'data'
*/
MasternodesTxType GuessMasternodeTxType(CTransaction const & tx, std::vector<unsigned char> & metadata)
{
    CScript const & memo = tx.vout[0].scriptPubKey;
    CScript::const_iterator pc = memo.begin();
    opcodetype opcode;
    if (!memo.GetOp(pc, opcode) || opcode != OP_RETURN)
    {
        return MasternodesTxType::None;;
    }
    if (!memo.GetOp(pc, opcode, metadata) ||
            (opcode != OP_PUSHDATA1 &&
             opcode != OP_PUSHDATA2 &&
             opcode != OP_PUSHDATA4) ||
            metadata.size() < MnTxMarker.size() + 1 ||     // i don't know how much exactly, but at least MnTxSignature + type prefix
            memcmp(&metadata[0], &MnTxMarker[0], MnTxMarker.size()) != 0)
    {
        return MasternodesTxType::None;
    }
    auto const & it = MasternodesTxTypeToCode.find(metadata[MnTxMarker.size()]);
    if (it == MasternodesTxTypeToCode.end())
    {
        return MasternodesTxType::None;
    }
    metadata.erase(metadata.begin(), metadata.begin() + MnTxMarker.size() + 1);
    return it->second;
}

/*
 * Loads all data from DB, creates indexes, calculates voting counters
 */
void CMasternodesView::Load()
{
    Clear();
    // Load masternodes itself, creating indexes
    db.LoadMasternodes([this] (uint256 & nodeId, CMasternode & node)
    {
        node.counterVotesFrom = 0;
        node.counterVotesAgainst = 0;
        allNodes.insert(std::make_pair(nodeId, node));
        nodesByOwner.insert(std::make_pair(node.ownerAuthAddress, nodeId));
        nodesByOperator.insert(std::make_pair(node.operatorAuthAddress, nodeId));

        if (node.IsActive())
        {
            activeNodes.insert(nodeId);
        }
    });

    // Load dismiss votes and update voting counters
    db.LoadVotes([this] (uint256 & voteId, CDismissVote & vote)
    {
        votes.insert(std::make_pair(voteId, vote));

        if (vote.IsActive())
        {
            // Indexing only active votes
            votesFrom.insert(std::make_pair(vote.from, voteId));
            votesAgainst.insert(std::make_pair(vote.against, voteId));

            // Assumed that node exists
            assert(HasMasternode(vote.from));
            assert(HasMasternode(vote.against));
            ++allNodes[vote.from].counterVotesFrom;
            ++allNodes[vote.against].counterVotesAgainst;
        }
    });

    // Load undo information
    db.LoadUndo([this] (uint256 & txid, uint256 & affectedItem, char undoType)
    {
        txsUndo.insert(std::make_pair(txid, std::make_pair(affectedItem, static_cast<MasternodesTxType>(undoType))));
    });
}

/*
 * Private. Deactivates vote, decrement counters, save state
 * Nothing checks cause private.
*/
void CMasternodesView::DeactivateVote(uint256 const & voteId, uint256 const & txid, int height)
{
    CDismissVote & vote = votes[voteId];

    vote.disabledByTx = txid;
    vote.deadSinceHeight = height;
    --allNodes[vote.from].counterVotesFrom;
    --allNodes[vote.against].counterVotesAgainst;

    txsUndo.insert(std::make_pair(txid, std::make_pair(voteId, MasternodesTxType::DismissVoteRecall)));
    db.WriteUndo(txid, voteId, static_cast<char>(MasternodesTxType::DismissVoteRecall), *currentBatch);
    db.WriteVote(voteId, vote, *currentBatch);
}

/*
 * Private. Deactivates votes (from active node, against any), recalculates all counters
 * Used in two plases: on deactivation by collateral spent and on finalize voting.
 * Nothing checks cause private.
*/
void CMasternodesView::DeactivateVotesFor(uint256 const & nodeId, uint256 const & txid, int height)
{
    CMasternode & node = allNodes[nodeId];

    if (node.IsActive())
    {
        // Check, deactivate and recalc votes 'from' us (remember, 'votesFrom' and 'votesAgainst' contains only active votes)
        auto const & range = votesFrom.equal_range(nodeId);
        std::for_each(range.first, range.second, [&] (std::pair<uint256 const, uint256> const & it)
        {
            // it.first == nodeId (from), it.second == voteId
            DeactivateVote(it.second, txid, height);
        });
        votesFrom.erase(range.first, range.second);
    }
    // Check, deactivate and recalc votes 'against' us (votes "against us" can exist even if we're not activated yet)
    {
        auto const & range = votesAgainst.equal_range(nodeId);
        std::for_each(range.first, range.second, [&] (std::pair<uint256 const, uint256> const & it)
        {
            // it->first == nodeId (against), it->second == voteId
            DeactivateVote(it.second, txid, height);
        });
        votesAgainst.erase(range.first, range.second);
    }
    // Like a checksum, count that node.counterVotesFrom == node.counterVotesAgainst == 0 !!!
    assert (node.counterVotesFrom == 0);
    assert (node.counterVotesAgainst == 0);
}

bool CMasternodesView::OnCollateralSpent(uint256 const & nodeId, uint256 const & txid, uint input, int height)
{
    // Assumed, that node exists
    CMasternode & node = allNodes[nodeId];
    assert(node.collateralSpentTx != uint256());

    if (node.IsActive())
    {
        // Remove masternode from active set
        activeNodes.erase(nodeId);
    }

    PrepareBatch();
    DeactivateVotesFor(nodeId, txid, height);

    node.collateralSpentTx = txid;
    if (node.deadSinceHeight == -1)
    {
        node.deadSinceHeight = height;
        /// @todo @mn update prune index
//        batch->Write(make_pair(DB_MASTERNODESPRUNEINDEX, make_pair(node.deadSinceHeight, txid)), ' ');
    }

    txsUndo.insert(std::make_pair(txid, std::make_pair(nodeId, MasternodesTxType::CollateralSpent)));

    db.WriteUndo(txid, nodeId, static_cast<char>(MasternodesTxType::CollateralSpent), *currentBatch);
    db.WriteMasternode(nodeId, node, *currentBatch);
    return true;
}

bool CMasternodesView::OnMasternodeAnnounce(uint256 const & nodeId, CMasternode const & node)
{
    // Check, that there in no MN with such 'ownerAuthAddress' or 'operatorAuthAddress'
    if (HasMasternode(nodeId) ||
            nodesByOwner.find(node.ownerAuthAddress) != nodesByOwner.end() ||
            nodesByOperator.find(node.operatorAuthAddress) != nodesByOperator.end())
    {
        return false;
    }

    PrepareBatch();

    allNodes.insert(std::make_pair(nodeId, node));
    nodesByOwner.insert(std::make_pair(node.ownerAuthAddress, nodeId));
    nodesByOperator.insert(std::make_pair(node.operatorAuthAddress, nodeId));

    txsUndo.insert(std::make_pair(nodeId, std::make_pair(nodeId, MasternodesTxType::AnnounceMasternode)));

    db.WriteUndo(nodeId, nodeId, static_cast<char>(MasternodesTxType::AnnounceMasternode), *currentBatch);
    db.WriteMasternode(nodeId, node, *currentBatch);
    return true;
}

bool CMasternodesView::OnMasternodeActivate(uint256 const & txid, uint256 const & nodeId, CKeyID const & operatorId, int height)
{
    // Check, that MN was announced
    auto it = nodesByOperator.find(operatorId);
    if (it == nodesByOperator.end() || it->second != nodeId)
    {
        return false;
    }
    // Assumed now, that node exists and consistent with 'nodesByOperator' index
    CMasternode & node = allNodes[nodeId];
    // Checks that MN was not activated nor spent nor finalized (voting) yet
    // We can check only 'deadSinceHeight != -1' so it must be consistent with 'collateralSpentTx' and 'dismissFinalizedTx'
    if (node.activationTx != uint256() || node.deadSinceHeight != -1 || node.minActivationHeight < height)
    {
        return false;
    }

    PrepareBatch();

    node.activationTx = txid;
    activeNodes.insert(nodeId);

    txsUndo.insert(std::make_pair(txid, std::make_pair(nodeId, MasternodesTxType::ActivateMasternode)));

    db.WriteUndo(txid, nodeId, static_cast<char>(MasternodesTxType::ActivateMasternode), *currentBatch);
    db.WriteMasternode(nodeId, node, *currentBatch);
    return true;
}

bool CMasternodesView::OnDismissVote(uint256 const & txid, CDismissVote const & vote, CKeyID const & operatorId)
{
    // Checks if:
    //      MN with operator (from) exists and active
    //      MN 'against' exists and not spent nor finalized (but may be not activated yet)
    //      MN 'from' counter is less than...X
    //      vote with pair 'from'+'against' not exists, or exists but deactivated
    // Then, if all is OK, add vote and increment counters
    // Save
    // (we can get 'active' status just by searching in 'activeNodes' instead of .IsActive())
    auto const & itFrom = nodesByOperator.find(operatorId);
    if (itFrom == nodesByOperator.end() || allNodes[itFrom->second].IsActive() == false)
    {
        return false;
    }
    uint256 const & idNodeFrom = itFrom->second;

    auto itAgainst = allNodes.find(vote.against);
    // We can check only by 'deadSince != -1' so it must be consistent with 'collateralSpentTx' and 'dismissFinalizedTx'
    if (itAgainst == allNodes.end() || itAgainst->second.deadSinceHeight != -1)
    {
        return false;
    }
    CMasternode & nodeFrom = allNodes[idNodeFrom];
    CMasternode & nodeAgainst = itAgainst->second;
    if (nodeFrom.counterVotesFrom >= MAX_DISMISS_VOTES_PER_MN)
    {
        return false;
    }

    if (ExistActiveVoteIndex(VoteIndex::From, idNodeFrom, vote.against))
    {
        return false;
    }

    PrepareBatch();
    CDismissVote copy(vote);
    copy.from = idNodeFrom;

    // Updating indexes
    votes.insert(std::make_pair(txid, copy));
    votesFrom.insert(std::make_pair(copy.from, txid));
    votesAgainst.insert(std::make_pair(copy.against, txid));

    // Updating counters
    ++nodeFrom.counterVotesFrom;
    ++nodeAgainst.counterVotesAgainst;

    txsUndo.insert(std::make_pair(txid, std::make_pair(txid, MasternodesTxType::DismissVote)));

    db.WriteUndo(txid, txid, static_cast<char>(MasternodesTxType::DismissVote), *currentBatch);
    db.WriteVote(txid, copy, *currentBatch);
    // we don't write any nodes here, cause only their counters affected
    return true;
}

/*
 * Private. Search in active vote index for pair 'from' 'against'
 * returns optional iterator
*/
boost::optional<CDismissVotesIndex::const_iterator>
CMasternodesView::ExistActiveVoteIndex(VoteIndex where, uint256 const & from, uint256 const & against)
{
    typedef std::pair<uint256 const, uint256> const & TPairConstRef;
    typedef std::function<bool(TPairConstRef)> TPredicate;
    TPredicate const & isEqualAgainst = [&against, this] (TPairConstRef pair) { return votes[pair.second].against == against; };
    TPredicate const & isEqualFrom    = [&from,    this] (TPairConstRef pair) { return votes[pair.second].from == from; };

    auto const & range = (where == VoteIndex::From) ? votesFrom.equal_range(from) : votesAgainst.equal_range(against);
    CDismissVotesIndex::const_iterator it = std::find_if(range.first, range.second, where == VoteIndex::From ? isEqualAgainst : isEqualFrom);
    if (it == range.second)
    {
        return {};
    }
    return {it};
}

bool CMasternodesView::OnDismissVoteRecall(uint256 const & txid, uint256 const & against, CKeyID const & operatorId, int height)
{
    // I think we don't need extra checks here (MN active, from and against - if one of MN deactivated - votes was deactivated too). Just checks for active vote
    auto itFrom = nodesByOperator.find(operatorId);
    if (itFrom == nodesByOperator.end() || allNodes[itFrom->second].IsActive() == false)
    {
        return false;
    }
    uint256 const & idNodeFrom = itFrom->second;

    // We forced to iterate through all range to find target vote.
    // Unfortunately, even extra index wouldn't help us, cause we whatever need to erase recalled vote from both
    // Remember! Every REAL and ACTIVE vote (in 'votes' map) has 2 indexes.
    // So, when recall, we should remove 2 indexes but only one real vote
    auto const optionalIt = ExistActiveVoteIndex(VoteIndex::From, idNodeFrom, against);
    if (!optionalIt)
    {
        return false;
    }

    uint256 const & voteId = (*optionalIt)->second;
    CDismissVote & vote = votes[voteId];

    // Here is real job: modify and write vote, remove active indexes, write undo
    vote.disabledByTx = txid;
    vote.deadSinceHeight = height;

    --allNodes[idNodeFrom].counterVotesFrom;
    --allNodes[against].counterVotesAgainst; // important, was skipped first time!

    PrepareBatch();
    txsUndo.insert(std::make_pair(txid, std::make_pair(voteId, MasternodesTxType::DismissVoteRecall)));

    db.WriteUndo(txid, voteId, static_cast<char>(MasternodesTxType::DismissVoteRecall), *currentBatch);
    db.WriteVote(voteId, vote, *currentBatch);

    votesFrom.erase(*optionalIt);

    // Finally, remove link from second index. It SHOULD be there.
    {
        auto const optionalIt = ExistActiveVoteIndex(VoteIndex::Against, idNodeFrom, against);
        assert(!optionalIt);
        votesAgainst.erase(*optionalIt);
    }
    return true;
}

bool CMasternodesView::OnFinalizeDismissVoting(uint256 const & txid, uint256 const & nodeId, int height)
{
    auto it = allNodes.find(nodeId);
    // We can check only 'deadSinceHeight != -1' so it must be consistent with 'collateralSpentTx' and 'dismissFinalizedTx'
    // It will not be accepted if collateral was spent, cause votes were not accepted too (collateral spent is absolute blocking condition)
    if (it == allNodes.end() || it->second.counterVotesAgainst < GetMinDismissingQuorum() || it->second.deadSinceHeight != -1)
    {
        return false;
    }

    CMasternode & node = it->second;
    if (node.IsActive())
    {
        // Remove masternode from active set
        activeNodes.erase(nodeId);
    }

    PrepareBatch();
    DeactivateVotesFor(nodeId, txid, height);

    node.dismissFinalizedTx = txid;
    if (node.deadSinceHeight == -1)
    {
        node.deadSinceHeight = height;
        /// @todo @mn update prune index
//        batch->Write(make_pair(DB_MASTERNODESPRUNEINDEX, make_pair(node.deadSinceHeight, txid)), ' ');
    }

    txsUndo.insert(std::make_pair(txid, std::make_pair(nodeId, MasternodesTxType::FinalizeDismissVoting)));

    db.WriteUndo(txid, nodeId, static_cast<char>(MasternodesTxType::FinalizeDismissVoting), *currentBatch);
    db.WriteMasternode(nodeId, node, *currentBatch);

    return true;
}

//bool CMasternodesView::HasUndo(uint256 const & txid) const
//{
//    return txsUndo.find(txid) != txsUndo.end();
//}

bool CMasternodesView::OnUndo(uint256 const & txid)
{
    auto itUndo = txsUndo.find(txid);
    if (itUndo == txsUndo.end())
    {
        return false;
    }

    // *** Note: only one iteration except 'CollateralSpent' and 'FinalizeDismissVoting' cause additional votes restoration
    while (itUndo != txsUndo.end() && itUndo->first == txid)
    {
        // don'n know where its optimal place:
        PrepareBatch();

        //  *itUndo == std::pair<uint256 txid, std::pair<uint256 affected_object_id, MasternodesTxType> >
        uint256 const & id = itUndo->second.first;
        MasternodesTxType txType = itUndo->second.second;
        switch (txType)
        {
            case MasternodesTxType::CollateralSpent:    // notify that all deactivated child votes will be restored by DismissVoteRecall additional undo
            {
                CMasternode & node = allNodes[id];

                node.collateralSpentTx = uint256();
                // Check if 'spent' was an only reason to deactivate
                if (node.dismissFinalizedTx == uint256())
                {
                    node.deadSinceHeight = -1;
                    activeNodes.insert(id);
                }
                db.WriteMasternode(id, node, *currentBatch);
            }
            break;
            case MasternodesTxType::AnnounceMasternode:
            {
                CMasternode & node = allNodes[id];

                nodesByOwner.erase(node.ownerAuthAddress);
                nodesByOperator.erase(node.operatorAuthAddress);
                allNodes.erase(id);

                db.EraseMasternode(id, *currentBatch);
            }
            break;
            case MasternodesTxType::ActivateMasternode:
            {
                CMasternode & node = allNodes[id];

                node.activationTx = uint256();
                activeNodes.erase(id);

                db.WriteMasternode(id, node, *currentBatch);
            }
            break;
            case MasternodesTxType::SetOperatorReward:
                /// @todo @mn implement
                break;
            case MasternodesTxType::DismissVote:
            {
                CDismissVote & vote = votes[id];

                // Updating counters first
                --allNodes[vote.from].counterVotesFrom;
                --allNodes[vote.against].counterVotesAgainst;

                votesFrom.erase(vote.from);
                votesAgainst.erase(vote.against);
                votes.erase(id);    // last!

                db.EraseVote(id, *currentBatch);
            }
            break;
            case MasternodesTxType::DismissVoteRecall:
            {
                CDismissVote & vote = votes[id];

                ++allNodes[vote.from].counterVotesFrom;
                ++allNodes[vote.against].counterVotesAgainst;

                votesFrom.insert(std::make_pair(vote.from, id));
                votesAgainst.insert(std::make_pair(vote.against, id));

                vote.disabledByTx = uint256();
                vote.deadSinceHeight = -1;

                db.WriteVote(id, vote, *currentBatch);
            }
            break;
            case MasternodesTxType::FinalizeDismissVoting: // notify that all deactivated child votes will be restored by DismissVoteRecall additional undo
            {
                CMasternode & node = allNodes[id];

                node.dismissFinalizedTx = uint256();
                if (node.collateralSpentTx == uint256())
                {
                    node.deadSinceHeight = -1;
                }
                if (node.IsActive())
                {
                    activeNodes.insert(id);
                }
                db.WriteMasternode(id, node, *currentBatch);
            }
            break;

            default:
                break;
        }
        db.EraseUndo(txid, id, *currentBatch); // erase db first! then map (cause iterator)!
        itUndo = txsUndo.erase(itUndo); // instead ++itUndo;
    }
    return true;
}

uint32_t CMasternodesView::GetMinDismissingQuorum()
{
    return 1 + (activeNodes.size() * 2) / 3; // 66% + 1
}

void CMasternodesView::PrepareBatch()
{
    if (!currentBatch)
    {
        currentBatch.reset(new CDBBatch(db));
    }
}


void CMasternodesView::WriteBatch()
{
    if (currentBatch)
    {
        db.WriteBatch(*currentBatch);
        currentBatch.reset();
    }
}

void CMasternodesView::DropBatch()
{
    if (currentBatch)
    {
        currentBatch.reset();
    }
}

void CMasternodesView::Clear()
{
    allNodes.clear();
    activeNodes.clear();
    nodesByOwner.clear();
    nodesByOperator.clear();

    votes.clear();
    votesFrom.clear();
    votesAgainst.clear();

    txsUndo.clear();
}