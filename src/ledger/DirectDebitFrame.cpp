// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "ledger/DirectDebitFrame.h"
#include "LedgerDelta.h"
#include "crypto/KeyUtils.h"
#include "crypto/SHA.h"
#include "crypto/SecretKey.h"
#include "database/Database.h"
#include "ledger/LedgerRange.h"
#include "util/types.h"
#include "transactions/ManageDirectDebitOpFrame.h"


using namespace std;
using namespace soci;

namespace stellar
{
	using xdr::operator==;

	
	const char* DirectDebitFrame::kSQLCreateStatement1 =
		"CREATE TABLE debits"
		"("
		"creditor     VARCHAR(56)     NOT NULL,"
		"debitor      VARCHAR(56)     NOT NULL,"
		"assetcode    VARCHAR(12)     NOT NULL,"
		"assettype    INT             NOT NULL,"
		"assetissuer  VARCHAR(56)     NOT NULL,"
		"lastmodified INT             NOT NULL,"
		"PRIMARY KEY  (debitor, creditor, assetcode, assetissuer)"
		");";

	DirectDebitFrame::DirectDebitFrame()
		: EntryFrame(DIRECT_DEBIT)
		, mDirectDebit(mEntry.data.directDebit())
		
	{
	}

	DirectDebitFrame::DirectDebitFrame(LedgerEntry const& from)
		: EntryFrame(from), mDirectDebit(mEntry.data.directDebit())
	{
		assert(isValid());
	}

	DirectDebitFrame::DirectDebitFrame(DirectDebitFrame const& from) : DirectDebitFrame(from.mEntry)
	{
	}
	bool
		DirectDebitFrame::isValid(LedgerEntry const& le)
	{
		bool res = (le.lastModifiedLedgerSeq <= INT32_MAX);
		DirectDebitEntry const& debit = le.data.directDebit();
		res = res && isAssetValid(debit.asset);
		res = res && !(debit.creditor == debit.debitor);
		return res;
	}

	bool
		DirectDebitFrame::isValid() const
	{
		return isValid(mEntry);
	}

	bool
		DirectDebitFrame::exists(Database& db, LedgerKey const& key)
	{
		if (cachedEntryExists(key, db) && getCachedEntry(key, db) != nullptr)
		{
			return true;
		}

		std::string debitor, creditor, assetCode, assetIssuer;
		getKeyFields(key, debitor, creditor, assetCode,assetIssuer);
		int exists = 0;
		auto timer = db.getSelectTimer("debit-exists");
		auto prep = db.getPreparedStatement(
			"SELECT EXISTS (SELECT NULL FROM debits "
			"WHERE debitor=:v1 AND creditor=:v2 AND assetcode=:v3 AND assetissuer=:v4)");
		auto& st = prep.statement();
		st.exchange(use(debitor));
		st.exchange(use(creditor));
		st.exchange(use(assetCode));
		st.exchange(use(assetIssuer));
		st.exchange(into(exists));
		st.define_and_bind();
		st.execute(true);
		return exists != 0;
	}

	uint64_t
		DirectDebitFrame::countObjects(soci::session& sess)
	{
		uint64_t count = 0;
		sess << "SELECT COUNT(*) FROM debits;", into(count);
		return count;
	}

	uint64_t
		DirectDebitFrame::countObjects(soci::session& sess, LedgerRange const& ledgers)
	{
		uint64_t count = 0;
		sess << "SELECT COUNT(*) FROM debits"
			" WHERE lastmodified >= :v1 AND lastmodified <= :v2;",
			into(count), use(ledgers.first()), use(ledgers.last());
		return count;
	}
	void
	DirectDebitFrame::getKeyFields(LedgerKey const& key, std::string& debitor,
		std::string& creditor, std::string& assetCode, std::string& assetIssuer)
	{
		debitor = KeyUtils::toStrKey(key.directDebit().debitor);
		creditor = KeyUtils::toStrKey(key.directDebit().creditor);
		if (key.directDebit().asset.type() == ASSET_TYPE_CREDIT_ALPHANUM4)
		{
			assetIssuer = KeyUtils::toStrKey(key.directDebit().asset.alphaNum4().issuer);
			assetCodeToStr(key.directDebit().asset.alphaNum4().assetCode, assetCode);
		}
		else if (key.directDebit().asset.type() == ASSET_TYPE_CREDIT_ALPHANUM12)
		{
			assetIssuer = KeyUtils::toStrKey(key.directDebit().asset.alphaNum12().issuer);
			assetCodeToStr(key.directDebit().asset.alphaNum12().assetCode, assetCode);
		}

	
	}

