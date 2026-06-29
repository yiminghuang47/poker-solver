// Unit tests for the Kuhn poker game model. No framework: a tiny CHECK macro
// that counts failures and reports a nonzero exit code to CTest.

#include "poker_solver/kuhn.hpp"

#include <iostream>
#include <string>

namespace kuhn = poker_solver::kuhn;

namespace {
int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::cerr << "FAIL: " << #cond << "  (" << __FILE__ << ':'    \
                      << __LINE__ << ")\n";                               \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

// Card indices for readability.
constexpr int J = 0, Q = 1, K = 2;

void test_terminal() {
    // Non-terminal: nothing decided yet, or a bet still faces a response.
    CHECK(!kuhn::is_terminal(""));
    CHECK(!kuhn::is_terminal("p"));
    CHECK(!kuhn::is_terminal("b"));
    CHECK(!kuhn::is_terminal("pb"));

    // Terminal: betting has closed.
    CHECK(kuhn::is_terminal("pp"));
    CHECK(kuhn::is_terminal("bp"));
    CHECK(kuhn::is_terminal("bb"));
    CHECK(kuhn::is_terminal("pbp"));
    CHECK(kuhn::is_terminal("pbb"));
}

void test_current_player() {
    CHECK(kuhn::current_player("") == 0);
    CHECK(kuhn::current_player("p") == 1);
    CHECK(kuhn::current_player("b") == 1);
    CHECK(kuhn::current_player("pb") == 0);
    CHECK(kuhn::current_player("pbp") == 1);
}

void test_payoff() {
    // Check-check showdown contests the antes (+/-1), from the actor's view.
    CHECK(kuhn::payoff("pp", K, J) == +1);
    CHECK(kuhn::payoff("pp", J, K) == -1);
    CHECK(kuhn::payoff("pp", Q, J) == +1);

    // Bet-call showdown contests one extra chip each (+/-2).
    CHECK(kuhn::payoff("bb", K, J) == +2);
    CHECK(kuhn::payoff("bb", J, K) == -2);
    CHECK(kuhn::payoff("pbb", K, Q) == +2);
    CHECK(kuhn::payoff("pbb", J, Q) == -2);

    // A fold gives the pot to the player to act (the bettor), regardless of cards.
    CHECK(kuhn::payoff("bp", J, K) == +1);
    CHECK(kuhn::payoff("bp", K, J) == +1);
    CHECK(kuhn::payoff("pbp", J, K) == +1);
}

void test_info_set_key() {
    CHECK(kuhn::info_set_key(Q, "pb") == "Qpb");
    CHECK(kuhn::info_set_key(K, "") == "K");
    CHECK(kuhn::info_set_key(J, "b") == "Jb");
}

void test_deals() {
    const auto d = kuhn::deals();
    CHECK(d.size() == 6);
    for (const auto& deal : d) {
        CHECK(deal[0] != deal[1]);  // the two private cards are distinct
    }
}

}  // namespace

int main() {
    test_terminal();
    test_current_player();
    test_payoff();
    test_info_set_key();
    test_deals();

    if (g_failures == 0) {
        std::cout << "all Kuhn model tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
