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
 * File:   PAutomaton.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 08-01-2020.
 */

#ifndef PDAAAL_PAUTOMATON_H
#define PDAAAL_PAUTOMATON_H

#include "PDA.h"

#include <memory>
#include <functional>
#include <vector>
#include <stack>
#include <queue>
#include <iostream>
#include <cassert>
#include <boost/functional/hash.hpp>


namespace pdaaal {

    enum class Trace_Type {
        None,
        Any,
        Shortest
    };

    struct trace_t {
        size_t _state = std::numeric_limits<size_t>::max(); // _state = p
        size_t _rule_id = std::numeric_limits<size_t>::max(); // size_t _to = pda.states()[_from]._rules[_rule_id]._to; // _to = q
        uint32_t _label = std::numeric_limits<uint32_t>::max(); // _label = \gamma
        // if is_pre_trace() {
        // then {use _rule_id (and potentially _state)}
        // else if is_post_epsilon_trace()
        // then {_state = q'; _rule_id invalid }
        // else {_state = p; _label = \gamma}

        trace_t() = default;

        trace_t(size_t rule_id, size_t temp_state)
                : _state(temp_state), _rule_id(rule_id), _label(std::numeric_limits<uint32_t>::max() - 1) {};

        trace_t(size_t from, size_t rule_id, uint32_t label)
                : _state(from), _rule_id(rule_id), _label(label) {};

        explicit trace_t(size_t epsilon_state)
                : _state(epsilon_state) {};

        [[nodiscard]] bool is_pre_trace() const {
            return _label == std::numeric_limits<uint32_t>::max() - 1;
        }
        [[nodiscard]] bool is_post_epsilon_trace() const {
            return _label == std::numeric_limits<uint32_t>::max();
        }
    };

    template <typename T> struct label_with_t {
        uint32_t _label = std::numeric_limits<uint32_t>::max();
        T _t;

        explicit label_with_t(T t) // epsilon edge
            : _t(t) {};

        label_with_t(uint32_t label, T t)
            : _label(label), _t(t) {};

        bool operator<(const label_with_t &other) const {
            return _label < other._label;
        }
        bool operator==(const label_with_t &other) const {
            return _label == other._label;
        }
        bool operator!=(const label_with_t &other) const {
            return !(*this == other);
        }
        [[nodiscard]] bool is_epsilon() const {
            return _label == std::numeric_limits<uint32_t>::max();
        }
    };

    template<typename W> using trace_ptr = std::conditional_t<is_weighted<W>, std::pair<const trace_t*, W>, const trace_t*>;
    template<typename W> using label_with_trace_t = label_with_t<trace_ptr<W>>;
    template<typename W> constexpr trace_ptr<W> default_trace_ptr() {
        if constexpr (is_weighted<W>) {
            return std::make_pair<const trace_t*, W>(nullptr, zero<W>()());
        } else {
            return nullptr;
        }
    }
    template<typename W> constexpr trace_ptr<W> trace_ptr_from(const trace_t *trace) {
        if constexpr (is_weighted<W>) {
            return std::make_pair(trace, zero<W>()());
        } else {
            return trace;
        }
    }
    template<typename W> constexpr const trace_t * trace_from(trace_ptr<W> t) {
        if constexpr (is_weighted<W>) {
            return t.first;
        } else {
            return t;
        }
    }


    template <typename W = void, typename C = std::less<W>, typename adder = add<W>>
    class PAutomaton {
    public:
        static constexpr auto epsilon = std::numeric_limits<uint32_t>::max();

        struct state_t;

        struct edge_t {
            state_t *_to;
            std::vector<label_with_trace_t<W>> _labels;

            // edge with a label and optional trace
            edge_t(state_t *to, uint32_t label, trace_ptr<W> trace)
                    : _to(to), _labels() {
                _labels.emplace_back(label, trace);
            };

            // epsilon edge with trace
            edge_t(state_t *to, trace_ptr<W> trace) : _to(to), _labels() {
                _labels.emplace_back(trace);
            };

            // wildcard (all labels), no trace
            edge_t(state_t *to, size_t all_labels) : _to(to), _labels() {
                for (uint32_t label = 0; label < all_labels; label++) {
                    _labels.emplace_back(label, default_trace_ptr<W>());
                }
            };

