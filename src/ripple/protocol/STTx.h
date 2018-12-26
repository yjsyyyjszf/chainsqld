//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_PROTOCOL_STTX_H_INCLUDED
#define RIPPLE_PROTOCOL_STTX_H_INCLUDED

#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/TxFormats.h>
#include <boost/container/flat_set.hpp>
#include <boost/logic/tribool.hpp>
#include <ripple/json/impl/json_assert.h>
#include <ripple/core/DatabaseCon.h>
#include <functional>

namespace ripple {
// VFALCO TODO replace these macros with language constants
#define TXN_SQL_NEW         'N'
#define TXN_SQL_CONFLICT    'C'
#define TXN_SQL_HELD        'H'
#define TXN_SQL_VALIDATED   'V'
#define TXN_SQL_INCLUDED    'I'
#define TXN_SQL_UNKNOWN     'U'

class STTx final
    : public STObject
    , public CountedObject <STTx>
{
public:
    static char const* getCountedObjectName () { return "STTx"; }

    static std::size_t const minMultiSigners = 1;
    static std::size_t const maxMultiSigners = 8;

public:
    STTx() = delete;
    STTx& operator= (STTx const& other) = delete;

    STTx (STTx const& other) = default;

    explicit STTx (SerialIter& sit);
    explicit STTx (SerialIter&& sit) : STTx(sit) {}

    explicit STTx (STObject&& object);
    explicit STTx (Json::Value& o0bj, AccountID accountID);


    /** Constructs a transaction.

        The returned transaction will have the specified type and
        any fields that the callback function adds to the object
        that's passed in.
    */
    STTx (
        TxType type,
        std::function<void(STObject&)> assembler);
private:
	static void getOneTx(std::vector<STTx>& vec, STTx const& tx, std::string sTableNameInDB = "");
public:
    STBase*
    copy (std::size_t n, void* buf) const override
    {
        return emplace(n, buf, *this);
    }

    STBase*
    move (std::size_t n, void* buf) override
    {
        return emplace(n, buf, std::move(*this));
    }

    // STObject functions.
    SerializedTypeID getSType () const override
    {
        return STI_TRANSACTION;
    }
    std::string getFullText () const override;

    // Outer transaction functions / signature functions.
    Blob getSignature () const;

    uint256 getSigningHash () const;

    TxType getTxnType () const
    {
        return tx_type_;
    }

	bool isChainSqlTableType() const
	{
        return checkChainsqlTableType(tx_type_);
	}

	void addSubTx(STTx const& tx) const
	{
		pTxs_->push_back(tx);
	}

	std::vector<STTx> const& getSubTxs() const
	{
		return *pTxs_;
	}

    static bool checkChainsqlTableType(TxType txType)
    {
        return txType == ttTABLELISTSET || txType == ttSQLSTATEMENT || txType == ttSQLTRANSACTION;
    }

    static bool checkChainsqlContractType(TxType txType)
    {
        return txType == ttCONTRACT;
    }

	static std::pair<std::shared_ptr<STTx>, std::string> parseSTTx(Json::Value& obj, AccountID accountID);

	static std::vector<STTx> getTxs(STTx const& tx, std::string sTableNameInDB = "", std::shared_ptr<STObject const> contractRawMetadata = NULL);

	bool isCrossChainUpload() const;

	std::string buildRaw(std::string sOperationRule) const;

    Blob getSigningPubKey () const
    {
        return getFieldVL (sfSigningPubKey);
    }

    std::uint32_t getSequence () const
    {
        return getFieldU32 (sfSequence);
    }
    void setSequence (std::uint32_t seq)
    {
        return setFieldU32 (sfSequence, seq);
    }

    boost::container::flat_set<AccountID>
    getMentionedAccounts() const;

    uint256 getTransactionID () const
    {
        return tid_;
    }

    Json::Value getJson (int options) const override;
    Json::Value getJson (int options, bool binary) const;

    void sign (
        PublicKey const& publicKey,
        SecretKey const& secretKey);

    /** Check the signature.
        @return `true` if valid signature. If invalid, the error message string.
    */
    std::pair<bool, std::string>
    checkSign(bool allowMultiSign) const;

    // SQL Functions with metadata.
    static
    std::string const&
    getMetaSQLInsertReplaceHeader ();

    std::string getMetaSQL (
        std::uint32_t inLedger, std::string const& escapedMetaData) const;

    std::string getMetaSQL (
        Serializer rawTxn,
        std::uint32_t inLedger,
        char status,
        std::string const& escapedMetaData) const;

    // Peersafe db 
    bool storePeersafeSql(LockedSociSession &db, std::uint64_t SeqInLedger, std::uint32_t inLedger, std::shared_ptr<STObject const> contractRawMetadata) const;

private:
    std::pair<bool, std::string> checkSingleSign () const;
    std::pair<bool, std::string> checkMultiSign () const;

	void buildRaw(Json::Value& condition, std::string& rule) const;

    uint256 tid_;
    TxType tx_type_;
	std::shared_ptr<std::vector<STTx>> pTxs_;

};

bool passesLocalChecks (STObject const& st, std::string&);

/** Sterilize a transaction.

    The transaction is serialized and then deserialized,
    ensuring that all equivalent transactions are in canonical
    form. This also ensures that program metadata such as
    the transaction's digest, are all computed.
*/
std::shared_ptr<STTx const>
sterilize (STTx const& stx);

/** Check whether a transaction is a pseudo-transaction */
bool isPseudoTx(STObject const& tx);

} // ripple

#endif
