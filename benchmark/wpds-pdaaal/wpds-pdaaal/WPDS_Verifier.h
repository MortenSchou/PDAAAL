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
 * File:   WPDS_Verifier.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 09-09-2021.
 */

#ifndef PDAAAL_WPDS_VERIFIER_H
#define PDAAAL_WPDS_VERIFIER_H

#include <pdaaal/PDAFactory.h>
// WPDS include must come after pdaaal include. Because otherwise we get compile errors in abseil, yeah...%&#!¤#
#include <WPDS.h>
#include <ref_ptr.h>
#include <utility>


namespace wpds_pdaaal {

    class Reach {
        bool isreached;
    public:
        ref_ptr< Reach >::count_t count;
        explicit Reach( bool b ) : isreached(b),count(0) {}
        static Reach* one() { return new Reach(true); }
        static Reach* zero() { return new Reach(false); }
        static Reach* quasiOne() { return one(); }
        // zero is the annihilator for extend
        Reach* extend( Reach* rhs ) const {
            if( isreached && rhs->isreached )
                return one();
            else // this or rhs is zero()
                return zero();
        }
        // zero is neutral for combine
        Reach* combine( Reach* rhs ) const {
            if( isreached || rhs->isreached )
                return one();
            else
                return zero();
        }
        bool equal( Reach* rhs ) const {
            return ( isreached == rhs->isreached );
        }
        std::ostream& print( std::ostream& o ) const {
            if( isreached )
                o << "ONE";
            else
                o << "ZERO";
            return o;
        }
    };

    class UintWeight {
        uint32_t weight;
    public:
        ref_ptr< UintWeight >::count_t count;
        explicit UintWeight( uint32_t b ) : weight(b), count(0) {}
        static UintWeight* one() { return new UintWeight(0); }
        static UintWeight* zero() { return new UintWeight(std::numeric_limits<uint32_t>::max()); }
        static UintWeight* quasiOne() { return one(); }
        // zero is the annihilator for extend
        UintWeight* extend( UintWeight* rhs ) const {
            if (weight != std::numeric_limits<uint32_t>::max() && rhs->weight != std::numeric_limits<uint32_t>::max())
                return new UintWeight(weight + rhs->weight);
            else // this or rhs is zero()
                return zero();
        }
        // zero is neutral for combine
        UintWeight* combine( UintWeight* rhs ) const {
            if(weight != std::numeric_limits<uint32_t>::max() || rhs->weight != std::numeric_limits<uint32_t>::max())
                return new UintWeight(std::min(weight, rhs->weight));
            else
                return zero();
        }
        bool equal( UintWeight* rhs ) const {
            return ( weight == rhs->weight );
        }
        std::ostream& print( std::ostream& o ) const {
            if( weight != std::numeric_limits<uint32_t>::max() )
                o << weight;
            else
                o << "ZERO";
            return o;
        }
    };

    class VectorUintWeight {
        std::vector<uint32_t> weight;
    public:
        ref_ptr< VectorUintWeight >::count_t count;
        explicit VectorUintWeight(const std::vector<uint32_t>& b) : weight(b), count(0) {}
        explicit VectorUintWeight(std::vector<uint32_t>&& b) : weight(std::move(b)), count(0) {}
        static VectorUintWeight* one() { return new VectorUintWeight(std::vector<uint32_t>()); }
        static VectorUintWeight* zero() { return new VectorUintWeight(std::vector<uint32_t>{std::numeric_limits<uint32_t>::max()}); }
        static VectorUintWeight* quasiOne() { return one(); }
        // zero is the annihilator for extend
        VectorUintWeight* extend( VectorUintWeight* rhs ) const {
            if (is_zero() || rhs->is_zero()) {
                return zero();
            } else {
                const auto& [small, large] = std::minmax(weight, rhs->weight, [](const auto& l, const auto& r){ return l.size() < r.size(); });
                auto result = large;
                std::transform(small.begin(), small.end(), large.begin(),
                               result.begin(), std::plus<uint32_t>());
                return new VectorUintWeight(std::move(result));
            }
        }
        // zero is neutral for combine
        VectorUintWeight* combine( VectorUintWeight* rhs ) const {
            if(is_zero() && rhs->is_zero()) {
                return zero();
            } else {
                return new VectorUintWeight(std::min(weight, rhs->weight));
            }
        }
        bool equal( VectorUintWeight* rhs ) const {
            return ( weight == rhs->weight );
        }
        std::ostream& print( std::ostream& o ) const {
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
    private:
        [[nodiscard]] bool is_zero() const {
            return weight.size() == 1 && weight[0] == std::numeric_limits<uint32_t>::max();
        }
    };

