#pragma once

// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/OperationFrame.h"

namespace stellar
{
	class DirectDebitPaymentOpFrame : public OperationFrame
	{
	private:
		DirectDebitPaymentResult &
			innerResult() const
		{
			return getResult().tr().directDebitPaymentResult();
		}

		DirectDebitPaymentOp const& mDirectDebitPayment;
		

	public:
		DirectDebitPaymentOpFrame(Operation const& op, OperationResult& res,
			TransactionFrame& parentTx);

		bool doApply(Application& app, LedgerDelta& delta,
			LedgerManager& ledgerManager) override;
		bool doCheckValid(Application& app) override;

		static DirectDebitPaymentResultCode
			getInnerCode(OperationResult const& res)
		{

			return res.tr().directDebitPaymentResult().code();
		}
	};
}
