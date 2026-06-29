// Trains CFR on Kuhn poker: shows exploitability shrinking with iterations, then
// the converged average strategy and game value.

#include "poker_solver/cfr.hpp"
#include "poker_solver/exploitability.hpp"
#include "poker_solver/kuhn.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace kuhn = poker_solver::kuhn;
namespace cfr = poker_solver::cfr;
namespace exploit = poker_solver::exploit;

int main(int argc, char** argv) {
    int iterations = (argc > 1) ? std::atoi(argv[1]) : 1000000;
    if (iterations <= 0) iterations = 1000000;

    std::cout << "poker-solver — Kuhn CFR\n\n";

    // Convergence: exploitability -> 0 as iterations grow.
    std::cout << "iterations   game value   exploitability\n";
    for (int n : {100, 1000, 10000, 100000, 1000000}) {
        cfr::Trainer t;
        const double value = t.train(n);
        const double eps = exploit::exploitability(t);
        std::cout << std::setw(10) << n << "   " << std::showpos << std::fixed
                  << std::setprecision(5) << std::setw(9) << value << std::noshowpos
                  << "   " << std::setprecision(6) << std::setw(10) << eps << '\n';
    }

    // Detailed converged strategy at the requested iteration count.
    cfr::Trainer trainer;
    const double value = trainer.train(iterations);
    std::cout << "\nconverged at " << iterations << " iterations  (value "
              << std::showpos << std::setprecision(5) << value << std::noshowpos
              << ", theory -0.05556)\n";
    std::cout << "average strategy        pass     bet\n";
    for (const auto& [key, node] : trainer.nodes()) {
        const auto avg = node.average_strategy();
        std::cout << "  " << std::left << std::setw(6) << key << std::right
                  << std::setw(10) << std::setprecision(4) << avg[kuhn::kPass]
                  << std::setw(8) << avg[kuhn::kBet] << '\n';
    }
    return 0;
}
