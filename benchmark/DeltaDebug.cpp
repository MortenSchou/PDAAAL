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
//        auto visit_state = [&dd,type](const json& state, const json& from){
//            for (const auto& edge : state["edges"]) {
//                dd.test_case().emplace_back();
//                dd.test_case().back()["type"] = type;
//                dd.test_case().back()["from"] = from;
//                dd.test_case().back()["edge"] = edge;
//            }
//            if (state.contains("accepting") && state["accepting"].get<bool>()) {
//                dd.test_case().emplace_back();
//                dd.test_case().back()["type"] = type;
//                dd.test_case().back()["from"] = from;
//                dd.test_case().back()["accepting"] = true;
//            }
//        };
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
//        auto automaton_states = automaton["states"];
//        if (automaton_states.is_array()) {
//            size_t from = 0;
//            for (const auto& state : automaton_states) {
//                json j_from = from;
//                visit_state(state, j_from);
//                ++from;
//            }
//        } else {
//            assert(automaton_states.is_object());
//            for (const auto& [from, state] : automaton_states.items()) {
//                json j_from = from;
//                visit_state(state, j_from);
//            }
//        }
    };

    visit_automaton(j_initial, 2);
    visit_automaton(j_final, 3);

    dd.to_json();
}

json test_case_to_json(const std::vector<json>& test_case, const json& meta) {
    json j_instance = json::object();
    j_instance["instance"] = json::array();
    j_instance["instance"].emplace_back(meta);
    bool state_names = meta["state-names"].get<bool>();
    j_instance["instance"].emplace_back();
    j_instance["instance"].emplace_back();
    j_instance["instance"].emplace_back();
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
    if (state_names) {
        for (const auto& [state_name, _] : j_pda["states"].items()) {
            j_initial["states"][state_name]["initial"] = true;
            j_final["states"][state_name]["initial"] = true;
        }
        for (auto& [_, state] : j_initial["states"].items()) {
            if (!state.contains("edges")) state["edges"] = json::array();
        }
        for (auto& [_, state] : j_final["states"].items()) {
            if (!state.contains("edges")) state["edges"] = json::array();
        }
    } else {
        size_t num_pda_states = j_pda["states"].size();
        size_t i = 0;
        for (json& state : j_initial["states"]) {
            if (!state.contains("edges")) state["edges"] = json::array();
            if (i < num_pda_states) state["initial"] = true;
            i++;
        }
        i=0;
        for (json& state : j_final["states"]) {
            if (!state.contains("edges")) state["edges"] = json::array();
            if (i < num_pda_states) state["initial"] = true;
            i++;
        }
    }
    return j_instance;
}

//void write_json_file(const json& j, const std::string& file_name, const fs::path& output_dir_path) {
//    auto file_path = output_dir_path / file_name;
//    std::ofstream stream(file_path);
//    if (!stream.is_open()) {
//        std::stringstream es;
//        es << "error: Could not open file: " << file_path << std::endl;
//        throw std::runtime_error(es.str());
//    }
//    stream << j.dump() << std::endl;
//}


int main(int argc, const char** argv) {
    po::options_description opts;
    opts.add_options()
            ("help,h", "produce help message");

//    size_t pds_id = 0;
//    size_t initial_id = 0;
//    size_t final_id = 0;
//    std::string input_dir, output_dir;
    std::string file_name;
    bool init = false;
    bool step = false;
//    bool steps = false;
    bool last_test_failed = false;
//    json fail_ids_input;
    opts.add_options()
            ("file,f", po::value<std::string>(&file_name), "File name of (input or output) pushdown json instance.")
//            ("pds,p", po::value<size_t>(&pds_id), "Index of pushdown")
//            ("initial,i", po::value<size_t>(&initial_id), "Index of initial P-automaton")
//            ("final,f", po::value<size_t>(&final_id), "Index of final P-automaton")
//            ("dir,d", po::value<std::string>(&input_dir), "Input directory to read files from.")
//            ("out-dir,o", po::value<std::string>(&output_dir), "Output directory to write files to.")
            ("init", po::bool_switch(&init), "Initiate delta-debugging.")
            ("step", po::bool_switch(&step), "Next step of delta-debugging.")
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

        std::ofstream stream(file_name);
        if (!stream.is_open()) {
            std::stringstream es;
            es << "error: Could not open file: " << file_name << std::endl;
            throw std::runtime_error(es.str());
        }
        stream << j_instance.dump() << std::endl;

        if (!done) { // Indicate in return value whether to continue. (I did not manage to use this from bash, we check for existence of file instead.)
            return 1;
        }
        return 0;
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
