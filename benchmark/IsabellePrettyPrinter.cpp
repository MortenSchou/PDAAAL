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
 * File:   IsabellePrettyPrinter.cpp
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 05-11-2021.
 */

#include "IsabellePrettyPrinter.h"

using namespace pdaaal;

template <typename T, typename Fn = std::function<void(std::ostream&,const T&)>>
std::ostream& print_list(std::ostream& out, std::vector<T> list, const std::string& separator = "", const std::string& prefix = "", Fn&& print = [](std::ostream& s, const T& t){ s << t; }) {
    static_assert(true);
    bool first = true;
    for (const auto& elem : list) {
        if (first) {
            first = false;
        } else {
            out << separator;
        }
        out << prefix;
        print(out,elem);
    }
    return out;
}

void IsabellePrettyPrinter::print_query(const TypedPDA<char>& pda, const PAutomaton<>& initial_automaton, const PAutomaton<>& final_automaton) {
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

    _out << "(* Query specific part START *)" << std::endl
         << std::endl
         << "(* List all control locations (in PDS), labels, and non-initial states in both P-automata *)" << std::endl
         << "datatype ctr_loc = ";
    if (states.empty()) {
        _out << "p" << std::endl;
    } else {
        print_list(_out, states, " | ", "p") << std::endl;
    }
    _out << "definition ctr_loc_list where \"ctr_loc_list = [";
    if (states.empty()) {
        _out << "p" << "]\"" << std::endl;
    } else {
        print_list(_out, states, ",", "p") << "]\"" << std::endl;
    }
    _out << "datatype label = ";
    print_list(_out, labels, " | ") << std::endl;
    _out << "definition label_list where \"label_list = [";
    print_list(_out, labels, ",") << "]\"" << std::endl;
    _out << "datatype state = ";
    print_list(_out, extra_states, " | ", "q") << std::endl;
    _out << "definition state_list where \"state_list = [";
    print_list(_out, extra_states, ",", "q") << "]\"" << std::endl;
    _out << std::endl
         << "(* Define rules of PDS, and the two P-automata *)" << std::endl;
    print_rules("pds_rules", pda);
    print_automaton("initial", initial_automaton, pda);
    print_automaton("final", final_automaton, pda);
    _out << "(* Query specific part END *)" << std::endl
         << std::endl;
}

std::ostream& IsabellePrettyPrinter::print_rules(const std::string& name, const TypedPDA<char>& pda){
    auto rules = pda.all_rules();
    _out << "definition " << name << " :: \"(ctr_loc, label) rule set\" where" << std::endl;
    if (rules.empty()) {
        _out << "  \"" << name << " = {}\"" << std::endl;
        return _out;
    }
    _out << "  \"" << name << " = {" << std::endl;
    bool first_rule = true;
    for (const auto& rule : rules) {
        if (first_rule) {
            first_rule = false;
        } else {
            _out << "," << std::endl;
        }
        _out << "  ((p" << rule._from << ", " << rule._pre << "), (p" << rule._to << ", ";
        switch (rule._op) {
            case POP:
                _out << "pop";
                break;
            case SWAP:
                _out << "swap " << rule._op_label;
                break;
            case NOOP:
                _out << "swap " << rule._pre;
                break;
            case PUSH:
                _out << "push " << rule._op_label << " " << rule._pre;
                break;
            default:
                throw std::logic_error("error: Unknown op type.");
                break;
        }
        _out << "))";
    }
    _out << "}\"" << std::endl;
    return _out;
}
std::ostream& IsabellePrettyPrinter::print_automaton(const std::string& name_prefix, const PAutomaton<>& automaton, const TypedPDA<char>& pda) {
    _out << "definition " << name_prefix
         << "_automaton :: \"((ctr_loc, state, label) PDS.state, label) transition set\" where" << std::endl;

    bool first = true;
    std::vector<size_t> accepting_ctr_loc;
    std::vector<size_t> accepting_ctr_loc_st;
    for (const auto& state: automaton.states()) {
        auto from = state->_id;
        if (state->_accepting) {
            if (from < automaton.pda().states().size()) {
                accepting_ctr_loc.push_back(from);
            } else {
                accepting_ctr_loc_st.push_back(from);
            }
        }
        for (const auto&[to, labels]: state->_edges) {
            for (const auto& label: labels) {
                if (first) {
                    first = false;
                    _out << "  \"" << name_prefix << "_automaton = {" << std::endl;
                } else {
                    _out << "," << std::endl;
                }
                _out << "  ((";
                print_automaton_state(from, automaton) << ", " << pda.get_symbol(label.first) << ", ";
                print_automaton_state(to, automaton) << "))";
            }
        }
    }
    if (first) {
        _out << "  \"" << name_prefix << "_automaton = {}\"" << std::endl;
    } else {
        _out << "}\"" << std::endl;
    }
    _out << "definition " << name_prefix << "_ctr_loc where \"" << name_prefix << "_ctr_loc = {";
    print_list(_out, accepting_ctr_loc, ", ", "", [&automaton,this](std::ostream& s, size_t state){ print_automaton_state(state, automaton, false); }) << "}\"" << std::endl;
    _out << "definition " << name_prefix << "_ctr_loc_st where \"" << name_prefix << "_ctr_loc_st = {";
    print_list(_out, accepting_ctr_loc_st, ", ", "", [&automaton,this](std::ostream& s, size_t state){ print_automaton_state(state, automaton, false); }) << "}\"" << std::endl;
    return _out;
}
std::ostream& IsabellePrettyPrinter::print_automaton_state(size_t state, const PAutomaton<>& automaton, bool print_type) {
    if (state < automaton.pda().states().size()) {
        if (print_type) {
            _out << "Ctr_Loc ";
        }
        _out << "p" << state;
    } else {
        if (print_type) {
            _out << "Ctr_Loc_St ";
        }
        _out << "q" << state;
    }
    return _out;
}

