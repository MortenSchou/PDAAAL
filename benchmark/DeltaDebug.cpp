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
#include <unordered_set>
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

    std::vector<std::vector<json>> steps(const std::vector<size_t>& failed_ids = std::vector<size_t>()) {
        std::vector<std::vector<json>> result;

        while (_i < _n) {
            auto next = step();
            if (next.has_value()) {
                result.emplace_back(next.value());
            }
        }

        return result;

    }

    void from_json(std::istream& s = std::cin) {
        json j;
        s >> j;
        _n = j["n"].get<size_t>();
        _i = j["i"].get<size_t>();
        _complement = j["complement"].get<bool>();
        _c = j["c"].get<std::vector<json>>();
        if (j.contains("meta")) {
            _meta = j["meta"].get<json>();
        }
    }
    void to_json(std::ostream& s = std::cout) const {
        json j;
        j["n"] = _n;
        j["i"] = _i;
        j["complement"] = _complement;
        j["c"] = _c;
        if (!_meta.is_null()) {
            j["meta"] = _meta;
        }
        s << j.dump() << std::endl;
    }
    std::vector<json>& test_case() {
        return _c;
    }
    json& meta() {
        return _meta;
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
    json _meta;
};

void initiate(std::istream& input_stream) {
    json j_instance; input_stream >> j_instance;
    json& j_pda = j_instance["instance"][1];
    json& j_initial = j_instance["instance"][2];
    json& j_final = j_instance["instance"][3];

    DeltaDebug dd;
    dd.meta() = j_instance["instance"][0];

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

    auto pda_states = j_pda["states"];
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
        for (const auto& accepting_state : automaton["accepting"]) {
            dd.test_case().emplace_back();
            dd.test_case().back()["type"] = type;
            dd.test_case().back()["accepting"] = accepting_state;
        }
        for (const auto& edge : automaton["edges"]) {
            dd.test_case().emplace_back();
            dd.test_case().back()["type"] = type;
            dd.test_case().back()["edge"] = edge;
        }
    };

    visit_automaton(j_initial, 2);
    visit_automaton(j_final, 3);

    dd.to_json();
}

json test_case_to_json(const std::vector<json>& test_case, const json& meta) {
    json j_instance = json::object();
    j_instance["instance"] = {meta,json(),json(),json()};
    bool state_names = meta["state-names"].get<bool>();
    json& j_pda = j_instance["instance"][1];
    json& j_initial = j_instance["instance"][2];
    json& j_final = j_instance["instance"][3];
    j_pda["states"] = state_names ? json::object() : json::array();
    j_initial["accepting"] = json::array(); j_initial["edges"] = json::array();
    j_final["accepting"] = json::array(); j_final["edges"] = json::array();
    for (const auto& feature : test_case) {
        size_t type = feature["type"].get<size_t>();
        switch (type) {
            case 1: {
                // PDA
                if (!feature["from"].is_number_unsigned()) { assert(state_names); }
                json& state = (feature["from"].is_number_unsigned()) ? j_pda["states"][feature["from"].get<size_t>()] : j_pda["states"][feature["from"].get<std::string>()];
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
                assert(state_names != feature["rule"]["to"].is_number_unsigned());
                json& to_state = (state_names) ? j_pda["states"][feature["rule"]["to"].get<std::string>()] : j_pda["states"][feature["rule"]["to"].get<json::size_type>()];
                if (to_state.is_null()) {
                    to_state = json::object();
                }
                break;
            }
            case 2:
            case 3: {
                json& j_automaton = type == 2 ? j_initial : j_final;
                if (feature.contains("accepting")) {
                    j_automaton["accepting"].emplace_back(feature["accepting"]);
                }
                if (feature.contains("edge")) {
                    j_automaton["edges"].emplace_back(feature["edge"]);
                }
                break;
            }
            default:
                std::cerr << "Error: Unknown feature type: " << type;
                exit(-1);
        }
    }

    for (json& state : j_pda["states"]) {
        if (state.is_null()) {
            state = json::object();
        }
    }
    return j_instance;
}