            void add_label(uint32_t label, trace_ptr<W> trace) {
                label_with_trace_t<W> label_trace{label, trace};
                auto lb = std::lower_bound(_labels.begin(), _labels.end(), label_trace);
                if (lb == std::end(_labels) || *lb != label_trace) {
                    _labels.insert(lb, label_trace);
                }
            }

            std::optional<label_with_trace_t<W>> find(uint32_t label) {
                label_with_trace_t<W> label_trace{label, default_trace_ptr<W>()};
                auto lb = std::lower_bound(_labels.begin(), _labels.end(), label_trace);
                if (lb != std::end(_labels) && *lb == label_trace) {
                    return std::optional<label_with_trace_t<W>>(*lb);
                }
                return std::nullopt;
            }

            bool contains(uint32_t label) {
                label_with_trace_t<W> label_trace{label, default_trace_ptr<W>()};
                auto lb = std::lower_bound(_labels.begin(), _labels.end(), label_trace);
                return lb != std::end(_labels) && *lb == label_trace;
            }

            [[nodiscard]] bool has_epsilon() const { return !_labels.empty() && _labels.back().is_epsilon(); }
            [[nodiscard]] bool has_non_epsilon() const { return !_labels.empty() && !_labels[0].is_epsilon(); }
        };

        struct state_t {
            bool _accepting = false;
            size_t _id;
            std::vector<edge_t> _edges;

            state_t(bool accepting, size_t id) : _accepting(accepting), _id(id) {};

            state_t(const state_t &other) = default;
        };

    public:
        // Accept one control state with given stack.
        PAutomaton(const PDA<W,C> &pda, size_t initial_state, const std::vector<uint32_t> &initial_stack) : _pda(pda) {
            const size_t size = pda.states().size();
            const size_t accepting = initial_stack.empty() ? initial_state : size;
            for (size_t i = 0; i < size; ++i) {
                add_state(true, i == accepting);
            }
            auto last_state = initial_state;
            for (size_t i = 0; i < initial_stack.size(); ++i) {
                auto state = add_state(false, i == initial_stack.size() - 1);
                add_edge(last_state, state, initial_stack[i]);
                last_state = state;
            }
        };

        PAutomaton(PAutomaton &&) noexcept = default;

        PAutomaton(const PAutomaton &other) : _pda(other._pda) {
            std::unordered_map<state_t *, state_t *> indir;
            for (auto &s : other._states) {
                _states.emplace_back(std::make_unique<state_t>(*s));
                indir[s.get()] = _states.back().get();
            }
            // fix links
            for (auto &s : _states) {
                for (auto &e : s->_edges) {
                    e._to = indir[e._to];
                }
            }
            for (auto &s : other._accepting) {
                _accepting.push_back(indir[s]);
            }
            for (auto &s : other._initial) {
                _initial.push_back(indir[s]);
            }
        }

        [[nodiscard]] const std::vector<std::unique_ptr<state_t>> &states() const { return _states; }
        [[nodiscard]] const std::vector<state_t *> &accepting_states() const { return _accepting; }
        
        [[nodiscard]] const PDA<W,C> &pda() const { return _pda; }

        void to_dot(std::ostream &out, const std::function<void(std::ostream &, const label_with_trace_t<W> &)> &printer = [](auto &s, auto &e) {
                        s << e._label;
                    }) const {
            out << "digraph NFA {\n";
            for (auto &s : _states) {
                out << "\"" << s->_id << "\" [shape=";
                if (s->_accepting)
                    out << "double";
                out << "circle];\n";
                for (const edge_t &e : s->_edges) {
                    out << "\"" << s->_id << "\" -> \"" << e._to->_id << "\" [ label=\"";
                    if (e.has_non_epsilon()) {
                        out << "\\[";
                        bool first = true;
                        for (auto &l : e._labels) {
                            if (l.is_epsilon()) { continue; }
                            if (!first)
                                out << ", ";
                            first = false;
                            printer(out, l);
                        }
                        out << "\\]";
                    }
                    if (e._labels.size() == number_of_labels())
                        out << "*";
                    if (e.has_epsilon()) {
                        if (!e._labels.empty()) out << " ";
                        out << u8"𝜀";
                    }

                    out << "\"];\n";
                }
            }
            for (auto &i : _initial) {
                out << "\"I" << i->_id << "\" -> \"" << i->_id << "\";\n";
                out << "\"I" << i->_id << "\" [style=invisible];\n";
            }

            out << "}\n";
        }

