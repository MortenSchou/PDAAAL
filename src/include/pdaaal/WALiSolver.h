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
 * File:   WALiSolver.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 19-04-2022.
 */

#ifndef PDAAAL_WALISOLVER_H
#define PDAAAL_WALISOLVER_H

#include "NFA.h"
#include <wali/wpds/WPDS.hpp>
#include <wali/Reach.hpp>
#include <wali/regex/Regex.hpp>
#include <wali/ShortestPathSemiring.hpp>
#include <wali/ShortestPathWorklist.hpp>
#include <wali/wfa/State.hpp>
#include <utility>

namespace pdaaal {

    class Reach : public wali::Reach {
    public:
        explicit Reach( bool b ) : wali::Reach(b) {};
        static wali::sem_elem_t One() { static Reach s(true); return s.one(); }
        static wali::sem_elem_t Zero() { static Reach s(false); return s.zero(); }
    };

    class UintWeight : public wali::ShortestPathSemiring {
    public:
        UintWeight() = default;
        explicit UintWeight( unsigned int b ) : ShortestPathSemiring(b) {}
        static wali::sem_elem_t One() { static wali::sem_elem_t O(wali::ShortestPathSemiring::make_one()); return O; }
        static wali::sem_elem_t Zero() { static wali::sem_elem_t Z(wali::ShortestPathSemiring::make_zero()); return Z; }
    };

    class VectorUintWeight : public wali::SemElem {
        std::vector<uint32_t> weight;
    public:
        VectorUintWeight() = default;
        explicit VectorUintWeight(const std::vector<uint32_t>& b) : weight(b) {}
        explicit VectorUintWeight(std::vector<uint32_t>&& b) : weight(std::move(b)) {}
        [[nodiscard]] wali::sem_elem_t one() const override { static wali::sem_elem_t O(new VectorUintWeight(std::vector<uint32_t>())); return O; }
        [[nodiscard]] wali::sem_elem_t zero() const override { static wali::sem_elem_t Z(new VectorUintWeight(std::vector<uint32_t>{std::numeric_limits<uint32_t>::max()})); return Z; }
        static wali::sem_elem_t One() { static VectorUintWeight s; return s.one(); }
        static wali::sem_elem_t Zero() { static VectorUintWeight s; return s.zero(); }

        // zero is the annihilator for extend
        wali::sem_elem_t extend( wali::SemElem* se ) override {
            auto* rhs = dynamic_cast< VectorUintWeight* >(se);
            if (is_zero() || rhs->is_zero()) {
                return zero();
            } else {
                const auto& [small, large] = std::minmax(weight, rhs->weight, [](const auto& l, const auto& r){ return l.size() < r.size(); });
                auto result = large;
                std::transform(small.begin(), small.end(), large.begin(),
                               result.begin(), std::plus<>());
                return new VectorUintWeight(std::move(result));
            }
        }
        // zero is neutral for combine
        wali::sem_elem_t combine( wali::SemElem* se ) override {
            auto* rhs = dynamic_cast< VectorUintWeight* >(se);
            if(is_zero() && rhs->is_zero()) {
                return zero();
            } else {
                return new VectorUintWeight(std::min(weight, rhs->weight));
            }
        }
        bool equal( wali::SemElem* se ) const override {
            auto* rhs = dynamic_cast< VectorUintWeight* >(se);
            return ( rhs && weight == rhs->weight );
        }
        std::ostream& print( std::ostream& o ) const override {
            if (is_zero()) {
                o << "ZERO";
            } else {
                bool first = true;
                o << "[";
                for (const auto& elem : weight) {
                    if (!first) o << ",";
                    first = false;
                    o << elem;
                }
                o << "]";

            }
            return o;
        }
        [[nodiscard]] nlohmann::json to_json() const {
            return is_zero() ? json() : json(weight);
        }
    private:
        [[nodiscard]] bool is_zero() const {
            return weight.size() == 1 && weight[0] == std::numeric_limits<uint32_t>::max();
        }
    };