void write_json_file(const json& j, const std::string& file_name) {
    std::ofstream stream(file_name);
    if (!stream.is_open()) {
        std::stringstream es;
        es << "error: Could not open file: " << file_name << std::endl;
        throw std::runtime_error(es.str());
    }
    stream << j.dump() << std::endl;
}


int main(int argc, const char** argv) {
    po::options_description opts;
    opts.add_options()
            ("help,h", "produce help message");

    std::string file_name;
    bool init = false;
    bool step = false;
    bool simplify = false;
//    bool steps = false;
    bool last_test_failed = false;
//    json fail_ids_input;
    opts.add_options()
            ("file,f", po::value<std::string>(&file_name), "File name of (input or output) pushdown json instance.")
            ("init", po::bool_switch(&init), "Initiate delta-debugging.")
            ("step", po::bool_switch(&step), "Next step of delta-debugging.")
            ("simplify", po::bool_switch(&simplify), "Simplify states and labels.")
//            ("steps", po::bool_switch(&step), "Perform multiple step of delta-debugging in one go.")
            ("fail", po::bool_switch(&last_test_failed), "The last test case failed.")
//            ("fail-ids", po::value<json>(&fail_ids_input), "IDs of failing test cases in last iteration.")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, opts), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << opts << std::endl;
        return 1;
    }

    if (init) {
        std::ifstream input_stream(file_name);
        if (!input_stream.is_open()) {
            std::stringstream es;
            es << "error: Could not open pda-file: " << file_name << std::endl;
            throw std::runtime_error(es.str());
        }
        initiate(input_stream);
    }

    if (step) {
        DeltaDebug dd;
        dd.from_json();
        auto next_test_case = dd.step(last_test_failed);
        dd.to_json();

        bool done = !next_test_case.has_value();
        auto test_case = done ? dd.test_case() : next_test_case.value(); // If done, we output the minimal test case.

        auto j_instance = test_case_to_json(test_case, dd.meta());
        write_json_file(j_instance, file_name);

        if (done) { // Indicate in return value whether to continue. (I did not manage to use this from bash, we check for existence of file instead.)
            return 1;
        }
        return 0;
    }

    if (simplify) {
        DeltaDebug dd;
        dd.from_json();
        if (!dd.meta()["state-names"]) { // NOTE: Only works for state_names==false

            // First find all states, since we want to preserve order.
            auto states = std::unordered_set<size_t>();
            for (auto& feature : dd.test_case()) {
                size_t type = feature["type"].get<size_t>();
                switch (type) {
                    case 1: {
                        states.emplace(feature["from"].get<size_t>());
                        states.emplace(feature["rule"]["to"].get<size_t>());
                        break;
                    }
                    case 2:
                    case 3: {
                        if (feature.contains("accepting")) {
                            states.emplace(feature["accepting"].get<size_t>());
                        }
                        if (feature.contains("edge")) {
                            states.emplace(feature["edge"][0].get<size_t>());
                            states.emplace(feature["edge"][2].get<size_t>());
                        }
                        break;
                    }
                    default:
                        std::cerr << "Error: Unknown feature type: " << type;
                        exit(-1);
                }
            }
            std::vector<size_t> states_v(states.begin(), states.end());
            std::sort(states_v.begin(), states_v.end());
            auto state_map = std::unordered_map<size_t,size_t>();
            size_t i = 0;
            for (auto state : states_v) {
                state_map.emplace(state, i);
                ++i;
            }
            auto map_state = [&state_map](size_t state) -> size_t {
                auto it = state_map.find(state);
                assert(it != state_map.end());
                return it->second;
            };
            auto label_map = std::unordered_map<std::string,std::string>();
            std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            size_t index = 0;
            auto map_label = [&label_map,&alphabet,&index](const std::string& label) -> std::string {
                if (label == "*") { // Don't touch the wildcard
                    return label;
                }
                auto it = label_map.find(label);
                if (it != label_map.end()) {
                    return it->second;
                }
                return label_map.emplace(label, index < alphabet.size() ? std::string(1,alphabet[index++]) : label).first->second;
            };
            for (auto& feature : dd.test_case()) {
                size_t type = feature["type"].get<size_t>();
                switch (type) {
                    case 1: {
                        feature["from"] = map_state(feature["from"].get<size_t>());
                        feature["pre"] = map_label(feature["pre"].get<std::string>());
                        if (feature["rule"].contains("swap")) {
                            feature["rule"]["swap"] = map_label(feature["rule"]["swap"].get<std::string>());
                        }
                        if (feature["rule"].contains("push")) {
                            feature["rule"]["push"] = map_label(feature["rule"]["push"].get<std::string>());
                        }
                        feature["rule"]["to"] = map_state(feature["rule"]["to"].get<size_t>());
                        break;
                    }
                    case 2:
                    case 3: {
                        if (feature.contains("accepting")) {
                            feature["accepting"] = map_state(feature["accepting"].get<size_t>());
                        }
                        if (feature.contains("edge")) {
                            feature["edge"][0] = map_state(feature["edge"][0].get<size_t>());
                            feature["edge"][1] = map_label(feature["edge"][1].get<std::string>());
                            feature["edge"][2] = map_state(feature["edge"][2].get<size_t>());
                        }
                        break;
                    }
                    default:
                        std::cerr << "Error: Unknown feature type: " << type;
                        exit(-1);
                }
            }
        }
        dd.to_json();
        write_json_file(test_case_to_json(dd.test_case(), dd.meta()), file_name);
    }

