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
 * File:   DiffTest.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 13-05-2025.
 */

#ifndef DIFFTEST_H
#define DIFFTEST_H

#include <pdaaal/Solver.h>

using namespace pdaaal;

enum class engine_t { prestar, poststar, dualstar };
template <typename instance_t>
bool solve(const instance_t& instance, engine_t engine) {
    auto instance_copy = instance.copy();
    switch (engine) {
    case engine_t::prestar:
        return Solver::pre_star_accepts<Trace_Type::None>(instance_copy);
    case engine_t::poststar:
        return Solver::post_star_accepts<Trace_Type::None>(instance_copy);
    case engine_t::dualstar:
    default:
        return Solver::dual_search_accepts<Trace_Type::None>(instance_copy);
    }
}
template <typename instance_t>
std::optional<typename instance_t::pda_t::weight_type> solve_w(const instance_t& instance, engine_t engine) {
    static_assert(instance_t::pda_t::has_weight, "Can only solve weight on weighted PDA.");
    auto instance_copy = instance.copy();
    switch (engine) {
    case engine_t::prestar:
        if (Solver::pre_star_accepts<Trace_Type::Shortest>(instance_copy)) {
            return Solver::get_trace<Trace_Type::Shortest>(instance_copy).second;
        }
        return std::nullopt;
    case engine_t::poststar:
        if (Solver::post_star_accepts<Trace_Type::Shortest>(instance_copy)) {
            return Solver::get_trace<Trace_Type::Shortest>(instance_copy).second;
        }
        return std::nullopt;
    case engine_t::dualstar:
    default:
        if (Solver::dual_search_accepts<Trace_Type::Shortest>(instance_copy)) {
            return Solver::get_trace_dual_search<Trace_Type::Shortest>(instance_copy).second;
        }
        return std::nullopt;
    }
}
template <typename T>
bool has_different_elements(std::vector<T> v) {
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i] != v[i - 1])
            return true;
    }
    return false;
}
template <typename instance_t>
auto diff_test(instance_t& instance) {
    using W = typename instance_t::pda_t::parent_t::weight;
    using weight_t = std::conditional_t<W::is_weight, typename W::type, bool>;

    std::vector<bool> answers;
    std::vector<weight_t> weights;
    engine_t engines[] = {engine_t::prestar, engine_t::poststar, engine_t::dualstar};
    for (auto engine : engines) {
        bool answer = solve(instance, engine);
        answers.push_back(answer);
        if constexpr (W::is_weight) {
            std::optional<typename W::type> weight = solve_w(instance, engine);
            answers.push_back(weight.has_value());
            if (weight.has_value()) {
                weights.push_back(weight.value());
            }
        }
    }
    return std::make_pair(answers, weights);
}


#endif // DIFFTEST_H