	void
		DirectDebitFrame::storeDelete(LedgerDelta& delta, Database& db) const
	{
		storeDelete(delta, db, getKey());
	}

	void
		DirectDebitFrame::storeDelete(LedgerDelta& delta, Database& db, LedgerKey const& key)
	{
		flushCachedEntry(key, db);

		std::string debitor, creditor, assetCode,assetIssuer;
		getKeyFields(key, debitor, creditor, assetCode,assetIssuer);

		auto timer = db.getDeleteTimer("debit");
		db.getSession() << "DELETE FROM debits "
			"WHERE debitor=:v1 AND creditor=:v2 AND assetcode=:v3 AND assetissuer=:v4",
			use(debitor), use(creditor), use(assetCode), use(assetIssuer);

		delta.deleteEntry(key);
	}

	

	void
		DirectDebitFrame::storeAdd(LedgerDelta& delta, Database& db)
	{
		if (!isValid())
		{
			throw std::runtime_error("Invalid DirectDebitEntry");
		}

		auto key = getKey();
		flushCachedEntry(key, db);

		touch(delta);

		std::string debitor, creditor, assetCode, assetIssuer;
		unsigned int assetType = key.directDebit().asset.type();
		getKeyFields(key, debitor, creditor, assetCode, assetIssuer);
		debitor = KeyUtils::toStrKey(mDirectDebit.debitor);
		
		auto prep = db.getPreparedStatement(
			"INSERT INTO debits "
			"(debitor, creditor, assettype, assetcode, assetissuer, "
			"lastmodified) "
			"VALUES (:v1, :v2, :v3, :v4, :v5, :v6)");
		auto& st = prep.statement();
		auto lastModified = getLastModified();
		st.exchange(use(debitor));
		st.exchange(use(creditor));
		st.exchange(use(assetType));
		st.exchange(use(assetCode));
		st.exchange(use(assetIssuer));
		st.exchange(use(lastModified));
		st.define_and_bind();
		{
			auto timer = db.getInsertTimer("debit");
			st.execute(true);
		}

		if (st.get_affected_rows() != 1)
		{
			throw std::runtime_error("Could not update data in SQL");
		}

		delta.addEntry(*this);
	}

		
	static const char* directDebitColumnSelector =
		"SELECT creditor, debitor, assetcode, assettype, assetissuer, lastmodified FROM debits";
		

	DirectDebitFrame::pointer
		DirectDebitFrame::loadDirectDebit(AccountID const& debitor, Asset const& asset,AccountID const& creditor,
			Database& db, LedgerDelta* delta)
	{
		

		LedgerKey key;
		key.type(DIRECT_DEBIT);
		key.directDebit().debitor = debitor;
		key.directDebit().asset = asset;
		key.directDebit().creditor = creditor;
		if (cachedEntryExists(key, db))
		{
			auto p = getCachedEntry(key, db);
			if (p)
			{
				pointer ret = std::make_shared<DirectDebitFrame>(*p);
				if (delta)
				{
					delta->recordEntry(*ret);
				}
				return ret;
			}
		}

		std::string debitorStr, creditorStr, assetCode,assetIssuer;

		getKeyFields(key, debitorStr, creditorStr, assetCode, assetIssuer);

		auto query = std::string(directDebitColumnSelector);
		query += (" WHERE debitor = :debitorid "
			" AND creditor = :creditorid"
			" AND assetcode = :asset"
			" AND assetissuer =  :issuer");
		auto prep = db.getPreparedStatement(query);
		auto& st = prep.statement();
		st.exchange(use(debitorStr));
		st.exchange(use(creditorStr));
		st.exchange(use(assetCode));
		st.exchange(use(assetIssuer));

		pointer retDebit;
		auto timer = db.getSelectTimer("debit");
		loadDebits(prep, [&retDebit](LedgerEntry const& debit) {
			retDebit = make_shared<DirectDebitFrame>(debit);
		});

		if (retDebit)
		{
			retDebit->putCachedEntry(db);
		}
		else
		{
			putCachedEntry(key, nullptr, db);
		}

		if (delta && retDebit)
		{
			delta->recordEntry(*retDebit);
		}
		return retDebit;
	}


