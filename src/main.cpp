// Trains CFR on Kuhn poker and prints the average strategy and game value.

#include "poker_solver/cfr.hpp"
#include "poker_solver/kuhn.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>

namespace kuhn = poker_solver::kuhn;

int main(int argc, char** argv) {
    int iterations = (argc > 1) ? std::atoi(argv[1]) : 100000;
    if (iterations <= 0) iterations = 100000;

    poker_solver::cfr::Trainer trainer;
    const double value = trainer.train(iterations);

    std::cout << "poker-solver — Kuhn CFR\n";
    std::cout << "iterations: " << iterations << '\n';
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "game value to player 0: " << value
              << "  (theory: -1/18 = -0.0556)\n\n";

    std::cout << "average strategy        pass     bet\n";
    for (const auto& [key, node] : trainer.nodes()) {
        const auto avg = node.average_strategy();
        std::cout << "  " << std::left << std::setw(6) << key << std::right
                  << std::setw(10) << avg[kuhn::kPass]
                  << std::setw(8) << avg[kuhn::kBet] << '\n';
    }
    return 0;
}