    template <typename W>
    struct WPDS_Rule {
        wpds::wpds_key_t _from;
        wpds::wpds_key_t _pre;
        wpds::wpds_key_t _to{};
        std::optional<wpds::wpds_key_t> _l1 = std::nullopt;
        std::optional<wpds::wpds_key_t> _l2 = std::nullopt;
        W* _weight = nullptr;

        WPDS_Rule(wpds::wpds_key_t from, wpds::wpds_key_t pre) : _from(from), _pre(pre) {};
        WPDS_Rule(wpds::wpds_key_t from, wpds::wpds_key_t pre, wpds::wpds_key_t to) : _from(from), _pre(pre), _to(to) {};
        WPDS_Rule(wpds::wpds_key_t from, wpds::wpds_key_t pre, wpds::wpds_key_t to, wpds::wpds_key_t l1, wpds::wpds_key_t l2, W* weight)
                : _from(from), _pre(pre), _to(to), _l1(l1), _l2(l2), _weight(weight) {};

        static WPDS_Rule<W> make_pop(wpds::wpds_key_t from, wpds::wpds_key_t pre, wpds::wpds_key_t to, W* weight) {
            return WPDS_Rule<W>(from, pre, to, std::nullopt, std::nullopt, weight);
        }
        static WPDS_Rule<W> make_swap(wpds::wpds_key_t from, wpds::wpds_key_t pre, wpds::wpds_key_t to, wpds::wpds_key_t op_label, W* weight) {
            return WPDS_Rule<W>(from, pre, to, op_label, std::nullopt, weight);
        }
        static WPDS_Rule<W> make_push(wpds::wpds_key_t from, wpds::wpds_key_t pre, wpds::wpds_key_t to, wpds::wpds_key_t op_label, W* weight) {
            return WPDS_Rule<W>(from, pre, to, pre, op_label, weight);
        }
        static WPDS_Rule<W> make_swap_push(wpds::wpds_key_t from, wpds::wpds_key_t pre, wpds::wpds_key_t to, wpds::wpds_key_t swap_label, wpds::wpds_key_t push_label, W* weight) {
            return WPDS_Rule<W>(from, pre, to, swap_label, push_label, weight);
        }

        static wpds::wpds_key_t key_from_size_t(size_t input) {
            if (input == std::numeric_limits<size_t>::max()) { // Convert some special values, and size check the rest.
                return int2key(std::numeric_limits<int>::max());
            } else if (input == std::numeric_limits<size_t>::max() -1) {
                return int2key(std::numeric_limits<int>::max() - 1);
            } else if (input == std::numeric_limits<size_t>::max() - 2) {
                return int2key(std::numeric_limits<int>::max() - 2);
            } else if (input < (size_t)std::numeric_limits<int>::max() - 2) {
                return int2key((int)input);
            } else {
                throw std::runtime_error("Encountered too big key input. size_t -> int issue. WPDS++.");
            }
        }
    };