	void
		DirectDebitFrame::loadDebits(StatementContext& prep,
			std::function<void(LedgerEntry const&)> debitProcessor)
	{
		
		std::string debitorStrKey, creditorStrKey, assetCode, assetIssuer;
		unsigned int assetType;

		LedgerEntry le;
		le.data.type(DIRECT_DEBIT);

		DirectDebitEntry& debit = le.data.directDebit();
		int lastModified = 0;

		auto& st = prep.statement();
		st.exchange(into(creditorStrKey));
		st.exchange(into(debitorStrKey));
		st.exchange(into(assetCode));
		st.exchange(into(assetType));
		st.exchange(into(assetIssuer));
		st.exchange(into(lastModified));
		st.define_and_bind();
		st.execute(true);
		while (st.got_data())
		{
			le.lastModifiedLedgerSeq = lastModified;
			debit.creditor = KeyUtils::fromStrKey<PublicKey>(creditorStrKey);
			debit.debitor = KeyUtils::fromStrKey<PublicKey>(debitorStrKey);
			debit.asset.type((AssetType)assetType);
			if (assetType == ASSET_TYPE_CREDIT_ALPHANUM4)
			{
				debit.asset.alphaNum4().issuer =
					KeyUtils::fromStrKey<PublicKey>(assetIssuer);
				strToAssetCode(debit.asset.alphaNum4().assetCode, assetCode);
			}
			else if (assetType == ASSET_TYPE_CREDIT_ALPHANUM12)
			{
				debit.asset.alphaNum12().issuer =
					KeyUtils::fromStrKey<PublicKey>(assetIssuer);
				strToAssetCode(debit.asset.alphaNum12().assetCode, assetCode);
			}

			if (!isValid(le))
			{
				throw std::runtime_error("Invalid DebitEntry");
			}

			debitProcessor(le);

			st.fetch();
		}
	}

	void
		DirectDebitFrame::dropAll(Database& db)
	{
		db.getSession() << "DROP TABLE IF EXISTS debits;";
		db.getSession() << kSQLCreateStatement1;
	}
	void
		DirectDebitFrame::storeChange(LedgerDelta& delta, Database& db)
	{
		
	}
	void
		DirectDebitFrame::deleteDirectDebitsModifiedOnOrAfterLedger(Database& db,
			uint32_t oldestLedger)
	{
		db.getEntryCache().erase_if(
			[oldestLedger](std::shared_ptr<LedgerEntry const> le) -> bool {
			return le && le->data.type() == DIRECT_DEBIT &&
				le->lastModifiedLedgerSeq >= oldestLedger;
		});

		{
			auto prep = db.getPreparedStatement(
				"DELETE FROM debits WHERE lastmodified >= :v1");
			auto& st = prep.statement();
			st.exchange(soci::use(oldestLedger));
			st.define_and_bind();
			st.execute(true);
		}
	}
	DirectDebitFrame&
		DirectDebitFrame::operator=(DirectDebitFrame const& other)
	{
		if (&other != this)
		{
			mDirectDebit = other.mDirectDebit;
			mKey = other.mKey;
			mKeyCalculated = other.mKeyCalculated;
		}
		return *this;
	}
	std::unordered_map<AccountID, std::vector<DirectDebitFrame::pointer>>
		DirectDebitFrame::loadAllDebits(Database& db)
	{
		std::unordered_map<AccountID, std::vector<DirectDebitFrame::pointer>> retDebits;
		auto query = std::string(directDebitColumnSelector);
		query += " ORDER BY creditor";
		auto prep = db.getPreparedStatement(query);

		auto timer = db.getSelectTimer("debit");
		loadDebits(prep, [&retDebits](LedgerEntry const& of) {
			auto& thisUserDebits = retDebits[of.data.directDebit().creditor];
			thisUserDebits.emplace_back(make_shared<DirectDebitFrame>(of));
		});
        
		return retDebits;
	}
	
}
