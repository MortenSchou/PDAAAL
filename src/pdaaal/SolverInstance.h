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
 * File:   SolverInstance.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 27-11-2020.
 */

#ifndef PDAAAL_SOLVERINSTANCE_H
#define PDAAAL_SOLVERINSTANCE_H

#include "PAutomaton.h"
#include "AbstractionPDA.h"
#include "AbstractionPAutomaton.h"

namespace pdaaal {

    template <typename pda_t, typename automaton_t, typename T, typename W, typename C, typename A>
    class SolverInstance_impl {
        using product_automaton_t = PAutomaton<W,C,A>; // No explicit abstraction on product automaton - this is covered by _initial and _final.
        using state_t = typename product_automaton_t::state_t;
    public:
        SolverInstance_impl(pda_t&& pda, const NFA<T>& initial_nfa, const std::vector<size_t>& initial_states,
                                         const NFA<T>& final_nfa,   const std::vector<size_t>& final_states)
        : _pda(std::move(pda)), _pda_size(_pda.states().size()),
          _initial(_pda, initial_nfa, initial_states), _final(_pda, final_nfa, final_states),
          _product(_pda, intersect_vector(initial_states, final_states), initial_nfa.empty_accept() && final_nfa.empty_accept()) { };

        // Returns whether an accepting state in the product automaton was reached.
        bool initialize_product() {
            std::vector<size_t> ids(_product.states().size());
            std::iota (ids.begin(), ids.end(), 0); // Fill with 0,1,...,size-1;
            return construct_reachable(ids,
                                       _swap_initial_final ? _final : _initial,
                                       _swap_initial_final ? _initial : _final);
        }

        // Returns whether an accepting state in the product automaton was reached.
        bool add_edge_product(size_t from, uint32_t label, size_t to, trace_ptr<W> trace) {
            const automaton_t& initial = _swap_initial_final ? _final : _initial;
            const automaton_t& final = _swap_initial_final ? _initial : _final;

            std::vector<std::pair<size_t,size_t>> from_states;
            if (from < _id_fast_lookup.size()) { // Avoid out-of-bounds.
                from_states = _id_fast_lookup[from]; // Copy here, since loop-body might alter _id_fast_lookup[from].
            }
            if (from < _pda_size) {
                from_states.emplace_back(from, from); // Initial states are not stored in _id_fast_lookup.
            }
            std::vector<size_t> waiting;
            for (auto [final_from, product_from] : from_states) { // Iterate through reachable 'from-states'.
                for (const auto& [final_to,final_labels] : final.states()[final_from]->_edges) {
                    if (final_labels.contains(label)) {
                        auto [fresh, product_to] = get_product_state(initial.states()[to].get(), final.states()[final_to].get());
                        _product.add_edge(product_from, product_to, label, trace);
                        if (_product.has_accepting_state()) {
                            return true; // Early termination
                        }
                        if (fresh) {
                            waiting.push_back(product_to); // If the 'to-state' is new (was not previously reachable), we need to continue constructing from there.
                        }
                    }
                }
            }
            return construct_reachable(waiting, initial, final);
        }

        automaton_t& automaton() {
            return _swap_initial_final ? _final : _initial;
        }
        const automaton_t& automaton() const {
            return _swap_initial_final ? _final : _initial;
        }

        const automaton_t& initial_automaton() const {
            return _initial;
        }
        const automaton_t& final_automaton() const {
            return _final;
        }

        const pda_t& pda() const {
            return _pda;
        }

        void enable_pre_star() {
            _swap_initial_final = true;
        }

        template<bool abstraction>
        using path_state = std::conditional_t<abstraction, std::pair<size_t,size_t>, size_t>;