    struct wali_pdaaal {
        static wali::Key key_from_size_t(size_t input) {
            if (input == std::numeric_limits<size_t>::max()) { // Convert some special values, and size check the rest.
                return wali::getKey(std::numeric_limits<int>::max());
            } else if (input == std::numeric_limits<size_t>::max() -1) {
                return wali::getKey(std::numeric_limits<int>::max() - 1);
            } else if (input == std::numeric_limits<size_t>::max() - 2) {
                return wali::getKey(std::numeric_limits<int>::max() - 2);
            } else if (input < (size_t)std::numeric_limits<int>::max() - 2) {
                return wali::getKey((int)input);
            } else {
                throw std::runtime_error("Encountered too big key input. size_t -> int issue. WALi.");
            }
        }
    };

    template <typename W>
    class WALi_SolverInstance {
    public:
        WALi_SolverInstance(wali::wpds::WPDS& pda,
                            const pdaaal::NFA<size_t>& initial_nfa, const std::vector<size_t>& initial_states,
                            const pdaaal::NFA<size_t>& final_nfa,   const std::vector<size_t>& final_states,
                            const std::vector<wali::Key>& all_labels, size_t max_pda_state)
                : _pda(pda), _max_pda_state(max_pda_state),
                  _initial(make_CA(initial_nfa, initial_states, all_labels, max_pda_state)),
                  _final(make_CA(final_nfa, final_states, all_labels, max_pda_state)) {};
        template<typename automaton_t>
        WALi_SolverInstance(wali::wpds::WPDS& pda, const automaton_t& initial, const automaton_t& final, size_t max_pda_state)
                : _pda(pda), _max_pda_state(max_pda_state), _initial(make_CA(initial)), _final(make_CA(final)) {}

        template<typename automaton_t>
        static wali::wfa::WFA make_CA(const automaton_t& p_automaton) {
            wali::wfa::WFA automaton;

            for (const auto& state : p_automaton.states()) {
                auto from_key = wali_pdaaal::key_from_size_t(state->_id);
                automaton.addState(from_key, W::Zero());
                if (state->_accepting) {
                    automaton.add_final_state(from_key);
                }
                for (const auto& [to, labels] : state->_edges) {
                    auto to_key = wali_pdaaal::key_from_size_t(to);
                    automaton.addState(to_key, W::Zero());
                    for (auto [label,_] : labels) {
                        automaton.addTrans(from_key, wali_pdaaal::key_from_size_t(label), to_key, W::One());
                    }
                }
            }
            return automaton;
        }

