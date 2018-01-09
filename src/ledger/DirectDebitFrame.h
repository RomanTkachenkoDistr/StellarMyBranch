#pragma once

// Copyright 2014 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0


#include "ledger/EntryFrame.h"
#include <functional>
#include <unordered_map>

namespace soci
{
	class session;
	namespace details
	{
		class prepare_temp_type;
	}
}

namespace stellar
{

	class LedgerRange;

	class StatementContext;

	class DirectDebitFrame : public EntryFrame
	{
	private:

		static void
			loadDebits(StatementContext& prep,
				std::function<void(LedgerEntry const&)> debitProcessor);
	public:
		typedef std::shared_ptr<DirectDebitFrame> pointer;

		DirectDebitFrame();
		DirectDebitFrame(LedgerEntry const& from);
		DirectDebitFrame(DirectDebitFrame const& from);
		DirectDebitEntry& mDirectDebit;
		EntryFrame::pointer
			copy() const override
		{
			return std::make_shared<DirectDebitFrame>(*this);
		}
		// Instance-based overrides of EntryFrame.
		void storeDelete(LedgerDelta& delta, Database& db) const override;
		void storeChange(LedgerDelta& delta, Database& db) override;
		void storeAdd(LedgerDelta& delta, Database& db) override;
		static void storeDelete(LedgerDelta& delta, Database& db,
			LedgerKey const& key);
		static bool exists(Database& db, LedgerKey const& key);
		static uint64_t countObjects(soci::session& sess);
		static uint64_t countObjects(soci::session& sess,
			LedgerRange const& ledgers);
		
		static pointer loadDirectDebit(AccountID const& debitor, Asset const& asset,AccountID const& creditor,
			Database& db, LedgerDelta* delta = nullptr);


		static void DirectDebitFrame::deleteTrustLinesModifiedOnOrAfterLedger(Database& db,
				uint32_t oldestLedger);
		
		static std::unordered_map<AccountID, std::vector<DirectDebitFrame::pointer>>
			loadAllDebits(Database& db);


		static void getKeyFields(LedgerKey const& key, std::string& debitor,
			std::string& creditor, std::string& assetCode,std::string& assetIssuer);
		
		DirectDebitEntry const&
			getDirectDebit() const
		{
			return mDirectDebit;
		}
		DirectDebitEntry&
			getDirectDebit()
		{
			return mDirectDebit;
		}

		static void dropAll(Database& db);

	private:
		static bool isValid(LedgerEntry const& le);
		bool isValid() const;

		static const char* kSQLCreateStatement1;
		static const char* kSQLCreateStatement2;
	};
}