        template<Trace_Type trace_type = Trace_Type::Any, bool abstraction = false>
        [[nodiscard]] typename std::conditional_t<trace_type == Trace_Type::Shortest && is_weighted<W>,
                std::tuple<std::vector<path_state<abstraction>>, std::vector<uint32_t>, W>,
                std::tuple<std::vector<path_state<abstraction>>, std::vector<uint32_t>>>
        find_path() const {
            if constexpr (trace_type == Trace_Type::Shortest && is_weighted<W>) { // TODO: Consider unweighted shortest path.
                // Dijkstra.
                struct queue_elem {
                    W weight;
                    size_t state;
                    uint32_t label;
                    size_t stack_index;
                    const queue_elem *back_pointer;
                    queue_elem(W weight, size_t state, uint32_t label, size_t stack_index, const queue_elem *back_pointer = nullptr)
                            : weight(weight), state(state), label(label), stack_index(stack_index), back_pointer(back_pointer) {};

                    bool operator<(const queue_elem &other) const {
                        if (state != other.state) {
                            return state < other.state;
                        }
                        return label < other.label;
                    }
                    bool operator==(const queue_elem &other) const {
                        return state == other.state && label == other.label;
                    }
                    bool operator!=(const queue_elem &other) const {
                        return !(*this == other);
                    }
                };
                struct queue_elem_comp {
                    bool operator()(const queue_elem &lhs, const queue_elem &rhs){
                        C less;
                        return less(rhs.weight, lhs.weight); // Used in a max-heap, so swap arguments to make it a min-heap.
                    }
                };
                queue_elem_comp less;
                A add;
                std::priority_queue<queue_elem, std::vector<queue_elem>, queue_elem_comp> search_queue;
                std::vector<queue_elem> visited;
                std::vector<std::unique_ptr<queue_elem>> pointers;
                for (size_t i = 0; i < _pda_size; ++i) { // Iterate over _product._initial ([i]->_id)
                    search_queue.emplace(zero<W>()(), i, std::numeric_limits<uint32_t>::max(), 0); // No label going into initial state.
                }
                while(!search_queue.empty()) {
                    auto current = search_queue.top();
                    search_queue.pop();

                    if (_product.states()[current.state]->_accepting) {
                        std::vector<path_state<abstraction>> path(current.stack_index + 1);
                        std::vector<uint32_t> label_stack(current.stack_index);
                        const queue_elem* p = &current;
                        while (p->stack_index > 0) {
                            path[p->stack_index] = get_original_ids(p->state).first;
                            label_stack[p->stack_index - 1] = p->label;
                            p = p->back_pointer;
                        }
                        if constexpr (abstraction) {
                            path[p->stack_index] = get_original_ids(p->state).to_pair();
                        } else {
                            path[p->stack_index] = get_original_ids(p->state).first;
                        }
                        return std::make_tuple(path, label_stack, current.weight);
                    }

                    auto lb = std::lower_bound(visited.begin(), visited.end(), current);
                    if (lb != std::end(visited) && *lb == current) {
                        if (less(*lb, current)) {
                            *lb = current;
                        } else {
                            continue;
                        }
                    } else {
                        lb = visited.insert(lb, current); // TODO: Consider using std::unordered_map instead...
                    }
                    auto u_pointer = std::make_unique<queue_elem>(*lb);
                    auto pointer = u_pointer.get();
                    pointers.push_back(std::move(u_pointer));
                    for (const auto &[to,labels] : _product.states()[current.state]->_edges) {
                        if (!labels.empty()) {
                            auto label = std::min_element(labels.begin(), labels.end(), [](const auto& a, const auto& b){ return C{}(a.second.second, b.second.second); });
                            search_queue.emplace(add(current.weight, label->second.second), to, label->first, current.stack_index + 1, pointer);
                        }
                    }
                }
                return std::make_tuple(std::vector<path_state<abstraction>>(), std::vector<uint32_t>(), max<W>()());
            } else {
                // DFS search.
                std::vector<path_state<abstraction>> path;
                std::vector<uint32_t> label_stack;

                std::vector<std::pair<size_t,size_t>> waiting; // state_id, stack_index
                waiting.reserve(_pda_size);
                for (size_t i = 0; i < _pda_size; ++i) {
                    if (_product.states()[i]->_accepting) { // Initial accepting state
                        if constexpr (abstraction) {
                            path.emplace_back(i,i);
                        } else {
                            path.push_back(i);
                        }
                        return std::make_tuple(path, label_stack);
                    }
                    waiting.emplace_back(i,0); // Add all initial states in _product.
                }
                std::unordered_set<size_t> seen;

                while (!waiting.empty()) {
                    auto [current, stack_index] = waiting.back();
                    waiting.pop_back();
                    for (const auto &[to,labels] : _product.states()[current]->_edges) {
                        if (!labels.empty() && seen.emplace(to).second) {
                            uint32_t label = labels[0].first;
                            path.resize(stack_index + 2);
                            label_stack.resize(stack_index + 1);
                            if constexpr (abstraction) {
                                path[stack_index] = get_original_ids(current).to_pair();
                            } else {
                                path[stack_index] = get_original_ids(current).first;
                            }
                            label_stack[stack_index] = label;
                            if (_product.states()[to]->_accepting) {
                                if constexpr (abstraction) {
                                    path[stack_index + 1] = get_original_ids(to).to_pair();
                                } else {
                                    path[stack_index + 1] = get_original_ids(to).first;
                                }
                                return std::make_tuple(path, label_stack);
                            }
                            waiting.emplace_back(to, stack_index + 1);
                        }
                    }
                }
                return std::make_tuple(std::vector<path_state<abstraction>>(), std::vector<uint32_t>());
            }
        }

    private:

        // Returns whether an accepting state in the product automaton was reached.
        bool construct_reachable(std::vector<size_t>& waiting, const automaton_t& initial, const automaton_t& final) {
            while (!waiting.empty()) {
                size_t top = waiting.back();
                waiting.pop_back();
                auto [i_from,f_from] = get_original_ids(top);
                for (const auto& [i_to,i_labels] : initial.states()[i_from]->_edges) {
                    for (const auto& [f_to,f_labels] : final.states()[f_from]->_edges) {
                        std::vector<typename decltype(i_labels)::value_type> labels;
                        std::set_intersection(i_labels.begin(), i_labels.end(), f_labels.begin(), f_labels.end(), std::back_inserter(labels));
                        if (!labels.empty()) {
                            auto [fresh, to_id] = get_product_state(initial.states()[i_to].get(), final.states()[f_to].get());
                            for (const auto & [label, trace] : labels) {
                                _product.add_edge(top, to_id, label, trace);
                            }
                            if (_product.has_accepting_state()) {
                                return true; // Early termination
                            }
                            if (fresh) {
                                waiting.push_back(to_id);
                            }
                        }
                    }
                }
            }
            return _product.has_accepting_state();
        }

        template<typename Elem>
        static std::vector<Elem> intersect_vector(const std::vector<Elem>& v1, const std::vector<Elem>& v2) {
            assert(std::is_sorted(v1.begin(), v1.end()));
            assert(std::is_sorted(v2.begin(), v2.end()));
            std::vector<Elem> result;
            std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(result));
            return result;
        }

        struct pair_size_t { // ptrie does not work with std::pair, so we make struct here.
            size_t first;    // We need std::has_unique_object_representations_v<pair_size_t> to be true.
            size_t second;
            [[nodiscard]] std::pair<size_t,size_t> to_pair() const {
                return std::make_pair(first, second);
            }
        };

        pair_size_t get_original_ids(size_t id) const {
            if (id < _pda_size) {
                return {id,id};
            }
            pair_size_t res;
            _id_map.unpack(id - _pda_size, &res);
            return res;
        }
        std::pair<bool,size_t> get_product_state(const state_t* a, const state_t* b) {
            if (a->_id == b->_id && a->_id < _pda_size) {
                return std::make_pair(false, a->_id);
            }
            auto [fresh, id] = _id_map.insert(pair_size_t{a->_id, b->_id});
            if (fresh) {
                size_t state_id = _product.add_state(false, a->_accepting && b->_accepting);
                assert(state_id == id + _pda_size);
                if (a->_id >= _id_fast_lookup.size()) {
                    _id_fast_lookup.resize(a->_id + 1);
                }
                _id_fast_lookup[a->_id].emplace_back(b->_id, state_id);
                return std::make_pair(true, state_id);
            } else {
                return std::make_pair(false ,id + _pda_size);
            }
        }

        const pda_t _pda;
        const size_t _pda_size;
        automaton_t _initial;
        automaton_t _final;
        product_automaton_t _product;
        bool _swap_initial_final = false;
        ptrie::set_stable<pair_size_t> _id_map;
        std::vector<std::vector<std::pair<size_t,size_t>>> _id_fast_lookup;
    };

    template <typename T, typename W, typename C, typename A>
    class SolverInstance : public SolverInstance_impl<TypedPDA<T,W,C,fut::type::vector>, PAutomaton<W,C,A>, T, W, C, A> {
    public:
        using pda_t = TypedPDA<T,W,C,fut::type::vector>;
        using pautomaton_t = PAutomaton<W,C,A>;
        SolverInstance(pda_t&& pda,
                       const NFA<T>& initial_nfa, const std::vector<size_t>& initial_states,
                       const NFA<T>& final_nfa,   const std::vector<size_t>& final_states)
        : SolverInstance_impl<pda_t, pautomaton_t, T, W, C, A>(std::move(pda), initial_nfa, initial_states, final_nfa, final_states) { };
    };

    template <typename T, typename W, typename C, typename A>
    class AbstractionSolverInstance : public SolverInstance_impl<RefinementPDA<T,W,C>, AbstractionPAutomaton<T,W,C,A>, T, W, C, A> {
    public:
        using pda_t = RefinementPDA<T,W,C>;
        using pautomaton_t = AbstractionPAutomaton<T,W,C,A>;
        AbstractionSolverInstance(pda_t&& pda,
                                  const NFA<T>& initial_nfa, const std::vector<size_t>& initial_states,
                                  const NFA<T>& final_nfa,   const std::vector<size_t>& final_states)
        : SolverInstance_impl<pda_t, pautomaton_t, T, W, C, A>(std::move(pda), initial_nfa, initial_states, final_nfa, final_states) { };
    };

}

#endif //PDAAAL_SOLVERINSTANCE_H
