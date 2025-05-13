/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  Copyright Morten K. Schou
 */

/*
 * File:   DiffTest.cpp
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 13-05-2025.
 */

#include "DiffTest.h"
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include "../src/pdaaal-bin/parsing/Parsing.h"

namespace fs = std::filesystem;
namespace po = boost::program_options;
using namespace pdaaal;

template <typename instance_t>
void run(instance_t& instance) {
    auto [answers, weights] = diff_test(instance);
    if (has_different_elements(answers))
        std::cout << "Different answers: " << vector_printer() << answers << std::endl;
    if (has_different_elements(weights))
        std::cout << "Different weights: " << vector_printer() << weights << std::endl;
}


int main(int argc, const char** argv) {
    int n = 1;
    po::options_description opts;
    opts.add_options()("help,h", "produce help message")("repeat,n", po::value<int>(&n),
                                                         "Repetitions of running solvers on the instance. Default: 1.");
    parsing::Parsing parsing("Parsing Options");
    opts.add(parsing.options());
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, opts), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        return 1;
    }

    auto instance_variant = parsing.parse_instance<>();

    for (int i = 0; i < n; i++) {
        std::visit([](auto&& instance) { run(*instance); }, instance_variant);
    }
    return 0;
}
