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
 * File:   Generate.cpp
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 29-10-2021.
 */


#include <fstream>
#include <utility>
#include <string>
#include <random>
#include <iostream>
#include <boost/program_options.hpp>
#include <pdaaal/Solver.h>

#include "IsabellePrettyPrinter.h"

namespace po = boost::program_options;
using namespace pdaaal;


TypedPDA<char> generate_pda(size_t num_states, size_t num_labels, size_t num_rules, std::mt19937& random_gen, std::ostream& debug) {
    if (num_labels > 26) {
        throw std::logic_error("Too many labels specified. Change type in implementation, if you need this.");
    }
    pdaaal::TypedPDA<char> pda;
    std::vector<char> labels;
    labels.reserve(num_labels);
    for (size_t i = 0; i < num_labels; ++i) {
        char lbl = 'A' + i;
        pda.insert_label(lbl);
        labels.push_back(lbl);
    }
    assert(labels.size() == num_labels);
    auto num_ops = 1 + 2 * num_labels;
    auto all_rules = num_states * num_states * num_labels * num_ops;
    auto add_rule = [&pda,&labels,num_states,num_labels,num_ops,all_rules](size_t seed){
        auto rule_num = seed % all_rules;
        auto op_num = rule_num % num_ops;
        auto remain = rule_num / num_ops;
        auto pre = remain % num_labels;
        remain = remain / num_labels;
        auto to = remain % num_states;
        auto from = remain / num_states;
        assert(pre < num_labels);
        assert(from < num_states);
        assert(to < num_states);
        if (op_num == 0) {
            pda.add_rule(from, to, POP, '*', labels[pre]);
        } else {
            auto op = (op_num - 1) % 2 == 0 ? SWAP : PUSH;
            auto op_label = (op_num - 1) / 2;
            assert(op_label < num_labels);
            if (op == SWAP && op_label == pre) {
                op = NOOP;
            }
            pda.add_rule(from, to, op, labels[op_label], labels[pre]);
        }
        return rule_num;
    };

    std::uniform_int_distribution<size_t> distrib(0, all_rules-1);
    for (size_t i = 0; i < num_rules; ++i) {
        debug << add_rule(distrib(random_gen)) << std::endl;
    }
    return pda;
}

PAutomaton<> generate_pautomaton(const TypedPDA<char>& pda, size_t num_extra_states, size_t num_transitions, std::mt19937& random_gen, std::ostream& debug) {
    size_t num_states = pda.states().size();
    size_t num_labels = pda.number_of_labels();

    // TODO: Which parameters to use for randomly selecting accepting states.
    std::uniform_int_distribution<size_t> accept_init_distrib(0, 10);
    std::uniform_int_distribution<size_t> accept_extra_distrib(0, 3);

    std::vector<size_t> initially_accepting_states;
    for (size_t i = 0; i < num_states; ++i) {
        if (accept_init_distrib(random_gen) == 0) {
            initially_accepting_states.push_back(i);
        }
    }
    pdaaal::PAutomaton<> automaton(pda, initially_accepting_states, true);
    for (size_t i = 0; i < num_extra_states; ++i) {
        bool accepting = accept_extra_distrib(random_gen) == 0;
        automaton.add_state(false, accepting);
    }

    auto all_transitions = (num_states + num_extra_states) * num_extra_states * num_labels;
    auto add_transition = [&automaton,num_states,num_extra_states,num_labels,all_transitions](size_t seed){
        auto transition_num = seed % all_transitions;
        auto label = transition_num % num_labels;
        auto remain = transition_num / num_labels;
        auto to = (remain % num_extra_states) + num_states;
        auto from = remain / num_extra_states;
        automaton.add_edge(from, to, label);
        return transition_num;
    };

    std::uniform_int_distribution<size_t> distrib(0, all_transitions-1);
    for (size_t i = 0; i < num_transitions; ++i) {
        debug << add_transition(distrib(random_gen)) << std::endl;
    }
    return automaton;
}

void print_rules_simple(std::ostream& out, const TypedPDA<char>& pda) {
    auto rules = pda.all_rules();
    for (const auto& rule : rules) {
        out << "<p" << rule._from << ", " << rule._pre << "> --> <p" << rule._to << ", ";
        switch (rule._op) {
            case POP:
                break;
            case SWAP:
                out << rule._op_label;
                break;
            case NOOP:
                out << rule._pre;
                break;
            case PUSH:
                out << rule._op_label << " " << rule._pre;
                break;
            default:
                throw std::logic_error("error: Unknown op type.");
                break;
        }
        out << ">" << std::endl;
    }
    out << "Count rules: " << rules.size() << std::endl;
}