    template <typename W>
    class WPDS_SolverInstance {
    public:
        WPDS_SolverInstance(wpds::WPDS<W>& pda,
                        const pdaaal::NFA<size_t>& initial_nfa, const std::vector<size_t>& initial_states,
                        const pdaaal::NFA<size_t>& final_nfa,   const std::vector<size_t>& final_states,
                        const std::vector<wpds::wpds_key_t>& all_labels, size_t max_pda_state, wpds::Semiring<W>& s)
                : _pda(pda),
                  _initial(make_CA(initial_nfa, initial_states, all_labels, max_pda_state, s)),
                  _final(make_CA(final_nfa, final_states, all_labels, max_pda_state, s)),
                  _s(s), _answer(_s) {};
        template<typename automaton_t>
        WPDS_SolverInstance(wpds::WPDS<W>& pda, const automaton_t& initial, const automaton_t& final, size_t max_pda_state, wpds::Semiring<W>& s)
                : _pda(pda), _initial(make_CA(initial, max_pda_state, s)), _final(make_CA(final, max_pda_state, s)), _s(s), _answer(_s) {};

        template<typename automaton_t>
        static wpds::CA<W> make_CA(const automaton_t& p_automaton, size_t max_pda_state, wpds::Semiring<W>& s) {
            wpds::CA<W> automaton(s);

            for (const auto& state : p_automaton.states()) {
                for (const auto& [to, labels] : state->_edges) {
                    auto from_key = WPDS_Rule<W>::key_from_size_t(state->_id);
                    auto to_key = WPDS_Rule<W>::key_from_size_t(to);
                    for (auto [label,_] : labels) {
                        automaton.add(from_key, WPDS_Rule<W>::key_from_size_t(label), to_key, W::one());
                    }
                }
            }

            // WPDS::CA supports only a single initial state. We simulate multiple initial states using epsilon transitions.
            automaton.add_initial_state(str2key("initial"));
            for (size_t pda_state = 0; pda_state < max_pda_state; ++pda_state) { // Yes this is hacky...
                auto to = WPDS_Rule<W>::key_from_size_t(pda_state);
                std::stringstream ss;
                ss << pda_state;
                auto label = str2key(ss.str()); // We need the label to be different from the normal labels. So we use string-to-key.
                automaton.add(automaton.initial_state(), label, to, W::one());
            }

            return automaton;
        }

