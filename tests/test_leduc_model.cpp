// Unit tests for the Leduc Hold'em game model.

#include "poker_solver/leduc.hpp"

#include <iostream>
#include <string>

namespace leduc = poker_solver::leduc;

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

constexpr int J = 0, Q = 1, K = 2;

void test_terminal_and_chance() {
    // Mid-round-1: not terminal, not yet a chance node.
    CHECK(!leduc::is_terminal(""));
    CHECK(!leduc::is_terminal("r"));
    CHECK(!leduc::is_terminal("rc"));     // round 1 closed -> chance node, not terminal

    // Chance node: round 1 closed by call/check, public card pending.
    CHECK(leduc::needs_public_card("rc"));
    CHECK(leduc::needs_public_card("cc"));
    CHECK(leduc::needs_public_card("crrc"));
    CHECK(!leduc::needs_public_card("r"));   // round still open
    CHECK(!leduc::needs_public_card("rf"));  // folded -> terminal, not chance
    CHECK(!leduc::needs_public_card("rc/Q")); // public already dealt

    // Folds end the hand immediately, in either round.
    CHECK(leduc::is_terminal("rf"));
    CHECK(leduc::is_terminal("crf"));
    CHECK(leduc::is_terminal("rc/Qrf"));

    // Round 2 closes -> showdown terminal.
    CHECK(leduc::is_terminal("rc/Qcc"));
    CHECK(leduc::is_terminal("cc/Krc"));
    CHECK(!leduc::is_terminal("rc/Qr"));  // round 2 still open
}

void test_player_and_legal() {
    CHECK(leduc::current_player("") == 0);
    CHECK(leduc::current_player("r") == 1);
    CHECK(leduc::current_player("rr") == 0);
    CHECK(leduc::current_player("rc/Q") == 0);   // player 0 leads round 2
    CHECK(leduc::current_player("rc/Qr") == 1);

    // Facing a check: may check or raise, but not fold.
    auto l0 = leduc::legal_actions("");
    CHECK(l0[leduc::kCall] && l0[leduc::kRaise] && !l0[leduc::kFold]);

    // Facing a bet: may fold, call, or raise.
    auto l1 = leduc::legal_actions("r");
    CHECK(l1[leduc::kFold] && l1[leduc::kCall] && l1[leduc::kRaise]);

    // Raise cap reached (two raises): may fold or call, not raise.
    auto l2 = leduc::legal_actions("rr");
    CHECK(l2[leduc::kFold] && l2[leduc::kCall] && !l2[leduc::kRaise]);
}

void test_contributions() {
    // Bet (2) then fold: bettor in for ante+2, folder for ante only.
    CHECK(leduc::contributions("rf")[0] == 3);
    CHECK(leduc::contributions("rf")[1] == 1);

    // Bet-call round 1 (2 each), checks round 2: ante+2 each.
    CHECK(leduc::contributions("rc/Qcc")[0] == 3);
    CHECK(leduc::contributions("rc/Qcc")[1] == 3);

    // Bet-call round 1 (2), bet-call round 2 (4): ante+2+4 each.
    CHECK(leduc::contributions("rc/Qrc")[0] == 7);
    CHECK(leduc::contributions("rc/Qrc")[1] == 7);

    // Two raises in round 1 (to 4 each): ante+4.
    CHECK(leduc::contributions("crrc")[0] == 5);
    CHECK(leduc::contributions("crrc")[1] == 5);
}

void test_payoffs() {
    // Fold: opponent of the folder wins the folder's committed chips.
    CHECK(leduc::terminal_utility_p0("rf", J, K) == +1.0);   // P1 folded
    CHECK(leduc::terminal_utility_p0("crf", K, J) == -1.0);  // P0 folded

    // Showdown, no pair: higher rank wins the opponent's contribution.
    CHECK(leduc::terminal_utility_p0("rc/Qcc", K, J) == +3.0);  // K beats J
    CHECK(leduc::terminal_utility_p0("rc/Qcc", J, K) == -3.0);

    // Showdown with a pair beats a higher unpaired card.
    CHECK(leduc::terminal_utility_p0("rc/Qcc", Q, K) == +3.0);  // pair of Q beats K
    CHECK(leduc::terminal_utility_p0("rc/Qcc", J, Q) == -3.0);  // P1 pairs Q

    // Equal ranks, no pair: split (zero).
    CHECK(leduc::terminal_utility_p0("rc/Jcc", K, K) == 0.0);
}

void test_info_set_key() {
    CHECK(leduc::info_set_key(Q, "r") == "Qr");
    CHECK(leduc::info_set_key(K, "rc/Qr") == "Krc/Qr");
}

}  // namespace

int main() {
    test_terminal_and_chance();
    test_player_and_legal();
    test_contributions();
    test_payoffs();
    test_info_set_key();

    if (g_failures == 0) {
        std::cout << "all Leduc model tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
}