void IsabellePrettyPrinter::print_begin() {
    _out << "theory Ex" << std::endl
         << "  imports PDS.PDS_Code" << std::endl
         << "begin" << std::endl
         << std::endl;
}
void IsabellePrettyPrinter::print_end() {
    _out << std::endl
         << "end" << std::endl;
}
void IsabellePrettyPrinter::print_proofs() {
    _out << "derive linorder ctr_loc" << std::endl
         << "derive linorder label" << std::endl
         << "instantiation ctr_loc :: finite begin" << std::endl
         << "  instance by (standard, rule finite_subset[of _ \"set ctr_loc_list\"]) (auto intro: ctr_loc.exhaust simp: ctr_loc_list_def)" << std::endl
         << "end" << std::endl
         << "instantiation label :: finite begin" << std::endl
         << "  instance by (standard, rule finite_subset[of _ \"set label_list\"]) (auto intro: label.exhaust simp: label_list_def)" << std::endl
         << "end" << std::endl
         << "instantiation state :: finite begin" << std::endl
         << "  instance by (standard, rule finite_subset[of _ \"set state_list\"]) (auto intro: state.exhaust simp: state_list_def)" << std::endl
         << "end" << std::endl
         << "instantiation ctr_loc :: enum begin" << std::endl
         << "definition \"enum_ctr_loc = ctr_loc_list\"" << std::endl
         << "definition \"enum_all_ctr_loc P = list_all P ctr_loc_list\"" << std::endl
         << "definition \"enum_ex_ctr_loc P = list_ex P ctr_loc_list\"" << std::endl
         << "instance apply standard" << std::endl
         << "     apply (auto simp: enum_ctr_loc_def enum_all_ctr_loc_def enum_ex_ctr_loc_def ctr_loc_list_def)" << std::endl
         << "  subgoal for x by (cases x; simp)" << std::endl
         << "  subgoal for P x by (cases x; simp)" << std::endl
         << "  subgoal for P x by (cases x; simp)" << std::endl
         << "  done" << std::endl
         << "end" << std::endl
         << std::endl;
}
void IsabellePrettyPrinter::print_lemma(bool answer) {
    _out << "lemma" << std::endl
         << "  \"check pds_rules initial_automaton initial_ctr_loc initial_ctr_loc_st" << std::endl
         << "                   final_automaton   final_ctr_loc   final_ctr_loc_st   = Some " << (answer ? "True" : "False") << "\"" << std::endl
         << "  by eval" << std::endl;
}
