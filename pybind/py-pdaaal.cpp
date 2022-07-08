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
 * File:   py-pdaaal.cpp
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 07-07-2022.
 */

#include <pybind11/stl.h>

#include <pdaaal/PDA.h>
#include <pdaaal/PAutomaton.h>

namespace py = pybind11;
using namespace pybind11::literals;
using namespace pdaaal;

PYBIND11_MODULE(pdaaal, m) {
    py::enum_<Trace_Type>(m, "Trace_Type")
        .value("None", Trace_Type::None)
        .value("Any", Trace_Type::Any)
        .value("Shortest", Trace_Type::Shortest)
        .value("Longest", Trace_Type::Longest)
        .value("ShortestFixedPoint", Trace_Type::ShortestFixedPoint);

    using pda_t = PDA<std::string>;
    py::class_<pda_t>(m, "PDA")
        .def(py::init<>())
//        .def(py::init<>([](const std::filesystem::path& p){
//            return PdaJsonParser::parse(p);
//        }))
        .def("add_wildcard_rule", &pda_t::add_wildcard_rule);
//        .def("add_rule", py::overload_cast<const pda_t::rule_t&>(&pda_t::add_rule));

    using automaton_t = PAutomaton<std::string,weight<void>,size_t,true>;
    py::class_<automaton_t>(m, "PAutomaton")
        .def(py::init<const pda_t&, const std::vector<size_t>&, bool>())
        .def("to_dot", py::overload_cast<Trace_Type>(&automaton_t::to_dot, py::const_), "trace_type"_a=Trace_Type::None);


}