        static wpds::CA<W> make_CA(const pdaaal::NFA<size_t>& nfa, const std::vector<size_t>& initial_states,
                                   const std::vector<wpds::wpds_key_t>& all_labels, size_t max_pda_state, wpds::Semiring<W>& s) {
            wpds::CA<W> automaton(s);

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
                        automaton.add_final_state(WPDS_Rule<W>::key_from_size_t(n_id));
                    }
                    nfastate_to_id.emplace(n, n_id);
                    waiting.emplace_back(n, n_id);
                }
                return n_id;
            };
            auto add_edges = [&automaton, &all_labels](const std::vector<size_t>& from, size_t to, bool negated, const std::vector<size_t>& labels){
                if (negated) {
                    assert(std::is_sorted(labels.begin(), labels.end()));
                    std::vector<wpds::wpds_key_t> negated_labels(labels.size());
                    std::transform(labels.begin(), labels.end(), negated_labels.begin(), [](size_t l){ return WPDS_Rule<W>::key_from_size_t(l); });
                    std::vector<wpds::wpds_key_t> result_labels;
                    result_labels.reserve(all_labels.size() - labels.size());
                    std::set_difference(all_labels.begin(), all_labels.end(), negated_labels.begin(), negated_labels.end(), std::back_inserter(result_labels));
                    auto to_key = WPDS_Rule<W>::key_from_size_t(to);
                    for (auto f : from) {
                        auto from_key = WPDS_Rule<W>::key_from_size_t(f);
                        for (auto label : result_labels) {
                            automaton.add(from_key, label, to_key, W::one());
                        }
                    }
                } else {
                    auto to_key = WPDS_Rule<W>::key_from_size_t(to);
                    for (auto f : from) {
                        auto from_key = WPDS_Rule<W>::key_from_size_t(f);
                        for (auto label : labels) {
                            automaton.add(from_key, WPDS_Rule<W>::key_from_size_t(label), to_key, W::one());
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
            // TODO: Add trans to all PDS states. Not just those in initial_state. Alternatively, do something smart after pre*/post*.
            // WPDS::CA supports only a single initial state. We simulate multiple initial states using epsilon transitions.
            automaton.add_initial_state(str2key("initial"));
            for (size_t pda_state = 0; pda_state < max_pda_state; ++pda_state) { // Yes this is hacky...
                auto to = WPDS_Rule<W>::key_from_size_t(pda_state);
                std::stringstream ss;
                ss << pda_state;
                auto label = str2key(ss.str()); // We need the label to be different from the normal labels. So we use string-to-key.
                automaton.add(automaton.initial_state(), label, to, W::one());
            }
//            for (auto initial_state : initial_states) {
//                auto to = WPDS_Rule<W>::key_from_size_t(initial_state);
//                std::stringstream ss;
//                ss << initial_state;
//                auto label = str2key(ss.str()); // We need the label to be different from the normal labels. So we use string-to-key.
//                automaton.add(automaton.initial_state(), label, to, W::one());
//            }

            return automaton;
        }

        std::pair<bool,ref_ptr<W>> post_star() {
            _answer = wpds::poststar<W>(_pda, _initial, _s);
            ref_ptr<W> reglangWeight = _answer.reglang_query(_final);
            return std::make_pair(!reglangWeight->equal(W::zero()), reglangWeight);
        }
        std::pair<bool,ref_ptr<W>> pre_star() {
            _answer = wpds::prestar<W>(_pda, _final, _s);
            ref_ptr<W> reglangWeight = _answer.reglang_query(_initial);
            return std::make_pair(!reglangWeight->equal(W::zero()), reglangWeight);
        }
        void get_trace() {
            wpds::CA<W> product(_s);
            product.query = _answer.get_query();
            wpds::util::KeepLeft<W> wmaker;
            if (_answer.get_query().is_poststar()) {
                wpds::util::intersect<W,W,W>(_answer,_final,wmaker,product);
            } else {
                wpds::util::intersect<W,W,W>(_answer,_initial,wmaker,product);
            }
            product.path_summary();
            auto answer = product.state_weight(product.initial_state());

        }

    private:
        wpds::WPDS<W>& _pda;
        wpds::CA<W> _initial;
        wpds::CA<W> _final;
        wpds::Semiring<W>& _s;
        wpds::CA<W> _answer;
    };


    template<typename W>
    class WPDS_PDAFactory {
        using solver_instance_t = WPDS_SolverInstance<W>;
        using label_t = size_t;
    public:
        using rule_t = WPDS_Rule<W>;

        explicit WPDS_PDAFactory(wpds::Semiring<W>& s, bool pre_star, const std::unordered_set<size_t>& all_labels)
        : _temp_pda(s, pre_star ? Query::prestar() : Query::poststar()), _s(s) {
            _all_labels.reserve(all_labels.size());
            for (const auto& label : all_labels) {
                _all_labels.emplace_back(rule_t::key_from_size_t(label));
            }
            std::sort(_all_labels.begin(), _all_labels.end());
        };
        solver_instance_t compile(const pdaaal::NFA<label_t>& initial_headers, const pdaaal::NFA<label_t>& final_headers) {
            build_pda();
            return solver_instance_t{_temp_pda, initial_headers, initial(), final_headers, accepting(), _all_labels, _max_pda_state+1, _s};
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
            if (!rule._l1 && !rule._l2) {
                _temp_pda.add_rule(rule._from, rule._pre, rule._to, rule._weight);
            } else if (rule._l1 && !rule._l2) {
                _temp_pda.add_rule(rule._from, rule._pre, rule._to, rule._l1.value(), rule._weight);
            } else if (rule._l1 && rule._l2){
                _temp_pda.add_rule(rule._from, rule._pre, rule._to, rule._l1.value(), rule._l2.value(), rule._weight);
            } else {
                throw std::logic_error("Invalid rule contains l2 but no l1.");
            }
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
        wpds::WPDS<W> _temp_pda;
        std::vector<wpds::wpds_key_t> _all_labels;
        size_t _max_pda_state = 0;
        wpds::Semiring<W>& _s;
    };

}

#endif //PDAAAL_WPDS_VERIFIER_H
