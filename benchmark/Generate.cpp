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
#include <filesystem>
#include <boost/program_options.hpp>

#include "DiffTest.h"
#include "IsabellePrettyPrinter.h"

namespace fs = std::filesystem;
namespace po = boost::program_options;

template<bool with_weight>
using generated_pda_t = PDA<std::string,std::conditional_t<with_weight,weight<uint32_t>,weight<void>>,fut::type::vector,std::string>;
template<bool with_weight>
using generated_automaton_t = decltype(PAutomaton(std::declval<generated_pda_t<with_weight>>(), std::declval<std::vector<size_t>>(), true));

template<bool with_weight = false>
generated_pda_t<with_weight> generate_pda(size_t num_states, size_t num_labels, size_t num_rules, std::mt19937& random_gen, std::ostream& debug, size_t num_weights = 0) {
    if constexpr (!with_weight) {
        num_weights = 0;
    }
    std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
    if (num_labels >= alphabet.size()) {
        throw std::logic_error("Too many labels specified. Change implementation, if you need this.");
    }
    generated_pda_t<with_weight> pda;
    std::vector<std::string> labels;
    labels.reserve(num_labels);
    for (size_t i = 0; i < num_labels; ++i) {
        std::string lbl = alphabet.substr(i,1);
        pda.insert_label(lbl);
        labels.push_back(lbl);
    }
    for (size_t i = 0; i < num_states; ++i) {
        std::stringstream state_name; state_name << "p" << i;
        pda.insert_state(state_name.str());
    }
    assert(labels.size() == num_labels);
    auto num_ops = 1 + 2 * num_labels;
    auto all_rules = num_states * num_states * num_labels * num_ops * (num_weights == 0 ? 1 : num_weights);
    auto add_rule = [&pda,&labels,num_states,num_labels,num_ops,num_weights,all_rules](size_t seed){
        auto rule_num = seed % all_rules;
        auto op_num = rule_num % num_ops;
        auto remain = rule_num / num_ops;
        auto pre = remain % num_labels;
        remain = remain / num_labels;
        auto to = remain % num_states;
        remain = remain / num_states;
        auto from = remain % num_states;
        auto weight = remain / num_states;
        assert(pre < num_labels);
        assert(from < num_states);
        assert(to < num_states);
        assert((num_weights == 0) ? weight <= num_weights : weight < num_weights);
        if (op_num == 0) {
            if constexpr (with_weight) {
                pda.add_rule(from, to, POP, "", labels[pre], (uint32_t)weight);
            } else {
                pda.add_rule(from, to, POP, "", labels[pre]);
            }
        } else {
            auto op = (op_num - 1) % 2 == 0 ? SWAP : PUSH;
            auto op_label = (op_num - 1) / 2;
            assert(op_label < num_labels);
            if (op == SWAP && op_label == pre) {
                op = NOOP;
            }
            if constexpr (with_weight) {
                pda.add_rule(from, to, op, labels[op_label], labels[pre], (uint32_t)weight);
            } else {
                pda.add_rule(from, to, op, labels[op_label], labels[pre]);
            }
        }
        return rule_num;
    };

    std::uniform_int_distribution<size_t> distrib(0, all_rules-1);
    for (size_t i = 0; i < num_rules; ++i) {
        debug << add_rule(distrib(random_gen)) << std::endl;
    }
    return pda;
}