//    if (steps) {
//        if (!fail_ids_input.is_array()) {
//            std::cerr << "Error in argument to --fail-ids:" << fail_ids_input.dump() << ". Must be an array." << std::endl;
//            return -1;
//        }
//        if (!std::all_of(fail_ids_input.begin(), fail_ids_input.end(), [](const json& elem){ return elem.is_number_unsigned(); })) {
//            std::cerr << "Error in argument to --fail-ids:" << fail_ids_input.dump() << ". Elements of array must be unsigned numbers" << std::endl;
//            return -1;
//        }
//        std::vector<size_t> fail_ids;
//        std::transform(fail_ids_input.begin(), fail_ids_input.end(), std::back_inserter(fail_ids), [](const json& elem){ return elem.get<size_t>(); });
//
//        DeltaDebug dd;
//        dd.from_json();
//        auto next_test_cases = dd.steps(fail_ids);
//        dd.to_json();
//
//        fs::path output_dir_path(output_dir);
//        if (!fs::is_directory(output_dir_path)) {
//            std::cerr << "Specified output directory: " << output_dir_path << " is not a valid directory.";
//            return -1;
//        }
//
//        if (next_test_cases.empty()) {
//            auto [j_pda, j_initial, j_final] = test_case_to_json(dd.test_case(), dd.meta());
//            write_json_file(j_pda, "pda-minimal.json", output_dir_path);
//            write_json_file(j_initial, "initial-minimal.json", output_dir_path);
//            write_json_file(j_final, "final-minimal.json", output_dir_path);
//        } else {
//            size_t i = 0;
//            for (const auto& test_case : next_test_cases) {
//                auto [j_pda, j_initial, j_final] = test_case_to_json(test_case, dd.meta());
//                std::stringstream pda_file_name; pda_file_name << "pda" << i << ".json";
//                std::stringstream initial_file_name; initial_file_name << "initial" << i << ".json";
//                std::stringstream final_file_name; final_file_name << "final" << i << ".json";
//                write_json_file(j_pda, pda_file_name.str(), output_dir_path);
//                write_json_file(j_initial, initial_file_name.str(), output_dir_path);
//                write_json_file(j_final, final_file_name.str(), output_dir_path);
//                ++i;
//            }
//        }
//    }

    return 0;
}