        static wali::wfa::WFA make_CA(const pdaaal::NFA<size_t>& nfa, const std::vector<size_t>& initial_states,
                                   const std::vector<wali::Key>& all_labels, size_t max_pda_state) {
            wali::wfa::WFA automaton;

            using nfastate_t = typename pdaaal::NFA<size_t>::state_t;
            std::unordered_map<const nfastate_t*, size_t> nfastate_to_id;
            std::vector<std::pair<const nfastate_t*,size_t>> waiting;
            size_t next_id = max_pda_state;
            auto get_nfastate_id = [&automaton, &waiting, &nfastate_to_id, &next_id](const nfastate_t* n) -> size_t {
                // Adds nfastate if not yet seen.
                size_t n_id;
                auto it = nfastate_to_id.find(n);
                if (it != nfastate_to_id.end()) {
                    n_id = it->second;
                } else {
                    n_id = next_id++;
                    if (n->_accepting) {
                        automaton.add_final_state(wali_pdaaal::key_from_size_t(n_id));
                    }
                    nfastate_to_id.emplace(n, n_id);
                    waiting.emplace_back(n, n_id);
                }
                return n_id;
            };
            auto add_edges = [&automaton, &all_labels](const std::vector<size_t>& from, size_t to, bool negated, const std::vector<size_t>& labels){
                if (negated) {
                    assert(std::is_sorted(labels.begin(), labels.end()));
                    std::vector<wali::Key> negated_labels(labels.size());
                    std::transform(labels.begin(), labels.end(), negated_labels.begin(), [](size_t l){ return wali_pdaaal::key_from_size_t(l); });
                    std::vector<wali::Key> result_labels;
                    result_labels.reserve(all_labels.size() - labels.size());
                    std::set_difference(all_labels.begin(), all_labels.end(), negated_labels.begin(), negated_labels.end(), std::back_inserter(result_labels));
                    auto to_key = wali_pdaaal::key_from_size_t(to);
                    for (auto f : from) {
                        auto from_key = wali_pdaaal::key_from_size_t(f);
                        for (auto label : result_labels) {
                            automaton.addTrans(from_key, label, to_key, W::One());
                        }
                    }
                } else {
                    auto to_key = wali_pdaaal::key_from_size_t(to);
                    for (auto f : from) {
                        auto from_key = wali_pdaaal::key_from_size_t(f);
                        for (auto label : labels) {
                            automaton.addTrans(from_key, wali_pdaaal::key_from_size_t(label), to_key, W::One());
                        }
                    }
                }
            };

            for (const auto& i : nfa.initial()) {
                for (const auto& e : i->_edges) {
                    for (const nfastate_t* n : e.follow_epsilon()) {
                        size_t n_id = get_nfastate_id(n);
                        add_edges(initial_states, n_id, e._negated, e._symbols);
                    }
                }
            }
            while (!waiting.empty()) {
                auto [top, top_id] = waiting.back();
                waiting.pop_back();
                std::vector<size_t> from{top_id};
                for (const auto& e : top->_edges) {
                    for (const nfastate_t* n : e.follow_epsilon()) {
                        size_t n_id = get_nfastate_id(n);
                        add_edges(from, n_id, e._negated, e._symbols);
                    }
                }
            }
            return automaton;
        }

        static void fix_initial_states(wali::wfa::WFA& automaton, size_t max_pda_state) {
            // wali::wfa supports only a single initial state. We simulate multiple initial states using epsilon transitions.
            auto i_key = wali::getKey("initial");
            automaton.addState(i_key, W::Zero());
            automaton.set_initial_state(i_key);
            for (size_t pda_state = 0; pda_state < max_pda_state; ++pda_state) { // Yes this is hacky...
                auto to = wali_pdaaal::key_from_size_t(pda_state);
                std::stringstream ss;
                ss << pda_state;
                auto label = wali::getKey(ss.str()); // We need the label to be different from the normal labels. So we use string-to-key.
                automaton.addTrans(automaton.initial_state(), label, to, W::One());
            }
        }

        std::pair<bool,wali::sem_elem_t> post_star() {
            _pda.poststar(_initial, _answer);
            fix_initial_states(_answer, _max_pda_state);
            fix_initial_states(_final, _max_pda_state);
            wali::wfa::KeepLeft weight_maker;
            auto product = _answer.intersect(weight_maker, _final);
            product.path_summary_iterative_original();
            auto w = product.getState(product.getInitialState())->weight();
            return std::make_pair(!w->equal(W::Zero()), w);
        }
        std::pair<bool,wali::sem_elem_t> pre_star() {
            _pda.prestar(_final, _answer);
            fix_initial_states(_answer, _max_pda_state);
            fix_initial_states(_initial, _max_pda_state);
            wali::wfa::KeepLeft weight_maker;
            auto product = _answer.intersect(weight_maker, _initial);
            product.path_summary_iterative_original();
            auto w = product.getState(product.getInitialState())->weight();
            return std::make_pair(!w->equal(W::Zero()), w);
        }
//        void get_trace() {
//            wpds::CA<W> product(_s);
//            product.query = _answer.get_query();
//            wpds::util::KeepLeft<W> wmaker;
//            if (_answer.get_query().is_poststar()) {
//                wpds::util::intersect<W,W,W>(_answer,_final,wmaker,product);
//            } else {
//                wpds::util::intersect<W,W,W>(_answer,_initial,wmaker,product);
//            }
//            product.path_summary();
//            auto answer = product.state_weight(product.initial_state());
//
//        }