template<bool with_weight = false>
generated_automaton_t<with_weight> generate_pautomaton(const generated_pda_t<with_weight>& pda, size_t num_extra_states, size_t num_transitions, std::mt19937& random_gen, std::ostream& debug) {
    size_t num_states = pda.states().size();
    size_t num_labels = pda.number_of_labels();

    // TODO: Which parameters to use for randomly selecting accepting states.
    std::uniform_int_distribution<size_t> accept_init_distrib(0, 4);
    std::uniform_int_distribution<size_t> accept_extra_distrib(0, 3);

    std::vector<size_t> initially_accepting_states;
    for (size_t i = 0; i < num_states; ++i) {
        if (accept_init_distrib(random_gen) == 0) {
            initially_accepting_states.push_back(i);
        }
    }
    generated_automaton_t<with_weight> automaton(pda, initially_accepting_states, true);
    for (size_t i = 0; i < num_extra_states; ++i) {
        bool accepting = accept_extra_distrib(random_gen) <= 1;
        automaton.add_state(false, accepting);
        std::stringstream state_name; state_name << "q" << i;
        automaton.insert_state(state_name.str());
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
    automaton.remove_redundant();
    return automaton;
}

void print_rules_simple(std::ostream& out, const PDA<char>& pda) {
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

// template<typename pda_t>
// bool to_isabelle(std::ostream& out, const pda_t& pda, pda_to_pautomaton_t<pda_t> initial_automaton, pda_to_pautomaton_t<pda_t> final_automaton) {
//     IsabellePrettyPrinter isabelle_pp(out);
//     isabelle_pp.print_begin();
//     isabelle_pp.print_query(pda, initial_automaton, final_automaton);
//     isabelle_pp.print_proofs();
//     PAutomatonProduct instance(pda, std::move(initial_automaton), std::move(final_automaton));
//     bool answer = Solver::pre_star_accepts(instance);
//     isabelle_pp.print_lemma(answer);
//     isabelle_pp.print_end();
//     return answer;
// }

template<typename instance_t>
json get_json(const instance_t& instance) {
    json result;
    result["instance"] = instance;
    return result;
}



// void generate(std::ostream& out, std::mt19937& random_gen, bool debug_info
// #ifndef NDEBUG
//         = true
// #else
//         = false
// #endif
//         ) {
//     std::stringstream dummy;
//
//     // Generate
//     auto pda = generate_pda(2, 2, 2, random_gen, debug_info ? out : dummy);
//     auto initial_automaton = generate_pautomaton(pda, 2, 2, random_gen, debug_info ? out : dummy);
//     auto final_automaton = generate_pautomaton(pda, 1, 2, random_gen, debug_info ? out : dummy);
//
//     // Print in Isabelle format (also calculates answer).
//     bool answer = to_isabelle(out, pda, initial_automaton, final_automaton);
//
//     // Variables for statistics
//     size_t count_p = 0, count_n = 0, count_already_intersecting = 0;
//
//     // Get statistics
//     PAutomatonProduct instance(pda, std::move(initial_automaton), std::move(final_automaton));
//     instance.enable_pre_star();
//     if (instance.initialize_product()) { count_already_intersecting++; }
//     if (answer) { count_p++; } else { count_n++; }
// }

void print_json(const json& j, const fs::path& output_dir, const std::string& name, size_t index) {
    std::stringstream file_name;
    file_name << name << index << ".json";
    auto file_path = output_dir / file_name.str();
    std::ofstream out_stream(file_path);
    if (!out_stream.is_open()) {
        std::stringstream es;
        es << "error: Could not open file: " << file_path << std::endl;
        throw std::runtime_error(es.str());
    }
    out_stream << j.dump() << std::endl;
}

template<bool with_weight = false>
void generate_many(std::mt19937& random_gen, const fs::path& output_dir, size_t number_of_instances) {
    std::stringstream dummy;

    // Variables for statistics
    size_t count_p = 0, count_n = 0, count_already_intersecting = 0;
    std::unordered_map<uint32_t,size_t> weight_counts;

    for (size_t i = 0; i < number_of_instances; ++i) {
        // Generate
        auto pda = generate_pda<with_weight>(4, 5, i%200, random_gen, dummy, i%19);
        auto initial_automaton = generate_pautomaton<with_weight>(pda, 3, i%13, random_gen, dummy);
        auto final_automaton = generate_pautomaton<with_weight>(pda, 2, i%11, random_gen, dummy);
        PAutomatonProduct instance(pda, std::move(initial_automaton), std::move(final_automaton));

        print_json(get_json(instance), output_dir, "instance", i);
//        print_json(pda.to_json(), output_dir, "pda", i);
//        print_json(initial_automaton.to_json(), output_dir, "initial", i);
//        print_json(final_automaton.to_json(), output_dir, "final", i);

        auto [answers, weights] = diff_test(instance);
        if (has_different_elements(answers)) std::cout << "WHOOPS: Different answers on case " << i << ". " << vector_printer() << answers << std::endl;
        if (has_different_elements(weights)) std::cout << "WHOOPS: Different weights on case " << i << ". " << vector_printer() << weights << std::endl;

        // Get statistics
        bool answer = solve(instance, engine_t::prestar);
        if constexpr (with_weight) {
            auto weight = solve_w(instance, engine_t::prestar);
            if (weight.has_value()) ++weight_counts[weight.value()];
        }

        instance.enable_pre_star();
        if (instance.initialize_product()) { count_already_intersecting++; }
        if (answer) { count_p++; } else { count_n++; }
    }

    std::cout << "Positive: " << count_p << std::endl
              << "Negative: " << count_n << std::endl
              << "Trivially positive: " << count_already_intersecting << std::endl;
    for (const auto& [weight, count] : pdaaal::fut::vector_map<uint32_t,size_t>(std::move(weight_counts))) {
        std::cout << weight << ": " << count << std::endl;
    }
}

template<bool with_weight = false>
void generate_pda_without_symmetries(const fs::path& output_dir) {
    // Parameters:
    size_t num_states = 2;
    size_t num_labels = 2;
    size_t num_weights = 2; // if constexpr (with_weight) then we use weights 1 and 2. (else unweighted)
    std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVXYZ";
    bool do_reduction = true;

    // Initialize:
    if (num_labels > 26) { throw std::logic_error("Too many labels specified. Change type in implementation, if you need this."); }
    std::vector<std::string> labels;
    std::unordered_set<std::string> all_labels;
    labels.reserve(num_labels);
    for (size_t i = 0; i < num_labels; ++i) {
        std::string lbl = alphabet.substr(i,1);
        labels.push_back(lbl);
        all_labels.emplace(lbl);
    }
    assert(labels.size() == num_labels);
    auto num_ops = 1 + 2 * num_labels;
    auto all_rules = num_states * num_states * num_labels * num_ops * (with_weight ? num_weights : 1);
    auto seed_to_rule = [num_states,num_labels,num_ops,num_weights,all_rules](size_t seed) -> std::tuple<size_t,size_t,op_t,std::optional<size_t>,size_t,uint32_t> {
        auto rule_num = seed % all_rules;
        auto op_num = rule_num % num_ops;
        auto remain = rule_num / num_ops;
        auto pre = remain % num_labels;
        remain = remain / num_labels;
        auto to = remain % num_states;
        remain = remain / num_states;
        auto from = remain % num_states;
        auto weight = (uint32_t)(1 + remain / num_states); // using weights 1 or 2.
        assert(pre < num_labels);
        assert(from < num_states);
        assert(to < num_states);
        if (op_num == 0) {
            return std::tuple(from, to, POP, std::nullopt, pre, weight);
        } else {
            auto op = (op_num - 1) % 2 == 0 ? SWAP : PUSH;
            auto op_label = (op_num - 1) / 2;
            assert(op_label < num_labels);
            return std::tuple(from, to, op, op_label, pre, weight);
        }
    };
    auto rule_to_seed = [num_states,num_labels,num_ops](size_t from, size_t to, op_t op, std::optional<size_t> op_label, size_t pre, uint32_t weight) -> size_t {
        size_t op_num = 0;
        if (op != POP) {
            assert(op_label);
            op_num = op_label.value()*2 + 1;
            if (op == PUSH) {
                op_num++;
            }
        }
        return ((((weight - 1) * num_states + from) * num_states + to) * num_labels + pre) * num_ops + op_num;
    };
    auto add_rule = [&labels,&seed_to_rule](size_t seed, generated_pda_t<with_weight>& pda){
        auto [from, to, op, op_label, pre, weight] = seed_to_rule(seed);
        if (op == SWAP && op_label && op_label.value() == pre) {
            op = NOOP;
        }
        if constexpr (with_weight) {
            pda.add_rule(from, to, op, op_label ? labels[op_label.value()] : "", labels[pre], weight);
        } else {
            pda.add_rule(from, to, op, op_label ? labels[op_label.value()] : "", labels[pre]);
        }
    };
    for (size_t i = 0; i < all_rules; ++i) {
        auto [from, to, op, op_label, pre, weight] = seed_to_rule(i);
        if(i != rule_to_seed(from, to, op, op_label, pre, weight)) throw std::logic_error("Error in rule<-->seed conversion!!");
    }
    auto swap_0_1 = [](size_t n) -> size_t {
        if (n == 0) {
            return 1;
        } else {
            return 0;
        }
    };
    auto rule_swap_state = [&seed_to_rule,&rule_to_seed,&swap_0_1](size_t seed) -> size_t {
        auto [from, to, op, op_label, pre, weight] = seed_to_rule(seed);
        return rule_to_seed(swap_0_1(from), swap_0_1(to), op, op_label, pre, weight);
    };
    auto rule_swap_label = [&seed_to_rule,&rule_to_seed,&swap_0_1](size_t seed) -> size_t {
        auto [from, to, op, op_label, pre, weight] = seed_to_rule(seed);
        return rule_to_seed(from, to, op, op_label ? std::make_optional(swap_0_1(op_label.value())) : op_label, swap_0_1(pre), weight);
    };
    auto rule_swap_both = [&seed_to_rule,&rule_to_seed,&swap_0_1](size_t seed) -> size_t {
        auto [from, to, op, op_label, pre, weight] = seed_to_rule(seed);
        return rule_to_seed(swap_0_1(from), swap_0_1(to), op, op_label ? std::make_optional(swap_0_1(op_label.value())) : op_label, swap_0_1(pre), weight);
    };

//    std::vector<TypedPDA<char>> pdas;
    std::unordered_set<std::vector<size_t>, absl::Hash<std::vector<size_t>>> pda_set;
    size_t count_pdas = 0;

    for (size_t num_rules = 0; num_rules <= 2; ++num_rules) {
        std::vector<bool> rule_selector(all_rules);
        std::fill(rule_selector.begin(), rule_selector.begin() + num_rules, true);
        do {
            std::vector<size_t> rules;
            for (size_t i = 0; i < all_rules; ++i) {
                if (rule_selector[i]) {
                    rules.push_back(i);
                }
            }

            if (do_reduction) {
                std::vector<size_t> swapped_state;
                std::vector<size_t> swapped_label;
                std::vector<size_t> swapped_both;
                std::transform(rules.begin(), rules.end(), std::back_inserter(swapped_state), rule_swap_state);
                std::transform(rules.begin(), rules.end(), std::back_inserter(swapped_label), rule_swap_label);
                std::transform(rules.begin(), rules.end(), std::back_inserter(swapped_both), rule_swap_both);
                if (pda_set.find(swapped_state) != pda_set.end() ||
                    pda_set.find(swapped_label) != pda_set.end() ||
                    pda_set.find(swapped_both) != pda_set.end()) {
                    continue;
                }
                pda_set.emplace(rules);
            }

            generated_pda_t<with_weight> pda(all_labels);
            pda.insert_state("p0");
            pda.insert_state("p1");
            for (size_t rule : rules) {
                add_rule(rule, pda);
            }
            std::stringstream name_with_index;
            name_with_index << "pds_rules_" << count_pdas;
            {
                std::stringstream file_name;
                file_name << "pda" << count_pdas << ".thy";
                auto file_path = output_dir / file_name.str();
                std::ofstream out_stream(file_path);
                if (!out_stream.is_open()) {
                    std::stringstream es;
                    es << "error: Could not open file: " << file_path << std::endl;
                    throw std::runtime_error(es.str());
                }
                IsabellePrettyPrinter isabelle_pp(out_stream);
                isabelle_pp.print_rules(name_with_index.str(), pda, "l");
            }
            {
                std::stringstream file_name;
                file_name << "pda" << count_pdas << ".json";
                auto file_path = output_dir / file_name.str();
                std::ofstream out_stream(file_path);
                if (!out_stream.is_open()) {
                    std::stringstream es;
                    es << "error: Could not open file: " << file_path << std::endl;
                    throw std::runtime_error(es.str());
                }
                out_stream << pda.to_json().dump() << std::endl;
            }
//            pdas.emplace_back(std::move(pda));
            count_pdas++;
        } while (std::prev_permutation(rule_selector.begin(), rule_selector.end()));
    }
    std::cout << "Num PDAs: " << count_pdas << std::endl;

}

template<bool with_weight = false>
void generate_pautomata_without_symmetries(const fs::path& output_dir, bool initial) {
    generated_pda_t<with_weight> pda;
    pda.insert_label("A");
    pda.insert_label("B");
    pda.insert_state("p0");
    pda.insert_state("p1");
    pda.add_rule(0, 1, POP, "", "A"); // We just need to have two labels and two states for now.
    assert(pda.states().size() == 2);
    assert(pda.number_of_labels() == 2);

    size_t num_states = pda.states().size();
    size_t num_labels = pda.number_of_labels();
    size_t num_extra_states = initial ? 2 : 1;
    std::string name = initial ? "initial" : "final";
    bool do_reduction = true;

    auto all_transitions = (num_states + num_extra_states) * num_extra_states * num_labels;
    auto seed_to_trans = [num_states,num_extra_states,num_labels,all_transitions](size_t seed){
        auto transition_num = seed % all_transitions;
        auto label = transition_num % num_labels;
        auto remain = transition_num / num_labels;
        auto to = (remain % num_extra_states) + num_states;
        auto from = remain / num_extra_states;
        return std::tuple(from, to, label);
    };
    auto trans_to_seed = [num_states,num_extra_states,num_labels,all_transitions](size_t from, size_t to, size_t label){
        return (from * num_extra_states + (to - num_states)) * num_labels + label;
    };
    auto add_transition = [&seed_to_trans](size_t seed, pda_to_pautomaton_t<generated_pda_t<with_weight>>& automaton){
        auto [from, to, label] = seed_to_trans(seed);
        automaton.add_edge(from, to, label);
    };
    for (size_t i = 0; i < all_transitions; ++i) {
        auto [from, to, label] = seed_to_trans(i);
        assert(i == trans_to_seed(from, to, label));
    }

    auto swap_2_extra_states = [num_states,num_extra_states](size_t state) -> size_t {
        if (num_extra_states == 2) {
            if (state == num_states) {
                return num_states+1;
            } else if (state == num_states+1) {
                return num_states;
            }
        }
        return state;
    };
    auto trans_swap_2_extra_states = [&seed_to_trans,&trans_to_seed,&swap_2_extra_states](size_t seed) -> size_t {
        auto [from, to, label] = seed_to_trans(seed);
        return trans_to_seed(swap_2_extra_states(from), swap_2_extra_states(to), label);
    };
//    std::vector<PAutomaton<>> automata;
    std::unordered_set<std::vector<size_t>, absl::Hash<std::vector<size_t>>> automata_set;
    size_t count_automata = 0;

    for (int num_trans = 0; num_trans <= 2; ++num_trans) {
        std::vector<bool> rule_selector(all_transitions);
        std::fill(rule_selector.begin(), rule_selector.begin() + num_trans, true);
        do {
            std::vector<size_t> transitions;
            for (size_t i = 0; i < all_transitions; ++i) {
                if (rule_selector[i]) {
                    transitions.push_back(i);
                }
            }

            std::vector<std::vector<size_t>> edges(num_states + num_extra_states);
            for (size_t trans : transitions) {
                auto [from, to, _] = seed_to_trans(trans);
                edges[from].emplace_back(to);
            }
            std::vector<size_t> waiting; std::unordered_set<size_t> seen; // Initialize reachability search
            for (size_t i = 0; i < num_states; ++i) { waiting.push_back(i); seen.emplace(i); }
            while (!waiting.empty()) {
                auto from = waiting.back(); waiting.pop_back();
                for (const auto& to : edges[from]) {
                    if (seen.emplace(to).second) waiting.push_back(to);
                }
            }
            if (do_reduction) {
                // If start of a transition is not reachable, then it is useless, so this combination is covered by another case.
                if (std::any_of(transitions.begin(), transitions.end(), [&seen,&seed_to_trans](size_t trans){ return seen.find(std::get<0>(seed_to_trans(trans))) == seen.end(); })) {
                    continue;
                }
                std::vector<size_t> transformed;
                std::transform(transitions.begin(), transitions.end(), std::back_inserter(transformed), trans_swap_2_extra_states);
                if (automata_set.find(transformed) != automata_set.end()) {
                    continue;
                }
                automata_set.emplace(transitions);
            }

            for (int accept_mask = 0; accept_mask < (1 << (num_states + num_extra_states)); ++accept_mask) {
                std::vector<size_t> initially_accepting_states;
                size_t pautomaton_state = 0;
                for (; pautomaton_state < num_states; ++pautomaton_state) {
                    if ((accept_mask & (1 << pautomaton_state)) != 0) {
                        initially_accepting_states.push_back(pautomaton_state);
                    }
                }
                pdaaal::PAutomaton automaton(pda, initially_accepting_states, true);
                automaton.insert_state("q2");
                automaton.insert_state("q3");
                bool skip = false;
                for (; pautomaton_state < num_states + num_extra_states; ++pautomaton_state) {
                    bool accepting = (accept_mask & (1 << pautomaton_state)) != 0;
                    automaton.add_state(false, accepting);
                    if (accepting && seen.find(pautomaton_state) == seen.end()) { // Unreachable accepting states should not matter
                        skip = true;
                    }
                }
                if (do_reduction && skip) continue;

                for (size_t transition : transitions) {
                    add_transition(transition, automaton);
                }
                std::stringstream name_with_index;
                name_with_index << name << "_" << count_automata;
                {
                    std::stringstream file_name;
                    file_name << name << count_automata << ".thy";
                    auto file_path = output_dir / file_name.str();
                    std::ofstream out_stream(file_path);
                    if (!out_stream.is_open()) {
                        std::stringstream es;
                        es << "error: Could not open file: " << file_path << std::endl;
                        throw std::runtime_error(es.str());
                    }
                    IsabellePrettyPrinter isabelle_pp(out_stream);
                    isabelle_pp.print_automaton(name_with_index.str(), automaton, pda, "l");
                }
                {
                    std::stringstream file_name;
                    file_name << name << count_automata << ".json";
                    auto file_path = output_dir / file_name.str();
                    std::ofstream out_stream(file_path);
                    if (!out_stream.is_open()) {
                        std::stringstream es;
                        es << "error: Could not open file: " << file_path << std::endl;
                        throw std::runtime_error(es.str());
                    }
                    out_stream << automaton.to_json().dump() << std::endl;
                }
//                automata.emplace_back(std::move(automaton));
                count_automata++;
            }
        } while (std::prev_permutation(rule_selector.begin(), rule_selector.end()));
    }

//    for (const auto& automaton : automata) {
//        automaton.to_dot(std::cout);
//    }
    std::cout << "Num " << name << " automata: " << count_automata << std::endl;
    // return automata;
}


int main(int argc, const char** argv) {
    po::options_description opts;
    opts.add_options()
            ("help,h", "produce help message");
//            ("version,v", "print version");

    po::options_description input("Input Options");
    po::options_description output("Output Options");
    size_t seed = std::random_device()(); // Default to a random seed. Overwritten if -s option is set.
    size_t number_of_instances = 1;
    bool with_weight = false;
//    size_t begin_from_instance = 0;
    input.add_options()
            ("seed,s", po::value<size_t>(&seed), "Seed for random number generator (use a random_device if not set)")
            ("instances,n", po::value<size_t>(&number_of_instances), "Number of instances to generate (default=1)")
            ("weight,w", po::bool_switch(&with_weight), "Generate weighted PDA (default=false) only supported for --random")
//            ("from,f", po::value<size_t>(&begin_from_instance), "Start generating from this instance number - to support chunking (default=0)")
            ;
//    bool no_parser_warnings = false;
//    bool silent = false;
//    std::string output_file;
    std::string output_dir;
    bool gen_random = false;
    output.add_options()
//            ("disable-parser-warnings,W", po::bool_switch(&no_parser_warnings), "Disable warnings from parser.")
//            ("silent,s", po::bool_switch(&silent), "Disables non-essential output (implies -W).")
//            ("output,o", po::value<std::string>(&output_file), "Output file (default is standard out).")
            ("random", po::bool_switch(&gen_random), "Generate random PDA and automata.")
            ("dir,d", po::value<std::string>(&output_dir), "Output directory to put instance files in.")
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

    if (output_dir.empty()) {
        std::cerr << "Please specify an output directory";
        return 1;
    }
    fs::path output_dir_path(output_dir);
    if (!fs::is_directory(output_dir_path)) {
        std::cerr << "Specified output directory: " << output_dir_path << " is not a valid directory.";
        return 1;
    }
    if (gen_random) {
        if (with_weight) {
            generate_many<true>(random_gen, output_dir_path, number_of_instances);
        } else {
            generate_many<false>(random_gen, output_dir_path, number_of_instances);
        }
    } else {
        if (with_weight) {
            generate_pda_without_symmetries<true>(output_dir_path);
            generate_pautomata_without_symmetries<true>(output_dir_path, true);
            generate_pautomata_without_symmetries<true>(output_dir_path, false);
        } else {
            generate_pda_without_symmetries<false>(output_dir_path);
            generate_pautomata_without_symmetries<false>(output_dir_path, true);
            generate_pautomata_without_symmetries<false>(output_dir_path, false);
        }
    }

//    if (output_file.empty() || output_file == "-") {
//        generate(std::cout, random_gen, false);
//    } else {
//        std::ofstream out_stream(output_file);
//        if (!out_stream.is_open()) {
//            std::stringstream es;
//            es << "error: Could not open file: " << output_file << std::endl;
//            throw std::runtime_error(es.str());
//        }
//        generate(out_stream, random_gen);
//    }

    return 0;
}
