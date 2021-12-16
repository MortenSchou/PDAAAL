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
 * File:   DeltaDebug.cpp
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 14-12-2021.
 */

#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <optional>
#include <boost/program_options.hpp>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
namespace po = boost::program_options;
using json = nlohmann::json;

class DeltaDebug {
public:
    std::optional<std::vector<json>> step(bool last_test_failed = false) {
        assert(!_c.empty());
        if (last_test_failed) {
            _c = partition(_c, _n, _i - 1, _complement);
            assert(!_c.empty());
            if (_complement) {
                _n--;
                if (_n == 2) {
                    _complement = false;
                }
            } else {
                _n = 2;
            }
            _i = 0;
        }
        std::vector<json> next_output;

        if (!_complement && _i == _n && _n > 2) { // When reach end of non-complement, start with complements. Don't do complement when n==2 (equal to non-complement).
            _complement = true;
            _i = 0;
        }
        if (_i == _n) { // Iterations done with current granularity
            if (_n == _c.size()) { // All done, since all instances of the smallest possible change is covered.
                return std::nullopt;
            }
            // Increase granularity, and restart iterations
            _n = std::min(_c.size(), 2*_n);
            _i = 0;
            _complement = false;
        }
        assert(_i < _n);
        next_output = partition(_c, _n, _i, _complement);
        _i++;

        return next_output;
    }

    void from_json(std::istream& s = std::cin) {
        json j;
        s >> j;
        _n = j["n"].get<size_t>();
        _i = j["i"].get<size_t>();
        _complement = j["complement"].get<bool>();
        _c = j["c"].get<std::vector<json>>();
    }
    void to_json(std::ostream& s = std::cout) const {
        json j;
        j["n"] = _n;
        j["i"] = _i;
        j["complement"] = _complement;
        j["c"] = _c;
        s << j.dump() << std::endl;
    }
    std::vector<json>& test_case() {
        return _c;
    }

private:
    static std::vector<json> partition(const std::vector<json>& c, size_t n, size_t i, bool complement) {
        size_t step = c.size() / n;
        size_t remainder = c.size() % n;
        std::vector<json> result;

        if (i < remainder) step++;
        size_t from = i * step;
        if (i >= remainder) from += remainder;

        if (complement) {
            for (size_t j = 0; j < from; j++) {
                result.emplace_back(c[j]);
            }
            for (size_t j = from + step; j < c.size(); j++) {
                result.emplace_back(c[j]);
            }
        } else {
            for (size_t j = from; j < from + step; j++) {
                result.emplace_back(c[j]);
            }
        }
        return result;
    }

    std::vector<json> _c;
    size_t _n = 2;
    size_t _i = 0;
    bool _complement = false;
};

void initiate(std::istream& pda_stream, std::istream& initial_stream, std::istream& final_stream) {
    json j_pda; pda_stream >> j_pda;
    json j_initial; initial_stream >> j_initial;
    json j_final; final_stream >> j_final;

    DeltaDebug dd;

    auto visit_rule = [&dd](const json& rule, const json& from, const json& label){
        dd.test_case().emplace_back();
        dd.test_case().back()["type"] = 1;
        dd.test_case().back()["from"] = from;
        dd.test_case().back()["pre"] = label;
        dd.test_case().back()["rule"] = rule;
    };
    auto visit_pda_state = [&visit_rule](const json& state, const json& from){
        for (const auto& [label, rules] : state.items()) {
            json j_label = label;
            if (rules.is_array()) {
                for (const auto& rule : rules) {
                    visit_rule(rule, from, label);
                }
            } else {
                visit_rule(rules, from, label);
            }
        }
    };

    auto pda_states = j_pda["pda"]["states"];
    if (pda_states.is_array()) {
        size_t from = 0;
        for (const auto& state : pda_states) {
            json j_from = from;
            visit_pda_state(state, j_from);
            ++from;
        }
    } else {
        assert(pda_states.is_object());
        for (const auto& [from, state] : pda_states.items()) {
            json j_from = from;
            visit_pda_state(state, j_from);
        }
    }
    auto visit_automaton = [&dd](const json& automaton, size_t type){
        auto visit_state = [&dd,type](const json& state, const json& from){
            for (const auto& edge : state["edges"]) {
                dd.test_case().emplace_back();
                dd.test_case().back()["type"] = type;
                dd.test_case().back()["from"] = from;
                dd.test_case().back()["edge"] = edge;
            }
//            if (state.contains("initial") && state["initial"].get<bool>()) {
//                dd.test_case().emplace_back();
//                dd.test_case().back()["type"] = type;
//                dd.test_case().back()["from"] = from;
//                dd.test_case().back()["initial"] = true;
//            }
            if (state.contains("accepting") && state["accepting"].get<bool>()) {
                dd.test_case().emplace_back();
                dd.test_case().back()["type"] = type;
                dd.test_case().back()["from"] = from;
                dd.test_case().back()["accepting"] = true;
            }
        };

        auto automaton_states = automaton["states"];
        if (automaton_states.is_array()) {
            size_t from = 0;
            for (const auto& state : automaton_states) {
                json j_from = from;
                visit_state(state, j_from);
                ++from;
            }
        } else {
            assert(automaton_states.is_object());
            for (const auto& [from, state] : automaton_states.items()) {
                json j_from = from;
                visit_state(state, j_from);
            }
        }
    };

    visit_automaton(j_initial["P-automaton"], 2);
    visit_automaton(j_final["P-automaton"], 3);

    dd.to_json();
}


