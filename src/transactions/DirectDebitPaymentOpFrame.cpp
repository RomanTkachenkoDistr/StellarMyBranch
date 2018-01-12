//Copyright 2014 Stellar Development Foundation and contributors. Licensed
//under the Apache License, Version 2.0. See the COPYING file at the root
//of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "transactions/DirectDebitPaymentOpFrame.h"
#include "database/Database.h"
#include "ledger/LedgerManager.h"
#include "main/Application.h"
#include "medida/meter.h"
#include "medida/metrics_registry.h"
#include "ledger/TrustFrame.h"
#include "ledger/AccountFrame.h"
#include "PaymentOpFrame.h"
#include "ledger/DirectDebitFrame.h"

namespace stellar
{
	DirectDebitPaymentOpFrame::DirectDebitPaymentOpFrame(Operation const& op, OperationResult& res,
		TransactionFrame& parentTx)
		: OperationFrame(op, res, parentTx)
		, mDirectDebitPayment(mOperation.body.directDebitPaymentOp())

	{
	}

	bool
		DirectDebitPaymentOpFrame::doApply(Application& app, LedgerDelta& delta,
			LedgerManager& ledgerManager)
	{
		Database& db = ledgerManager.getDatabase();
		auto debitor = AccountFrame::loadAccount(mDirectDebitPayment.payment.destination, db);
		if (!debitor)
		{
			app.getMetrics()
				.NewMeter({ "op-direct-debit-payment", "failure", "destination-not-exist" },
					"operation")
				.Mark();
			innerResult().code(DIRECT_DEBIT_PAYMENT_ACCOUNT_NOT_EXIST);
			return false;
		}
		auto creditor = AccountFrame::loadAccount(mDirectDebitPayment.creditor, db);
		if (!creditor)
		{
			app.getMetrics()
				.NewMeter({ "op-direct-debit-payment", "failure", "creditor-not-exist" },
					"operation")
				.Mark();
			innerResult().code(DIRECT_DEBIT_PAYMENT_ACCOUNT_NOT_EXIST);
			return false;
		}
		auto debit = DirectDebitFrame::loadDirectDebit(getSourceID(), mDirectDebitPayment.payment.asset, mDirectDebitPayment.creditor, db, &delta);
		if (!debit)
		{
			app.getMetrics()
				.NewMeter({ "op-direct-debit-payment","failure","not-allowed" }, "operation")
				.Mark();
			innerResult().code(DIRECT_DEBIT_PAYMENT_NOT_ALLOWED);
			return false;
		}
		
		
		Operation op;
		op.sourceAccount.activate() = mDirectDebitPayment.creditor;
		op.body.type(PAYMENT);
		PaymentOp& payment = op.body.paymentOp();
		payment = mDirectDebitPayment.payment;
		OperationResult opRes;
		opRes.code(opINNER);
		opRes.tr().type(PAYMENT);
		PaymentOpFrame paymentOp(op, opRes, mParentTx);
		paymentOp.setSourceAccountPtr(creditor);
		if (!paymentOp.doCheckValid(app) ||
			!paymentOp.doApply(app, delta, ledgerManager))
		{
			if (paymentOp.getResultCode() != opINNER)
			{
				throw std::runtime_error("Unexpected error code from Payment");
			}
			DirectDebitPaymentResultCode res;

			switch (PaymentOpFrame::getInnerCode(paymentOp.getResult()))
			{
			case PAYMENT_UNDERFUNDED:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "underfunded" },
						"operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_UNDERFUNDED;
				break;
			case PAYMENT_SRC_NOT_AUTHORIZED:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "src-not-authorized" },
						"operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_SRC_NOT_AUTHORIZED;
				break;
			case PAYMENT_SRC_NO_TRUST:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "src-no-trust" },
						"operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_SRC_NO_TRUST;
				break;
			case PAYMENT_NO_DESTINATION:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "no-destination" },
						"operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_NO_DESTINATION;
				break;
			case PAYMENT_NO_TRUST:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "no-trust" }, 
						"operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_NO_TRUST;
				break;
			case PAYMENT_NOT_AUTHORIZED:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "not-authorized" },
						"operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_NOT_AUTHORIZED;
				break;
			case PAYMENT_LINE_FULL:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "line-full" }, "operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_LINE_FULL;
				break;
			case PAYMENT_NO_ISSUER:
				app.getMetrics()
					.NewMeter({ "op-direct-debit-payment", "failure", "no-issuer" }, "operation")
					.Mark();
				res = DIRECT_DEBIT_PAYMENT_NO_ISSUER;
				break;
			default:
				throw std::runtime_error("Unexpected error code from Payment");
			}
			innerResult().code(res);
			return false;
		}
		
		assert(PaymentOpFrame::getInnerCode(paymentOp.getResult()) ==
			PAYMENT_SUCCESS);

		app.getMetrics()
			.NewMeter({ "op-direct-debit-payment", "success", "apply" }, "operation")
			.Mark();
		innerResult().code(DIRECT_DEBIT_PAYMENT_SUCCESS);
		return true;
	}

	bool
		DirectDebitPaymentOpFrame::doCheckValid(Application& app)
	{
		
		if (!isAssetValid(mDirectDebitPayment.payment.asset))
		{
			app.getMetrics()
				.NewMeter({ "op-direct-debit-payment", "failure", "bad-asset" },
					"operation")
				.Mark();
			innerResult().code(DIRECT_DEBIT_PAYMENT_MALFORMED);
			return false;
		}
		
		
		return true;
	}
}