        [[nodiscard]] bool accepts(size_t state, const std::vector<uint32_t> &stack) const {
            //Equivalent to (but hopefully faster than): return !_accept_path(state, stack).empty();

            if (stack.empty()) {
                return _states[state]->_accepting;
            }
            // DFS search.
            std::stack<std::pair<size_t, size_t>> search_stack;
            search_stack.emplace(state, 0);
            while (!search_stack.empty()) {
                auto current = search_stack.top();
                search_stack.pop();
                auto current_state = current.first;
                auto stack_index = current.second;
                for (auto &edge : _states[current_state]->_edges) {
                    if (edge.contains(stack[stack_index])) {
                        auto to = edge._to->_id;
                        if (stack_index + 1 < stack.size()) {
                            search_stack.emplace(to, stack_index + 1);
                        } else if (edge._to->_accepting) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        template<Trace_Type trace_type = Trace_Type::Any>
        [[nodiscard]] typename std::conditional_t<trace_type == Trace_Type::Shortest && is_weighted<W>,
                std::pair<std::vector<size_t>, W>, std::vector<size_t>>
        accept_path(size_t state, const std::vector<uint32_t> &stack) const {
            if constexpr (trace_type == Trace_Type::Shortest && is_weighted<W>) { // TODO: Consider unweighted shortest path.
                if (stack.empty()) {
                    if (_states[state]->_accepting) {
                        return std::make_pair(std::vector<size_t>{state}, zero<W>()());
                    } else {
                        return std::make_pair(std::vector<size_t>(), max<W>()());
                    }
                }
                // Dijkstra.
                struct queue_elem {
                    W weight;
                    size_t state;
                    size_t stack_index;
                    const queue_elem *back_pointer;
                    queue_elem(W weight, size_t state, size_t stack_index, const queue_elem *back_pointer = nullptr)
                    : weight(weight), state(state), stack_index(stack_index), back_pointer(back_pointer) {};

                    bool operator<(const queue_elem &other) const {
                        if (state != other.state) {
                            return state < other.state;
                        }
                        return stack_index < other.stack_index;
                    }
                    bool operator==(const queue_elem &other) const {
                        return state == other.state && stack_index == other.stack_index;
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
                adder add;
                std::priority_queue<queue_elem, std::vector<queue_elem>, queue_elem_comp> search_queue;
                std::vector<queue_elem> visited;
                std::vector<std::unique_ptr<queue_elem>> pointers;
                search_queue.emplace(zero<W>()(), state, 0);
                while(!search_queue.empty()) {
                    auto current = search_queue.top();
                    search_queue.pop();
                    if (current.stack_index == stack.size()) {
                        std::vector<size_t> path(stack.size() + 1);
                        path[current.stack_index] = current.state;
                        for (auto p = current.back_pointer; p != nullptr; p = p->back_pointer) {
                            path[p->stack_index] = p->state;
                        }
                        return std::make_pair(path, current.weight);
                    }
                    auto lb = std::lower_bound(visited.begin(), visited.end(), current);
                    if (lb != std::end(visited) && *lb == current) {
                        if (less(*lb, current)) {
                            *lb = current;
                        } else {
                            break;
                        }
                    } else {
                        lb = visited.insert(lb, current);
                    }
                    auto u_pointer = std::make_unique<queue_elem>(*lb);
                    auto pointer = u_pointer.get();
                    pointers.push_back(std::move(u_pointer));
                    for (auto &edge : _states[current.state]->_edges) {
                        auto label = edge.find(stack[current.stack_index]);
                        if (label) {
                            if (current.stack_index + 1 < stack.size() || edge._to->_accepting) {
                                search_queue.emplace(add(current.weight, label->_t.second), edge._to->_id, current.stack_index + 1, pointer);
                            }
                        }
                    }
                }
                return std::make_pair(std::vector<size_t>(), max<W>()());
            } else {
                if (stack.empty()) {
                    if (_states[state]->_accepting) {
                        return std::vector<size_t>{state};
                    } else {
                        return std::vector<size_t>();
                    }
                }
                // DFS search.
                std::vector<size_t> path(stack.size() + 1);
                std::stack<std::pair<size_t, size_t>> search_stack;
                search_stack.emplace(state, 0);
                while (!search_stack.empty()) {
                    auto current = search_stack.top();
                    search_stack.pop();
                    auto current_state = current.first;
                    auto stack_index = current.second;
                    path[stack_index] = current_state;
                    for (auto &edge : _states[current_state]->_edges) {
                        if (edge.contains(stack[stack_index])) {
                            auto to = edge._to->_id;
                            if (stack_index + 1 < stack.size()) {
                                search_stack.emplace(to, stack_index + 1);
                            } else if (edge._to->_accepting) {
                                path[stack_index + 1] = to;
                                return path;
                            }
                        }
                    }
                }
                return std::vector<size_t>();
            }
        }

        [[nodiscard]] const trace_t *get_trace_label(const std::tuple<size_t, uint32_t, size_t> &edge) const {
            return get_trace_label(std::get<0>(edge), std::get<1>(edge), std::get<2>(edge));
        }
        [[nodiscard]] const trace_t *get_trace_label(size_t from, uint32_t label, size_t to) const {
            for (auto &e : _states[from]->_edges) {
                if (e._to->_id == to) {
                    label_with_trace_t<W> label_trace{label, default_trace_ptr<W>()};
                    auto lb = std::lower_bound(e._labels.begin(), e._labels.end(), label_trace);
                    assert(lb != std::end(e._labels)); // We assume the edge exists.
                    return trace_from<W>(lb->_t);
                }
            }
            assert(false); // We assume the edge exists.
            return nullptr;
        }

        [[nodiscard]] size_t number_of_labels() const { return _pda.number_of_labels(); }

        size_t add_state(bool initial, bool accepting) {
            auto id = next_state_id();
            _states.emplace_back(std::make_unique<state_t>(accepting, id));
            if (accepting) {
                _accepting.push_back(_states.back().get());
            }
            if (initial) {
                _initial.push_back(_states.back().get());
            }
            return id;
        }
        [[nodiscard]] size_t next_state_id() const {
            return _states.size();
        }

        void add_epsilon_edge(size_t from, size_t to, trace_ptr<W> trace = default_trace_ptr<W>()) {
            auto &edges = _states[from]->_edges;
            for (auto &e : edges) {
                if (e._to->_id == to) {
                    if (!e._labels.back().is_epsilon()) {
                        e._labels.emplace_back(trace);
                    }
                    return;
                }
            }
            edges.emplace_back(_states[to].get(), trace);
        }

        void add_edge(size_t from, size_t to, uint32_t label, trace_ptr<W> trace = default_trace_ptr<W>()) {
            assert(label < std::numeric_limits<uint32_t>::max() - 1);
            auto &edges = _states[from]->_edges;
            for (auto &e : edges) {
                if (e._to->_id == to) {
                    e.add_label(label, trace);
                    return;
                }
            }
            edges.emplace_back(_states[to].get(), label, trace);
        }

        void add_wildcard(size_t from, size_t to) {
            auto &edges = _states[from]->_edges;
            for (auto &e : edges) {
                if (e._to->_id == to) {
                    e._labels.clear();
                    for (uint32_t i = 0; i < number_of_labels(); i++) {
                        e._labels.emplace_back(i, default_trace_ptr<W>());
                    }
                    return;
                }
            }
            edges.emplace_back(_states[to].get(), number_of_labels());
        }

        const trace_t *new_pre_trace(size_t rule_id) {
            _trace_info.emplace_back(std::make_unique<trace_t>(rule_id, std::numeric_limits<size_t>::max()));
            return _trace_info.back().get();
        }
        const trace_t *new_pre_trace(size_t rule_id, size_t temp_state) {
            _trace_info.emplace_back(std::make_unique<trace_t>(rule_id, temp_state));
            return _trace_info.back().get();
        }
        const trace_t *new_post_trace(size_t from, size_t rule_id, uint32_t label) {
            _trace_info.emplace_back(std::make_unique<trace_t>(from, rule_id, label));
            return _trace_info.back().get();
        }
        const trace_t *new_post_trace(size_t epsilon_state) {
            _trace_info.emplace_back(std::make_unique<trace_t>(epsilon_state));
            return _trace_info.back().get();
        }
    private:
        std::vector<std::unique_ptr<state_t>> _states;
        std::vector<state_t *> _initial;
        std::vector<state_t *> _accepting;

        std::vector<std::unique_ptr<trace_t>> _trace_info;

        const PDA<W,C> &_pda;
    };


}

#endif //PDAAAL_PAUTOMATON_H