int main(int argc, const char** argv) {
    po::options_description opts;
    opts.add_options()
            ("help,h", "produce help message");

    size_t pds_id = 0;
    size_t initial_id = 0;
    size_t final_id = 0;
    std::string input_dir, output_dir;
    bool init = false;
    bool step = false;
    bool last_test_failed = false;
    opts.add_options()
            ("pds,p", po::value<size_t>(&pds_id), "Index of pushdown")
            ("initial,i", po::value<size_t>(&initial_id), "Index of initial P-automaton")
            ("final,f", po::value<size_t>(&final_id), "Index of final P-automaton")
            ("dir,d", po::value<std::string>(&input_dir), "Input directory to read files from.")
            ("out-dir,o", po::value<std::string>(&output_dir), "Output directory to write files to.")
            ("init", po::bool_switch(&init), "Initiate delta-debugging.")
            ("step", po::bool_switch(&step), "Next step of delta-debugging.")
            ("fail", po::bool_switch(&last_test_failed), "The last test case failed.")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, opts), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        return 1;
    }

    if (init) {
        if (input_dir.empty()) {
            std::cerr << "Please specify an input directory";
            return 1;
        }
        fs::path input_dir_path(input_dir);
        if (!fs::is_directory(input_dir_path)) {
            std::cerr << "Specified input directory: " << input_dir_path << " is not a valid directory.";
            return 1;
        }

        std::stringstream pda_file_name;
        pda_file_name << "pda" << pds_id << ".json";
        auto pda_file_path = input_dir_path / pda_file_name.str();
        std::stringstream initial_file_name;
        initial_file_name << "initial" << initial_id << ".json";
        auto initial_file_path = input_dir_path / initial_file_name.str();
        std::stringstream final_file_name;
        final_file_name << "final" << final_id << ".json";
        auto final_file_path = input_dir_path / final_file_name.str();

        std::ifstream pda_stream(pda_file_path);
        if (!pda_stream.is_open()) {
            std::stringstream es;
            es << "error: Could not open pda-file: " << pda_file_path << std::endl;
            throw std::runtime_error(es.str());
        }
        std::ifstream initial_stream(initial_file_path);
        if (!initial_stream.is_open()) {
            std::stringstream es;
            es << "error: Could not open file: " << initial_file_path << std::endl;
            throw std::runtime_error(es.str());
        }
        std::ifstream final_stream(final_file_path);
        if (!final_stream.is_open()) {
            std::stringstream es;
            es << "error: Could not open file: " << final_file_path << std::endl;
            throw std::runtime_error(es.str());
        }
        initiate(pda_stream, initial_stream, final_stream);
    }

    if (step) {
        DeltaDebug dd;
        dd.from_json();
        auto next_test_case = dd.step(last_test_failed);
        dd.to_json();

        if (next_test_case.has_value()) {
            fs::path output_dir_path(output_dir);
            if (!fs::is_directory(output_dir_path)) {
                std::cerr << "Specified output directory: " << output_dir_path << " is not a valid directory.";
                return -1;
            }

            std::stringstream pda_file_name;
            pda_file_name << "pda" << pds_id << ".json";
            auto pda_file_path = output_dir_path / pda_file_name.str();
            std::stringstream initial_file_name;
            initial_file_name << "initial" << initial_id << ".json";
            auto initial_file_path = output_dir_path / initial_file_name.str();
            std::stringstream final_file_name;
            final_file_name << "final" << final_id << ".json";
            auto final_file_path = output_dir_path / final_file_name.str();

            json j_pda; j_pda["pda"]["states"] = json();
            json j_initial; j_initial["P-automaton"]["states"] = json();
            json j_final; j_final["P-automaton"]["states"] = json();
            bool state_names = false;
            for (const auto& feature : next_test_case.value()) {
                size_t type = feature["type"].get<size_t>();
                if (!feature["from"].is_number_unsigned()) state_names = true;
                switch (type) {
                    case 1: {
                        // PDA
                        json& state = (feature["from"].is_number_unsigned()) ? j_pda["pda"]["states"][feature["from"].get<size_t>()] : j_pda["pda"]["states"][feature["from"].get<std::string>()];
                        auto label = feature["pre"].get<std::string>();
                        if (state.contains(label)) {
                            if (!state[label].is_array()) {
                                auto temp_rule = state[label];
                                state[label] = json::array();
                                state[label].emplace_back(temp_rule);
                            }
                            state[label].emplace_back(feature["rule"]);
                        } else {
                            state[label] = feature["rule"];
                        }
                        break;
                    }
                    case 2:
                    case 3: {
                        json& j_automaton = type == 2 ? j_initial : j_final;
                        json& state = (feature["from"].is_number_unsigned()) ? j_automaton["P-automaton"]["states"][feature["from"].get<size_t>()] : j_automaton["P-automaton"]["states"][feature["from"].get<std::string>()];
                        if (feature.contains("accepting")) {
                            assert(feature["accepting"].get<bool>());
                            state["accepting"] = true;
                        }
                        if (feature.contains("edge")) {
                            state["edges"].emplace_back(feature["edge"]);
                        }
                        break;
                    }
                    default:
                        std::cerr << "Error: Unknown feature type: " << type;
                        return -1;
                }
            }

            if (j_pda["pda"]["states"].empty()) j_pda["pda"]["states"] = state_names ? json::object() : json::array();
            if (j_initial["P-automaton"]["states"].empty()) j_initial["P-automaton"]["states"] = state_names ? json::object() : json::array();
            if (j_final["P-automaton"]["states"].empty()) j_final["P-automaton"]["states"] = state_names ? json::object() : json::array();

            for (json& state : j_pda["pda"]["states"]) {
                if (state.is_null()) {
                    state = json::object();
                }
            }

            size_t num_pda_states = j_pda["pda"]["states"].size();
            size_t i = 0;
            for (json& state : j_initial["P-automaton"]["states"]) {
                if (state.is_null()) {
                    state["edges"] = json::array();
                }
                if (i < num_pda_states) {
                    state["initial"] = true;
                }
                i++;
            }
            i=0;
            for (json& state : j_final["P-automaton"]["states"]) {
                if (state.is_null()) {
                    state["edges"] = json::array();
                }
                if (i < num_pda_states) {
                    state["initial"] = true;
                }
                i++;
            }

            std::ofstream pda_stream(pda_file_path);
            if (!pda_stream.is_open()) {
                std::stringstream es;
                es << "error: Could not open pda-file: " << pda_file_path << std::endl;
                throw std::runtime_error(es.str());
            }
            std::ofstream initial_stream(initial_file_path);
            if (!initial_stream.is_open()) {
                std::stringstream es;
                es << "error: Could not open file: " << initial_file_path << std::endl;
                throw std::runtime_error(es.str());
            }
            std::ofstream final_stream(final_file_path);
            if (!final_stream.is_open()) {
                std::stringstream es;
                es << "error: Could not open file: " << final_file_path << std::endl;
                throw std::runtime_error(es.str());
            }
            pda_stream << j_pda.dump() << std::endl;
            initial_stream << j_initial.dump() << std::endl;
            final_stream << j_final.dump() << std::endl;
            return 1;
        }
        return 0;
    }

    return 0;
}
