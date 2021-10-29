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
#include <pdaaal/PAutomaton.h>

namespace po = boost::program_options;
using namespace pdaaal;


TypedPDA<char> generate_pda(size_t num_states, size_t num_labels, size_t num_rules, std::ostream& debug) {
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

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> distrib(0, all_rules-1);
    for (size_t i = 0; i < num_rules; ++i) {
        debug << add_rule(distrib(gen)) << std::endl;
    }
    return pda;
}

PAutomaton<> generate_pautomaton(const TypedPDA<char>& pda, size_t num_extra_states, size_t num_transitions, std::ostream& debug) {
    size_t num_states = pda.states().size();
    size_t num_labels = pda.number_of_labels();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> accept_distrib(0, 3);

    std::vector<size_t> initially_accepting_states;
    // TODO: Randomly generate initially_accepting_states
    pdaaal::PAutomaton<> automaton(pda, initially_accepting_states, true);

    for (size_t i = 0; i < num_extra_states; ++i) {
        bool accepting = accept_distrib(gen) == 0; // TODO: Which of extra_states should be accepting???
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
        debug << add_transition(distrib(gen)) << std::endl;
    }
    return automaton;
}

void print_rules(std::ostream& out, const TypedPDA<char>& pda) {
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

template <typename T, typename Fn = std::function<T(T)>>
std::ostream& print_list(std::ostream& out, const std::string& prefix, const std::string& separator, std::vector<T> list, Fn&& map = [](const T& t){ return t; }) {
    bool first = true;
    for (const auto& elem : list) {
        if (first) {
            first = false;
        } else {
            out << separator;
        }
        out << prefix << map(elem);
    }
    return out;
}

std::ostream& print_automaton_state(std::ostream& out, size_t state, const PAutomaton<>& automaton) {
    if (state < automaton.pda().states().size()) {
        out << "Ctr_Loc p" << state;
    } else {
        out << "Ctr_Loc_St q" << state;
    }
    return out;
}

std::ostream& print_rules(std::ostream& out, const std::string& name, const TypedPDA<char>& pda){
    out << "definition " << name << " :: \"(ctr_loc, label) rule set\" where" << std::endl
        << "  \"" << name << " = {" << std::endl;
    bool first_rule = true;
    for (const auto& rule : pda.all_rules()) {
        if (first_rule) {
            first_rule = false;
        } else {
            out << "," << std::endl;
        }
        out << "  ((p" << rule._from << ", " << rule._pre << "), (p" << rule._to << ", ";
        switch (rule._op) {
            case POP:
                out << "pop";
                break;
            case SWAP:
                out << "swap " << rule._op_label;
                break;
            case NOOP:
                out << "swap " << rule._pre;
                break;
            case PUSH:
                out << "push " << rule._op_label << " " << rule._pre;
                break;
            default:
                throw std::logic_error("error: Unknown op type.");
                break;
        }
        out << "))";
    }
    out << "}\"" << std::endl;
    return out;
}

std::ostream& print_automaton(std::ostream& out, const std::string& name, const PAutomaton<>& automaton, const TypedPDA<char>& pda) {
    out << "definition " << name << " :: \"((ctr_loc, state, label) PDS.state, label) transition set\" where" << std::endl
        << "  \"" << name << " = {" << std::endl;
    bool first = true;
    std::vector<size_t> accepting_states;
    std::vector<size_t> accepting_extra_states;
    for (const auto& state : automaton.states()) {
        auto from = state->_id;
        if (state->_accepting) {
            if (from < pda.states().size()) {
                accepting_states.push_back(from);
            } else {
                accepting_extra_states.push_back(from);
            }
        }
        for (const auto& [to, labels] : state->_edges) {
            for (const auto& label : labels) {
                if (first) {
                    first = false;
                } else {
                    out << "," << std::endl;
                }
                out << "  ((";
                print_automaton_state(out, from, automaton) << ", " << pda.get_symbol(label.first) << ", ";
                print_automaton_state(out, to, automaton) << "))";
            }
        }
    }
    out << "}\"" << std::endl
        << "definition " << name << "_F_ctr_loc where \"" << name << "_F_ctr_loc = {";
    print_list(out, "p", ",", accepting_states) << "}\"" << std::endl
        << "definition " << name << "_F_ctr_loc_st where \"" << name << "_F_ctr_loc_st = {";
    print_list(out, "q", ",", accepting_extra_states) << "}\"" << std::endl;
    return out;
}

void print_to_isabelle(std::ostream& out, const TypedPDA<char>& pda, const PAutomaton<>& initial_automaton, const PAutomaton<>& final_automaton) {
    size_t num_states = pda.states().size();
    size_t num_labels = pda.number_of_labels();
    std::vector<size_t> states(num_states);
    std::iota(states.begin(), states.end(), 0);
    std::vector<char> labels(num_labels);
    for (size_t label_id = 0; label_id < num_labels; ++label_id) {
        labels[label_id] = pda.get_symbol(label_id);
    }
    std::vector<size_t> extra_states;
    for (const auto& state : initial_automaton.states()) {
        if (state->_id >= num_states) {
            extra_states.push_back(state->_id);
        }
    }
    for (const auto& state : final_automaton.states()) {
        if (state->_id >= num_states) {
            extra_states.push_back(state->_id);
        }
    }
    // Remove duplicates
    std::sort(extra_states.begin(), extra_states.end());
    extra_states.erase(std::unique(extra_states.begin(), extra_states.end()), extra_states.end());

    out << "(* Query specific part START *)" << std::endl
        << std::endl
        << "(* List all control locations (in PDS), labels, and non-initial states in both P-automata *)" << std::endl
        << "datatype ctr_loc = ";
    print_list(out, "p", " | ", states) << std::endl;
    out << "definition ctr_loc_list where \"ctr_loc_list = [";
    print_list(out, "p", ",", states) << "]\"" << std::endl;
    out << "datatype label = ";
    print_list(out, "", " | ", labels) << std::endl;
    out << "definition label_list where \"label_list = [";
    print_list(out, "", ",", labels) << "]\"" << std::endl;
    out << "datatype state = ";
    print_list(out, "q", " | ", extra_states) << std::endl;
    out << "definition state_list where \"state_list = [";
    print_list(out, "q", ",", extra_states) << "]\"" << std::endl;
    out << std::endl
        << "(* Define rules of PDS, and the two P-automata *)" << std::endl;
    print_rules(out, "pds_rules", pda);
    print_automaton(out, "initial_automaton", initial_automaton, pda);
    print_automaton(out, "final_automaton", final_automaton, pda);
    out << std::endl
        << "(* Query specific part END *)" << std::endl;
}

void generate(std::ostream& out, bool debug_info = false) {
    out << "Test Generate" << std::endl;
    std::stringstream dummy;
#ifndef NDEBUG
    debug_info = true;
#endif
    auto pda = generate_pda(4, 5, 20, debug_info ? out : dummy);
    print_rules(out, pda);

    auto initial_automaton = generate_pautomaton(pda, 3, 8, debug_info ? out : dummy);
    auto final_automaton = generate_pautomaton(pda, 2, 5, debug_info ? out : dummy);

    print_to_isabelle(out, pda, initial_automaton, final_automaton);
}

int main(int argc, const char** argv) {

    po::options_description opts;
    opts.add_options()
            ("help,h", "produce help message");
//            ("version,v", "print version");

    po::options_description output("Output Options");

//    bool no_parser_warnings = false;
//    bool silent = false;
    std::string output_file;
    output.add_options()
//            ("disable-parser-warnings,W", po::bool_switch(&no_parser_warnings), "Disable warnings from parser.")
//            ("silent,s", po::bool_switch(&silent), "Disables non-essential output (implies -W).")
            ("output,o", po::value<std::string>(&output_file), "Output file (default is standard out).")
            ;
    opts.add(output);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, opts), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        return 1;
    }
    if (output_file.empty() || output_file == "-") {
        generate(std::cout);
    } else {
        std::ofstream out_stream(output_file);
        if (!out_stream.is_open()) {
            std::stringstream es;
            es << "error: Could not open file: " << output_file << std::endl;
            throw std::runtime_error(es.str());
        }
        generate(out_stream);
    }

    return 0;
}