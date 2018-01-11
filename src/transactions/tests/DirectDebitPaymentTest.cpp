// Copyright 2016 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "lib/catch.hpp"
#include "lib/json/json.h"
#include "main/Application.h"
#include "test/TestAccount.h"
#include "test/TestExceptions.h"
#include "test/TestUtils.h"
#include "test/TxTests.h"
#include "test/test.h"
#include "util/Logging.h"
#include "util/make_unique.h"
#include "util/Timer.h"
#include "ledger/LedgerDelta.h"


using namespace stellar;
using namespace stellar::txtest;

TEST_CASE("debitpayment", "[tx][debitpayment]")
{
	auto const& cfg = getTestConfig();

	VirtualClock clock;
	auto app = createTestApplication(clock, cfg);

	app->start();

	auto const minBalance3 = app->getLedgerManager().getMinBalance(3);
	// set up world
	auto root = TestAccount::createRoot(*app);
	auto creditor = root.create("creditor", minBalance3);
	auto debitor = root.create("debitor", minBalance3);
	auto destination = root.create("acc", minBalance3);
	Asset nativeAsset = makeNativeAsset();
	int64_t amount = 20;
	Asset invalidAsset = makeInvalidAsset();
	
	Asset asset = makeAsset(debitor, "USD");
	
	SECTION("debit payment not allowed") {
		SECTION("native asset not allowed") {
			for_all_versions(*app, [&] {
				REQUIRE_THROWS_AS(debitor.directDebitPayment(creditor, destination, nativeAsset, amount),
					ex_DIRECT_DEBIT_PAYMNET_NOT_ALLOWED);
			});
		}
		SECTION("allowed asset but native not allowed") {
			for_all_versions(*app, [&] {
			creditor.changeTrust(asset, 10000);
			creditor.manageDirectDebit(asset, debitor, false);
			
				REQUIRE_THROWS_AS(debitor.directDebitPayment(creditor, destination, nativeAsset, amount),
					ex_DIRECT_DEBIT_PAYMNET_NOT_ALLOWED);
			});
		}
		SECTION("allowed native asset but asset not allowed") {
			for_all_versions(*app, [&] {
			creditor.changeTrust(asset, 10000);
			creditor.manageDirectDebit(nativeAsset, debitor, false);
			
				REQUIRE_THROWS_AS(debitor.directDebitPayment(creditor, destination, asset, amount),
					ex_DIRECT_DEBIT_PAYMNET_NOT_ALLOWED);
			});
		}
	}
	SECTION("debit payment native asset success")
	{
		for_all_versions(*app, [&] {
		creditor.manageDirectDebit(nativeAsset, debitor, false);
		debitor.directDebitPayment(creditor, destination, nativeAsset, amount);
		AccountFrame::pointer creditorAcc, debitorAcc, destAcc;
		creditorAcc = loadAccount(creditor, *app);
		debitorAcc = loadAccount(debitor, *app);
		destAcc = loadAccount(destination, *app);
		
			REQUIRE(debitorAcc->getBalance() + 100 == minBalance3);
			REQUIRE(creditorAcc->getBalance() + 100 + amount == minBalance3);
			REQUIRE((destAcc->getBalance() - amount) == minBalance3);
		});
	}
	SECTION("debit payment non-native asset success")
	{
		for_all_versions(*app, [&] {
			creditor.changeTrust(asset, 1000);
			destination.changeTrust(asset, 1000);
			debitor.pay(creditor, asset, amount);

			creditor.manageDirectDebit(asset, debitor, false);

			debitor.directDebitPayment(creditor, destination, asset, amount);
			
			auto creditorBalance = creditor.loadTrustLine(asset).balance;
			auto destBalance = destination.loadTrustLine(asset).balance;

			REQUIRE(destBalance == amount);
			REQUIRE(creditorBalance == 0);
			
		});
	}
	
	
}