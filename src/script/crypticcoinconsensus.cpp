// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "crypticcoinconsensus.h"

#include "consensus/upgrades.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/interpreter.h"
#include "version.h"

namespace {

/** A class that deserializes a single CTransaction one time. */
class TxInputStream
{
public:
    TxInputStream(int nTypeIn, int nVersionIn, const unsigned char *txTo, size_t txToLen) :
    m_type(nTypeIn),
    m_version(nVersionIn),
    m_data(txTo),
    m_remaining(txToLen)
    {}

    void read(char* pch, size_t nSize)
    {
        if (nSize > m_remaining)
            throw std::ios_base::failure(std::string(__func__) + ": end of data");

        if (pch == NULL)
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");

        if (m_data == NULL)
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");

        memcpy(pch, m_data, nSize);
        m_remaining -= nSize;
        m_data += nSize;
    }

    template<typename T>
    TxInputStream& operator>>(T& obj)
    {
        ::Unserialize(*this, obj);
        return *this;
    }

    int GetVersion() const { return m_version; }
    int GetType() const { return m_type; }
private:
    const int m_type;
    const int m_version;
    const unsigned char* m_data;
    size_t m_remaining;
};

inline int set_error(crypticcoinconsensus_error* ret, crypticcoinconsensus_error serror)
{
    if (ret)
        *ret = serror;
    return 0;
}

struct ECCryptoClosure
{
    ECCVerifyHandle handle;
};

ECCryptoClosure instance_of_eccryptoclosure;
}

int crypticcoinconsensus_verify_script(const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
                                    const unsigned char *txTo        , unsigned int txToLen,
                                    unsigned int nIn, unsigned int flags, crypticcoinconsensus_error* err)
{
    try {
        TxInputStream stream(SER_NETWORK, PROTOCOL_VERSION, txTo, txToLen);
        CTransaction tx;
        stream >> tx;
        if (nIn >= tx.vin.size())
            return set_error(err, crypticcoinconsensus_ERR_TX_INDEX);
        if (GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) != txToLen)
            return set_error(err, crypticcoinconsensus_ERR_TX_SIZE_MISMATCH);

         // Regardless of the verification result, the tx did not error.
         set_error(err, crypticcoinconsensus_ERR_OK);
        PrecomputedTransactionData txdata(tx);
        CAmount am(0);
        uint32_t consensusBranchId = SPROUT_BRANCH_ID;
        return VerifyScript(
            tx.vin[nIn].scriptSig,
            CScript(scriptPubKey, scriptPubKey + scriptPubKeyLen),
            flags,
            TransactionSignatureChecker(&tx, nIn, am, txdata),
            consensusBranchId,
            NULL);
    } catch (const std::exception&) {
        return set_error(err, crypticcoinconsensus_ERR_TX_DESERIALIZE); // Error deserializing
    }
}

unsigned int crypticcoinconsensus_version()
{
    // Just use the API version for now
    return CRYPTICCOINCONSENSUS_API_VER;
}