    private:
        wali::wpds::WPDS& _pda;
        size_t _max_pda_state;
        wali::wfa::WFA _initial;
        wali::wfa::WFA _final;
        wali::wfa::WFA _answer;
    };

    struct WPDS_Rule {
        wali::Key _from;
        wali::Key _pre;
        wali::Key _to{};
        wali::Key _l1 = wali::WALI_EPSILON;
        wali::Key _l2 = wali::WALI_EPSILON;
        wali::sem_elem_t _weight = nullptr;

        WPDS_Rule(wali::Key from, wali::Key pre) : _from(from), _pre(pre) {};
        WPDS_Rule(wali::Key from, wali::Key pre, wali::Key to) : _from(from), _pre(pre), _to(to) {};
        WPDS_Rule(wali::Key from, wali::Key pre, wali::Key to, wali::Key l1, wali::Key l2, const wali::sem_elem_t& weight)
                : _from(from), _pre(pre), _to(to), _l1(l1), _l2(l2), _weight(weight) {};

        static WPDS_Rule make_pop(wali::Key from, wali::Key pre, wali::Key to, const wali::sem_elem_t& weight) {
            return {from, pre, to, wali::WALI_EPSILON, wali::WALI_EPSILON, weight};
        }
        static WPDS_Rule make_swap(wali::Key from, wali::Key pre, wali::Key to, wali::Key op_label, const wali::sem_elem_t& weight) {
            return {from, pre, to, op_label, wali::WALI_EPSILON, weight};
        }
        static WPDS_Rule make_push(wali::Key from, wali::Key pre, wali::Key to, wali::Key op_label, const wali::sem_elem_t& weight) {
            return {from, pre, to, pre, op_label, weight};
        }
        static WPDS_Rule make_swap_push(wali::Key from, wali::Key pre, wali::Key to, wali::Key swap_label, wali::Key push_label, const wali::sem_elem_t& weight) {
            return {from, pre, to, swap_label, push_label, weight};
        }
    };

    template<typename W>
    class WALi_PDAFactory {
        using solver_instance_t = WALi_SolverInstance<W>;
        using label_t = size_t;
    public:
        using rule_t = WPDS_Rule;

        explicit WALi_PDAFactory(const std::unordered_set<size_t>& all_labels)
        {
            _all_labels.reserve(all_labels.size());
            for (const auto& label : all_labels) {
                _all_labels.emplace_back(wali_pdaaal::key_from_size_t(label));
            }
            std::sort(_all_labels.begin(), _all_labels.end());
        };
        solver_instance_t compile(const pdaaal::NFA<label_t>& initial_headers, const pdaaal::NFA<label_t>& final_headers) {
            build_pda();
            return solver_instance_t{_temp_pda, initial_headers, initial(), final_headers, accepting(), _all_labels, _max_pda_state+1};
        }
    protected:
        virtual void build_pda() = 0;
        virtual const std::vector<size_t>& initial() = 0;
        virtual const std::vector<size_t>& accepting() = 0;

        void add_rule(rule_t rule) {
            _max_pda_state = std::max(_max_pda_state, rule._from);
            _max_pda_state = std::max(_max_pda_state, rule._to);
            if (rule._weight == nullptr) {
                rule._weight = W::one(); // Default weight.
            }
            _temp_pda.add_rule(rule._from, rule._pre, rule._to, rule._l1, rule._l2, rule._weight);
        }
        void add_wildcard_rule(rule_t rule) {
            for (const auto& pre : _all_labels) {
                rule._pre = pre;
                add_rule(rule);
            }
        }
        void add_wildcard_noop_rule(rule_t rule) {
            for (const auto& pre : _all_labels) {
                rule._pre = pre;
                rule._l1 = pre;
                add_rule(rule);
            }
        }
    private:
        wali::wpds::WPDS _temp_pda;
        std::vector<wali::Key> _all_labels;
        size_t _max_pda_state = 0;
    };

}

#endif //PDAAAL_WALISOLVER_H
