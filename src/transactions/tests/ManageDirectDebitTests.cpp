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


using namespace stellar;
using namespace stellar::txtest;

TEST_CASE("manage_direct_debit", "[tx][manage_direct_debit]")
{
	auto const& cfg = getTestConfig();

	VirtualClock clock;
	auto app = createTestApplication(clock, cfg);

	app->start();
	
	auto const minBalance2 = app->getLedgerManager().getMinBalance(2);

	// set up world
	auto root = TestAccount::createRoot(*app);
	auto creditor = root.create("gw", minBalance2);
	auto debitor = root.create("debitor", minBalance2);
	
	Asset nativeAsset = makeNativeAsset();

	Asset asset = makeAsset(debitor, "USD");

	SECTION("manage direct debit malformed") {
		for_all_versions(*app, [&] {
			Asset invalidAsset = makeInvalidAsset();
			SecretKey invalidAccount = SecretKey::random();
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
	creditor.changeTrust(asset, 100);
	creditor.manageDirectDebit(asset, debitor, false);
	SECTION("create direct debit succes") {
		for_all_versions(*app, [&] {

			REQUIRE(loadDirectDebit(debitor, asset, creditor, *app, true));
		});
	}
	SECTION("create direct debit exist") {
		for_all_versions(*app, [&] {

			REQUIRE_THROWS_AS(creditor.manageDirectDebit(asset, debitor, false),
				ex_MANAGE_DIRECT_DEBIT_EXIST);
		});
	SECTION("delete direct debit succes") {
			for_all_versions(*app, [&] {
				creditor.manageDirectDebit(asset, debitor, true);
				REQUIRE(!loadDirectDebit(debitor, asset, creditor, *app, true));
			});
		}
	}
}