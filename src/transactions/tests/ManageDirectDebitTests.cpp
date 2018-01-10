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

TEST_CASE("manage_direct_debit", "[tx][manage_direct_debit]")
{
	auto const& cfg = getTestConfig();

	VirtualClock clock;
	auto app = createTestApplication(clock, cfg);

	app->start();

	auto const minBalance3 = app->getLedgerManager().getMinBalance(3);
	auto const lowBalance = app->getLedgerManager().getMinBalance(1);
	// set up world
	auto root = TestAccount::createRoot(*app);
	auto creditor = root.create("gw", minBalance3);
	auto debitor = root.create("debitor", minBalance3);
	auto account = root.create("acc", lowBalance);
	Asset nativeAsset = makeNativeAsset();

	Asset asset = makeAsset(debitor, "USD");
	
	SECTION("manage direct debit malformed") {
		for_all_versions(*app, [&] {
			Asset invalidAsset = makeInvalidAsset();
			SecretKey const& invalidAccount = SecretKey::random();

			SECTION("manage direct debit invalid asset") {

				REQUIRE_THROWS_AS(creditor.manageDirectDebit(invalidAsset, debitor, false),
					ex_MANAGE_DIRECT_DEBIT_MALFORMED);
				REQUIRE_THROWS_AS(creditor.manageDirectDebit(invalidAsset, debitor, true),
					ex_MANAGE_DIRECT_DEBIT_MALFORMED);

			}
			SECTION("manage direct debit bad debitor")
			{

				REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, invalidAccount.getPublicKey(), false),
					ex_MANAGE_DIRECT_DEBIT_MALFORMED);


			}
		});
	}
	
	SECTION("create direct debit self") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, creditor, false),
				ex_MANAGE_DIRECT_DEBIT_SELF_NOT_ALLOWED);
		});
	}
	SECTION("create direct debit low reserve") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(account.manageDirectDebit(nativeAsset, creditor, false),
				ex_MANAGE_DIRECT_DEBIT_LOW_RESERVE);
		});
	}
	SECTION("delete direct debit not exist") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, debitor, true),
				ex_MANAGE_DIRECT_DEBIT_NOT_EXIST);
		});
	}
	SECTION("create direct debit no trust") {
		for_all_versions(*app, [&] {
			REQUIRE_THROWS_AS(creditor.manageDirectDebit(asset, debitor, false),
				ex_MANAGE_DIRECT_DEBIT_NO_TRUST);
		});
	}
	
	SECTION("manage direct debit with native asset") {
		for_all_versions(*app, [&] {
			SECTION("create direct debit success") {

				creditor.manageDirectDebit(nativeAsset, debitor, false);
				REQUIRE(loadDirectDebit(debitor, nativeAsset, creditor, *app, true));

			}
			SECTION("create direct debit exist") {

				creditor.manageDirectDebit(nativeAsset, debitor, false);
				REQUIRE_THROWS_AS(creditor.manageDirectDebit(nativeAsset, debitor, false),
					ex_MANAGE_DIRECT_DEBIT_EXIST);
			}
			SECTION("delete direct debit succes") {

				creditor.manageDirectDebit(nativeAsset, debitor, false);
				creditor.manageDirectDebit(nativeAsset, debitor, true);

				REQUIRE(!loadDirectDebit(debitor, nativeAsset, creditor, *app, false));

			}
		});
	}
	
	SECTION("manage direct debit with asset") {
		creditor.changeTrust(asset, 100);
		for_all_versions(*app, [&] {
			SECTION("create direct debit success") {

				creditor.manageDirectDebit(asset, debitor, false);
				REQUIRE(loadDirectDebit(debitor, asset, creditor, *app, true));

			}
			SECTION("create direct debit exist") {

				creditor.manageDirectDebit(asset, debitor, false);
				REQUIRE_THROWS_AS(creditor.manageDirectDebit(asset, debitor, false),
					ex_MANAGE_DIRECT_DEBIT_EXIST);
			}
			SECTION("delete direct debit succes") {

				creditor.manageDirectDebit(asset, debitor, false);
				creditor.manageDirectDebit(asset, debitor, true);

				REQUIRE(!loadDirectDebit(debitor, asset, creditor, *app, false));

			}
		});
	}
}