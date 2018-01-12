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

TEST_CASE("managedebit", "[tx][managedebit]")
{
	auto const& cfg = getTestConfig();

	VirtualClock clock;
	auto app = createTestApplication(clock, cfg);

	app->start();
	// set up world
	uint64_t const minBalance3 = app->getLedgerManager().getMinBalance(3);
	uint64_t const lowBalance = app->getLedgerManager().getMinBalance(1);
	
	TestAccount root = TestAccount::createRoot(*app);
	TestAccount creditor = root.create("creditor", minBalance3);
	TestAccount debitor = root.create("debitor", minBalance3);
	TestAccount account = root.create("acc", lowBalance);
	Asset nativeAsset = makeNativeAsset();

	Asset asset = makeAsset(debitor, "USD");

	bool createDebit = false;
	bool deleteDebit = true;
	
	SECTION("manage direct debit malformed") {
		
			Asset invalidAsset = makeInvalidAsset();
			SecretKey const& invalidAccount = SecretKey::random();

			SECTION("manage direct debit invalid asset") {
				for_all_versions(*app, [&] {
					REQUIRE_THROWS_AS(creditor.manageDirectDebit(invalidAsset, debitor, createDebit),
						ex_MANAGE_DIRECT_DEBIT_MALFORMED);
					REQUIRE_THROWS_AS(creditor.manageDirectDebit(invalidAsset, debitor, deleteDebit),
						ex_MANAGE_DIRECT_DEBIT_MALFORMED);
				});
			}
			SECTION("manage direct debit bad debitor")
			{
				for_all_versions(*app, [&] {
					REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, invalidAccount.getPublicKey(), 
						createDebit),
						ex_MANAGE_DIRECT_DEBIT_DEBITOR_NOT_EXIST);

				});
			}
		
	}
	
	SECTION("create direct debit self") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, creditor, createDebit),
				ex_MANAGE_DIRECT_DEBIT_SELF_NOT_ALLOWED);
		});
	}
	SECTION("create direct debit low reserve") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(account.manageDirectDebit(nativeAsset, creditor, createDebit),
				ex_MANAGE_DIRECT_DEBIT_LOW_RESERVE);
		});
	}
	SECTION("delete direct debit not exist") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, debitor, deleteDebit),
				ex_MANAGE_DIRECT_DEBIT_NOT_EXIST);
		});
	}
	SECTION("create direct debit no trust") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(creditor.manageDirectDebit(asset, debitor, createDebit),
				ex_MANAGE_DIRECT_DEBIT_NO_TRUST);
		});
	}
	
	SECTION("manage direct debit with native asset") {
		
			SECTION("create direct debit success") {
				for_all_versions(*app, [&] {
					creditor.manageDirectDebit(nativeAsset, debitor, createDebit);
					REQUIRE(loadDirectDebit(debitor, nativeAsset, creditor, *app, deleteDebit));
				});
			}
			SECTION("create direct debit exist") {
				for_all_versions(*app, [&] {
					creditor.manageDirectDebit(nativeAsset, debitor, createDebit);
					REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, debitor, createDebit),
						ex_MANAGE_DIRECT_DEBIT_EXIST);
				});
			}
			SECTION("delete direct debit succes") {
				for_all_versions(*app, [&] {
					creditor.manageDirectDebit(nativeAsset, debitor, createDebit);
					creditor.manageDirectDebit(nativeAsset, debitor, deleteDebit);

					REQUIRE(!loadDirectDebit(debitor, nativeAsset, creditor, *app, createDebit));
				});
			}
		
	}
	
	SECTION("manage direct debit with asset") {
		creditor.changeTrust(asset, 100);
		
			SECTION("create direct debit success") {
				for_all_versions(*app, [&] {
					creditor.manageDirectDebit(asset, debitor, createDebit);
					REQUIRE(loadDirectDebit(debitor, asset, creditor, *app, deleteDebit));
				});
			}
			SECTION("create direct debit exist") {
				for_all_versions(*app, [&] {
					creditor.manageDirectDebit(asset, debitor, createDebit);
					REQUIRE_THROWS_AS(creditor.manageDirectDebit(asset, debitor, createDebit),
						ex_MANAGE_DIRECT_DEBIT_EXIST);
				});
			}
			SECTION("delete direct debit succes") {
				for_all_versions(*app, [&] {
					creditor.manageDirectDebit(asset, debitor, createDebit);
					creditor.manageDirectDebit(asset, debitor, deleteDebit);

					REQUIRE(!loadDirectDebit(debitor, asset, creditor, *app, createDebit));
				});
			}
		
	}
}