bool to_isabelle(std::ostream& out, const TypedPDA<char>& pda, PAutomaton<> initial_automaton, PAutomaton<> final_automaton) {
    IsabellePrettyPrinter isabelle_pp(out);
    isabelle_pp.print_begin();
    isabelle_pp.print_query(pda, initial_automaton, final_automaton);
    isabelle_pp.print_proofs();
    PAutomatonProduct instance(pda, std::move(initial_automaton), std::move(final_automaton));
    bool answer = Solver::pre_star_accepts(instance);
    isabelle_pp.print_lemma(answer);
    isabelle_pp.print_end();
    return answer;
}
bool solve(const TypedPDA<char>& pda, PAutomaton<> initial_automaton, PAutomaton<> final_automaton) {
    PAutomatonProduct instance(pda, std::move(initial_automaton), std::move(final_automaton));
    bool answer = Solver::pre_star_accepts(instance);
    return answer;
}

void generate(std::ostream& out, std::mt19937& random_gen, bool debug_info
#ifndef NDEBUG
        = true
#else
        = false
#endif
        ) {
    std::stringstream dummy;

    // Generate
    auto pda = generate_pda(4, 5, 20, random_gen, debug_info ? out : dummy);
    auto initial_automaton = generate_pautomaton(pda, 3, 8, random_gen, debug_info ? out : dummy);
    auto final_automaton = generate_pautomaton(pda, 2, 5, random_gen, debug_info ? out : dummy);

    // Print in Isabelle format (also calculates answer).
    bool answer = to_isabelle(out, pda, initial_automaton, final_automaton);

    // Variables for statistics
    size_t count_p = 0, count_n = 0, count_already_intersecting = 0;

    // Get statistics
    PAutomatonProduct instance(pda, std::move(initial_automaton), std::move(final_automaton));
    instance.enable_pre_star();
    if (instance.initialize_product()) { count_already_intersecting++; }
    if (answer) { count_p++; } else { count_n++; }
}

void generate_many(std::mt19937& random_gen) {
    std::stringstream dummy;

    // Variables for statistics
    size_t count_p = 0, count_n = 0, count_already_intersecting = 0;

    for (size_t i = 0; i < 1000; ++i) {
        // Generate
        auto pda = generate_pda(4, 5, 20, random_gen, dummy);
        auto initial_automaton = generate_pautomaton(pda, 3, 8, random_gen, dummy);
        auto final_automaton = generate_pautomaton(pda, 2, 5, random_gen, dummy);
        
        bool answer = solve(pda, initial_automaton, final_automaton);

        // Get statistics
        PAutomatonProduct instance(pda, std::move(initial_automaton), std::move(final_automaton));
        instance.enable_pre_star();
        if (instance.initialize_product()) { count_already_intersecting++; }
        if (answer) { count_p++; } else { count_n++; }
    }

    std::cout << "Positive: " << count_p << std::endl
              << "Negative: " << count_n << std::endl
              << "Trivial: " << count_already_intersecting << std::endl;
}


int main(int argc, const char** argv) {

    po::options_description opts;
    opts.add_options()
            ("help,h", "produce help message");
//            ("version,v", "print version");

    po::options_description input("Input Options");
    po::options_description output("Output Options");
    size_t seed = std::random_device()(); // Default to a random seed. Overwritten if -s option is set.
    input.add_options()
            ("seed,s", po::value<size_t>(&seed), "Seed for random number generator (use a random_device if not set)")
            ;
//    bool no_parser_warnings = false;
//    bool silent = false;
    std::string output_file;
    output.add_options()
//            ("disable-parser-warnings,W", po::bool_switch(&no_parser_warnings), "Disable warnings from parser.")
//            ("silent,s", po::bool_switch(&silent), "Disables non-essential output (implies -W).")
            ("output,o", po::value<std::string>(&output_file), "Output file (default is standard out).")
            ;
    opts.add(input);
    opts.add(output);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, opts), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        return 1;
    }

    std::mt19937 random_gen(seed);

    if (output_file.empty() || output_file == "-") {
//        generate(std::cout, random_gen);
        generate_many(random_gen);
    } else {
        std::ofstream out_stream(output_file);
        if (!out_stream.is_open()) {
            std::stringstream es;
            es << "error: Could not open file: " << output_file << std::endl;
            throw std::runtime_error(es.str());
        }
        generate(out_stream, random_gen);
    }

    return 0;
}