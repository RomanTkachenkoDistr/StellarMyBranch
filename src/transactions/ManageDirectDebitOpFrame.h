#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/OperationFrame.h"

namespace stellar
{
	class ManageDirectDebitOpFrame : public OperationFrame
	{
	private:
		ManageDirectDebitResult&
			innerResult() const
		{
			return getResult().tr().manageDirectDebitResult();
		}

		ManageDirectDebitOp const& mManageDirectDebit;

	public:
		ManageDirectDebitOpFrame(Operation const& op, OperationResult& res,
			TransactionFrame& parentTx);

		bool doApply(Application& app, LedgerDelta& delta,
			LedgerManager& ledgerManager) override;
		bool doCheckValid(Application& app) override;
		bool applyDelete(Application& app, LedgerDelta& delta,
			LedgerManager& ledgerManager);
		LedgerEntry buildDirectDebit(AccountID const & creditor, ManageDirectDebitOp const & op);

		static ManageDirectDebitResultCode
			getInnerCode(OperationResult const& res)
		{
		
			return res.tr().manageDirectDebitResult().code();
		}
		
	};
}
