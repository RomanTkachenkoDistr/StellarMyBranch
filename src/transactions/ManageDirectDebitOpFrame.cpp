 //Copyright 2014 Stellar Development Foundation and contributors. Licensed
 //under the Apache License, Version 2.0. See the COPYING file at the root
 //of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/ManageDirectDebitOpFrame.h"
#include "database/Database.h"
#include "ledger/LedgerManager.h"
#include "ledger/DirectDebitFrame.h"
#include "main/Application.h"
#include "medida/meter.h"
#include "medida/metrics_registry.h"
#include "ledger/TrustFrame.h"
#include "ledger/AccountFrame.h"

namespace stellar
{
	ManageDirectDebitOpFrame::ManageDirectDebitOpFrame(Operation const& op, OperationResult& res,
		TransactionFrame& parentTx)
		: OperationFrame(op, res, parentTx)
		, mManageDirectDebit(mOperation.body.manageDirectDebitOp())
		
	{
	}

	bool
		ManageDirectDebitOpFrame::doApply(Application& app, LedgerDelta& delta,
			LedgerManager& ledgerManager)
	{
		Database& db = ledgerManager.getDatabase();
		DirectDebitFrame::pointer directDebit;
		directDebit = DirectDebitFrame::loadDirectDebit(mManageDirectDebit.debitor, mManageDirectDebit.asset, getSourceID(), db, &delta);
		if (mManageDirectDebit.cancelDebit)
		{
			if (!directDebit)
			{
				app.getMetrics()
					.NewMeter({ "op-manage-direct-debit", "failure", "no-direct-debit" },
						"operation")
					.Mark();
				innerResult().code(MANAGE_DIRECT_DEBIT_NOT_EXIST);
				return false;
			}
			app.getMetrics()
				.NewMeter({ "op-manage-direct-debit", "success", "apply" }, "operation")
				.Mark();
			innerResult().code(MANAGE_DIRECT_DEBIT_SUCCESS);
			
			directDebit->storeDelete(delta, db);
			mSourceAccount->addNumEntries(-1, ledgerManager);
			mSourceAccount->storeChange(delta, db);

			return true;
		}
		auto account = AccountFrame::loadAccount(mManageDirectDebit.debitor, db);
		if (!account)
		{
			app.getMetrics()
				.NewMeter({ "op-Manage-direct-debit", "failure", "debitor-not-exist" },
					"operation")
				.Mark();
			innerResult().code(MANAGE_DIRECT_DEBIT_MALFORMED);
			return false;
		}
		if (mManageDirectDebit.debitor == getSourceID())
		{
			app.getMetrics()
				.NewMeter({ "op-manage-direct-debit", "failure", "payment-self" },
					"operation")
				.Mark();
			innerResult().code(MANAGE_DIRECT_DEBIT_SELF_NOT_ALLOWED);
			return false;
		}
		if (mManageDirectDebit.asset.type() != ASSET_TYPE_NATIVE)
		{
			auto trustLine = TrustFrame::loadTrustLine(getSourceID(),mManageDirectDebit.asset, db, &delta); 
			if (!trustLine)
			{
				app.getMetrics()
					.NewMeter({ "op-manage-direct-debit", "failure", "no-trust-line" },
						"operation")
					.Mark();
				innerResult().code(MANAGE_DIRECT_DEBIT_NO_TRUST);
				return false;
			}
		}
		if (directDebit)
		{
			app.getMetrics()
				.NewMeter({ "op-manage-direct-debit", "failure", "debit-exist" },
					"operation")
				.Mark();
			innerResult().code(MANAGE_DIRECT_DEBIT_EXIST);
			return false;
		}
		if (!mSourceAccount->addNumEntries(1, ledgerManager))
		{
			app.getMetrics()
				.NewMeter({ "op-manage-direct-debit", "failure", "low-reserve" },
					"operation")
				.Mark();
			innerResult().code(MANAGE_DIRECT_DEBIT_LOW_RESERVE);
			return false;
		}
		
			app.getMetrics()
				.NewMeter({ "op-manage-direct-debit", "success", "apply" }, "operation")
				.Mark();
			innerResult().code(MANAGE_DIRECT_DEBIT_SUCCESS);
			DirectDebitFrame::pointer debit;
			LedgerEntry le;
			le.data.type(DIRECT_DEBIT);
			le.data.directDebit() = buildDirectDebit(getSourceID(), mManageDirectDebit);
			debit = std::make_shared<DirectDebitFrame>(le);
			mSourceAccount->storeChange(delta, db);
			debit->storeAdd(delta, db);
			
			return true;
		
	}

	bool
		ManageDirectDebitOpFrame::doCheckValid(Application& app)
	{
		if (!isAssetValid(mManageDirectDebit.asset))
		{
			app.getMetrics()
				.NewMeter({ "op-manage-direct-debit", "failure", "bad-asset" },
					"operation")
				.Mark();
			innerResult().code(MANAGE_DIRECT_DEBIT_MALFORMED);
			return false;
		}
		
		
		return true;
	}
	DirectDebitEntry
		ManageDirectDebitOpFrame::buildDirectDebit(AccountID const& creditor,
			ManageDirectDebitOp const& op)
	{
		DirectDebitEntry o;
		o.asset = op.asset;
		o.creditor = creditor;
		o.debitor = op.debitor;

		return o;
	}
}
