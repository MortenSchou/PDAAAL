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
 * File:   PAutomaton_test.cpp
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 19-02-2020.
 */

#define BOOST_TEST_MODULE PAutomaton

#include <boost/test/unit_test.hpp>
#include <pdaaal/PAutomaton.h>
#include <pdaaal/TypedPDA.h>
#include <pdaaal/Solver.h>
#include <chrono>

using namespace pdaaal;

BOOST_AUTO_TEST_CASE(UnweightedPreStar)
{
    // This is pretty much the rules from the example in Figure 3.1 (Schwoon-php02)
    // However r_2 requires a swap and a push, which is done through auxiliary state 3.
    std::unordered_set<char> labels{'A', 'B', 'C'};
    TypedPDA<char> pda(labels);
    pda.add_rule(0, 1, PUSH, 'B', false, 'A');
    pda.add_rule(0, 0, POP, '*', false, 'B');
    pda.add_rule(1, 3, SWAP, 'A', false, 'B');
    pda.add_rule(2, 0, SWAP, 'B', false, 'C');
    pda.add_rule(3, 2, PUSH, 'C', false, 'A');

    std::vector<char> init_stack{'A', 'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::pre_star(automaton);

    std::vector<char> test_stack_reachable{'C', 'B', 'B', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(2, pda.encode_pre(test_stack_reachable)), true);

    std::vector<char> test_stack_unreachable{'C', 'A', 'B', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(2, pda.encode_pre(test_stack_unreachable)), false);
}

BOOST_AUTO_TEST_CASE(UnweightedPostStar)
{
    // This is pretty much the rules from the example in Figure 3.1 (Schwoon-php02)
    // However r_2 requires a swap and a push, which is done through auxiliary state 3.
    std::unordered_set<char> labels{'A', 'B', 'C'};
    TypedPDA<char> pda(labels);
    pda.add_rule(0, 1, PUSH, 'B', false, 'A');
    pda.add_rule(0, 0, POP, '*', false, 'B');
    pda.add_rule(1, 3, SWAP, 'A', false, 'B');
    pda.add_rule(2, 0, SWAP, 'B', false, 'C');
    pda.add_rule(3, 2, PUSH, 'C', false, 'A');

    std::vector<char> init_stack{'A', 'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::post_star(automaton);

    std::vector<char> test_stack_reachable{'B', 'A', 'A', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(1, pda.encode_pre(test_stack_reachable)), true);

    std::vector<char> test_stack_unreachable{'A', 'A', 'B', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(0, pda.encode_pre(test_stack_unreachable)), false);

    automaton.to_dot(std::cout, [&pda](auto &s, auto &l) { s << pda.get_symbol(l); });

}

BOOST_AUTO_TEST_CASE(UnweightedPostStarPath)
{
    // This is pretty much the rules from the example in Figure 3.1 (Schwoon-php02)
    // However r_2 requires a swap and a push, which is done through auxiliary state 3.
    std::unordered_set<char> labels{'A', 'B', 'C'};
    TypedPDA<char> pda(labels);
    pda.add_rule(0, 1, PUSH, 'B', false, 'A');
    pda.add_rule(0, 0, POP, '*', false, 'B');
    pda.add_rule(1, 3, SWAP, 'A', false, 'B');
    pda.add_rule(2, 0, SWAP, 'B', false, 'C');
    pda.add_rule(3, 2, PUSH, 'C', false, 'A');

    std::vector<char> init_stack{'A', 'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::post_star(automaton);

    std::vector<char> test_stack_reachable{'B', 'A', 'A', 'A'};
    BOOST_CHECK_EQUAL(automaton.accept_path(1, pda.encode_pre(test_stack_reachable)).size(), 5);

    std::vector<char> test_stack_unreachable{'A', 'A', 'B', 'A'};
    BOOST_CHECK_EQUAL(automaton.accept_path(0, pda.encode_pre(test_stack_unreachable)).size(), 0);
}

BOOST_AUTO_TEST_CASE(WeightedPreStar)
{
    // This is pretty much the rules from the example in Figure 3.1 (Schwoon-php02)
    // However r_2 requires a swap and a push, which is done through auxiliary state 3.
    std::unordered_set<char> labels{'A', 'B', 'C'};
    TypedPDA<char, std::vector<int>> pda(labels);
    std::vector<int> w{1};
    pda.add_rule(0, 1, PUSH, 'B', false, 'A', w);
    pda.add_rule(0, 0, POP , '*', false, 'B', w);
    pda.add_rule(1, 3, SWAP, 'A', false, 'B', w);
    pda.add_rule(2, 0, SWAP, 'B', false, 'C', w);
    pda.add_rule(3, 2, PUSH, 'C', false, 'A', w);

    std::vector<char> init_stack{'A', 'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::pre_star(automaton);

    std::vector<char> test_stack_reachable{'C', 'B', 'B', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(2, pda.encode_pre(test_stack_reachable)), true);

    std::vector<char> test_stack_unreachable{'C', 'A', 'B', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(2, pda.encode_pre(test_stack_unreachable)), false);
}

BOOST_AUTO_TEST_CASE(WeightedPostStar)
{
    // This is pretty much the rules from the example in Figure 3.1 (Schwoon-php02)
    // However r_2 requires a swap and a push, which is done through auxiliary state 3.
    std::unordered_set<char> labels{'A', 'B', 'C'};
    TypedPDA<char, std::array<double, 3>> pda(labels);
    std::array<double, 3> w{0.5, 1.2, 0.3};
    pda.add_rule(0, 1, PUSH, 'B', false, 'A', w);
    pda.add_rule(0, 0, POP , '*', false, 'B', w);
    pda.add_rule(1, 3, SWAP, 'A', false, 'B', w);
    pda.add_rule(2, 0, SWAP, 'B', false, 'C', w);
    pda.add_rule(3, 2, PUSH, 'C', false, 'A', w);

    std::vector<char> init_stack{'A', 'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::post_star<Trace_Type::Shortest>(automaton);

    std::vector<char> test_stack_reachable{'B', 'A', 'A', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(1, pda.encode_pre(test_stack_reachable)), true);

    std::vector<char> test_stack_unreachable{'A', 'A', 'B', 'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(0, pda.encode_pre(test_stack_unreachable)), false);
}

BOOST_AUTO_TEST_CASE(WeightedPostStar2)
{
    std::unordered_set<char> labels{'A', 'B'};
    TypedPDA<char, int> pda(labels);

    pda.add_rule(1, 2, POP, '*', false, 'A', 1);
    pda.add_rule(1, 3, PUSH , 'B', false, 'A', 3);
    pda.add_rule(1, 3, SWAP, 'A',  false, 'B', 2);
    pda.add_rule(2, 1, POP, '*',  false, 'B', 4);
    std::vector<char> pre{'A', 'B'};
    pda.add_rule(2, 2, PUSH, 'B', false, pre, 5);
    pda.add_rule(3, 1, POP, '*', false, 'B', 1);

    std::vector<char> init_stack{'A', 'B', 'A'};
    PAutomaton automaton(pda, 1, pda.encode_pre(init_stack));

    Solver::post_star<Trace_Type::Shortest>(automaton);

    std::vector<char> test_stack_reachable{'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(1, pda.encode_pre(test_stack_reachable)), true);
}

BOOST_AUTO_TEST_CASE(WeightedPostStar3)
{
    std::unordered_set<char> labels{'A'};
    TypedPDA<char, int> pda(labels);
    std::vector<char> pre{'A'};

    pda.add_rule(1, 2, PUSH, 'A', false, 'A', 16);
    pda.add_rule(1, 3, PUSH , 'A', false, 'A', 1);
    pda.add_rule(3, 3, PUSH , 'A', false, 'A', 2);
    pda.add_rule(3, 2, POP , 'A', false, 'A', 1);

    std::vector<char> init_stack{'A'};
    PAutomaton automaton(pda, 1, pda.encode_pre(init_stack));

    Solver::post_star<Trace_Type::Shortest>(automaton);

    std::vector<char> test_stack_reachable{'A','A'};
    BOOST_CHECK_EQUAL(automaton.accepts(2, pda.encode_pre(test_stack_reachable)), true);
}

BOOST_AUTO_TEST_CASE(WeightedPostStar4)
{
    std::unordered_set<char> labels{'A'};
    TypedPDA<char, int> pda(labels);

    pda.add_rule(0, 3, PUSH, 'A', false, 'A', 4);
    pda.add_rule(0, 1, PUSH , 'A', false, 'A', 1);
    pda.add_rule(3, 1, PUSH , 'A', false, 'A', 8);
    pda.add_rule(1, 2, POP , 'A', false, 'A', 2);
    pda.add_rule(2, 4, POP , 'A', false, 'A', 16);

    std::vector<char> init_stack{'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::post_star<Trace_Type::Shortest>(automaton);

    std::vector<char> test_stack_reachable{'A'};
    BOOST_CHECK_EQUAL(automaton.accepts(4, pda.encode_pre(test_stack_reachable)), true);
}

BOOST_AUTO_TEST_CASE(WeightedPostStarResult)
{
    std::unordered_set<char> labels{'A'};
    TypedPDA<char, int> pda(labels);

    pda.add_rule(0, 3, PUSH, 'A', false, 'A', 4);
    pda.add_rule(0, 1, PUSH , 'A', false, 'A', 1);
    pda.add_rule(3, 1, PUSH , 'A', false, 'A', 8);
    pda.add_rule(1, 2, POP , 'A', false, 'A', 2);
    pda.add_rule(2, 4, POP , 'A', false, 'A', 16);

    std::vector<char> init_stack{'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::post_star<Trace_Type::Shortest>(automaton);

    std::vector<char> test_stack_reachableA{'A'};
    auto result4A = automaton.accept_path<Trace_Type::Shortest>(4, pda.encode_pre(test_stack_reachableA));
    auto distance4A = result4A.second;

    std::vector<char> test_stack_reachableAA{'A','A'};
    auto result2AA = automaton.accept_path<Trace_Type::Shortest>(2, pda.encode_pre(test_stack_reachableAA));
    auto distance2AA = result2AA.second;

    BOOST_CHECK_EQUAL(distance4A, 30);          //Example Derived on whiteboard
    BOOST_CHECK_EQUAL(distance2AA, 14);         //Example Derived on whiteboard
}

BOOST_AUTO_TEST_CASE(WeightedPostStar4EarlyTermination)
{
    std::unordered_set<char> labels{'A'};
    TypedPDA<char, int> pda(labels);

    pda.add_rule(0, 3, PUSH, 'A', false, 'A', 4);
    pda.add_rule(0, 1, PUSH , 'A', false, 'A', 1);
    pda.add_rule(3, 1, PUSH , 'A', false, 'A', 8);
    pda.add_rule(1, 2, POP , 'A', false, 'A', 2);
    pda.add_rule(2, 4, POP , 'A', false, 'A', 16);

    std::vector<char> init_stack{'A'};
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    std::vector<char> test_stack_reachable{'A'};
    BOOST_CHECK_EQUAL(Solver::post_star_accepts<Trace_Type::Shortest>(automaton, 4, pda.encode_pre(test_stack_reachable)), true);

    auto result4A = automaton.accept_path<Trace_Type::Shortest>(4, pda.encode_pre(test_stack_reachable));
    auto distance4A = result4A.second;
    BOOST_CHECK_EQUAL(distance4A, 30);          //Example Derived on whiteboard
}

TypedPDA<int,int> create_syntactic_network_broad(int network_size = 2) {
    std::unordered_set<int> labels{0,1,2};
    int start_state = 0;
    int states = 4;
    int end_state = 4;

    TypedPDA<int, int> pda(labels);

    for (int j = 0; j < network_size; j++) {
        pda.add_rule(start_state, 1 + start_state, PUSH, 0, false, 0, 0);
        pda.add_rule(start_state, 1 + start_state, PUSH, 1, false, 0, 1);
        pda.add_rule(start_state, 1 + start_state, PUSH, 2, false, 0, 1);
        pda.add_rule(start_state, 2 + start_state, PUSH, 0, false, 2, 0);
        pda.add_rule(start_state, 3 + start_state, POP, 0, false, 1, 1);

        pda.add_rule(1 + start_state, 3 + start_state, PUSH, 1, false, 2, 1);
        pda.add_rule(1 + start_state, end_state, PUSH, 0, false, 0, 1);
        pda.add_rule(1 + start_state, end_state, PUSH, 1, false, 1, 1);

        for (size_t i = 0; i < labels.size(); i++) {
            pda.add_rule(2 + start_state, 2 + start_state, POP, 0, false, i, 5);
        }

        pda.add_rule(2 + start_state, end_state, PUSH, 0, false, 0, 1);

        pda.add_rule(3 + start_state, 2 + start_state, POP, 0, false, 2, 1);
        pda.add_rule(3 + start_state, end_state, PUSH, 2, false, 0, 1);
        pda.add_rule(3 + start_state, end_state, PUSH, 2, false, 1, 1);

        start_state = end_state;
        end_state = end_state + states;
    }
    return pda;
}

TypedPDA<int,int> create_syntactic_network_deep(int network_size = 2){
    std::unordered_set<int> labels{0,1,2};
    TypedPDA<int, int> pda(labels);
    int start_state = 0;
    int new_start_state = 4;
    int end_state = 2;
    int new_end_state = 6;

    for(int j = 0; j < network_size; j++){
        pda.add_rule(start_state, 1+start_state, POP, 2, false, 1, 1);

        pda.add_rule(1+start_state, end_state, SWAP, 2, false, 0, 1);

        pda.add_rule(end_state, 3+start_state, POP, 1, false, 2, 1);

        pda.add_rule(3+start_state, start_state, SWAP, 1, false, 0, 1);

        for(size_t i = 0; i < labels.size(); i++){
            for(size_t k = 0; k < labels.size(); k++) {
                pda.add_rule(new_start_state, start_state, PUSH, i, false, k, 1);
            }
        }
        for(size_t i = 0; i < labels.size(); i++){
            for(size_t k = 0; k < labels.size(); k++) {
                pda.add_rule(end_state, new_end_state, PUSH, i, false, k, 1);
            }
        }
        start_state = new_start_state;
        end_state = new_end_state;
        new_start_state = new_start_state + 4;
        new_end_state = new_end_state + 4;
    }
    return pda;
}

BOOST_AUTO_TEST_CASE(WeightedPostStarSyntacticModel)
{
    TypedPDA<int,int> pda = create_syntactic_network_broad(1);
    std::vector<int> init_stack;
    init_stack.push_back(0);
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));

    Solver::post_star<Trace_Type::Shortest>(automaton);

    std::vector<int> test_stack_reachable;
    test_stack_reachable.push_back(0);
    test_stack_reachable.push_back(0);
    test_stack_reachable.push_back(0);

    BOOST_CHECK_EQUAL(automaton.accepts(4, pda.encode_pre(test_stack_reachable)), true);
}

BOOST_AUTO_TEST_CASE(WeightedPostStarVSPostUnorderedPerformance)
{
    std::unordered_set<int> labels;
    int alphabet_size = 10000;

    //Insert labels alphabet
    for(int i = 0; i < alphabet_size; i++){
        labels.insert(i);
    }

    TypedPDA<int, int> pda(labels);

    for(int i = 0; i < alphabet_size; i++){
        pda.add_rule(0, 1, SWAP, i, false, 0, 1);
        pda.add_rule(1, 2, SWAP, 0, false, i, i);
        pda.add_rule(2, 3, PUSH, i, false, 0, 1);
    }

    std::vector<int> init_stack;
    init_stack.push_back(0);

    std::vector<int> test_stack_reachable;
    test_stack_reachable.push_back(0);

    PAutomaton shortest_automaton(pda, 0, pda.encode_pre(init_stack));
    auto t1 = std::chrono::high_resolution_clock::now();
    Solver::post_star<Trace_Type::Shortest>(shortest_automaton);
    auto t2 = std::chrono::high_resolution_clock::now();
    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));
    auto t3 = std::chrono::high_resolution_clock::now();
    Solver::post_star<Trace_Type::Any>(automaton);
    auto t4 = std::chrono::high_resolution_clock::now();

    auto duration_short_post = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
    auto duration_post = std::chrono::duration_cast<std::chrono::microseconds>( t4 - t3 ).count();

    BOOST_TEST_MESSAGE( "ShortestTrace: " << std::to_string(duration_short_post) << " PostStar: " << std::to_string(duration_post));
}


BOOST_AUTO_TEST_CASE(WeightedShortestPerformance)
{
    TypedPDA<int, int> pda = create_syntactic_network_deep(200);

    std::vector<int> init_stack;
    init_stack.push_back(0);

    std::vector<int> test_stack_reachable;
    test_stack_reachable.push_back(0);
    test_stack_reachable.push_back(0);
    test_stack_reachable.push_back(0);
    test_stack_reachable.push_back(0);

    PAutomaton shortest_automaton(pda, 0, pda.encode_pre(init_stack));
    auto t1 = std::chrono::high_resolution_clock::now();
    Solver::post_star<Trace_Type::Shortest>(shortest_automaton);
    auto t2 = std::chrono::high_resolution_clock::now();

    PAutomaton automaton(pda, 0, pda.encode_pre(init_stack));
    auto t3 = std::chrono::high_resolution_clock::now();
    Solver::post_star<Trace_Type::Any>(automaton);
    auto t4 = std::chrono::high_resolution_clock::now();

    auto duration_short_post = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();
    auto duration_post = std::chrono::duration_cast<std::chrono::microseconds>( t4 - t3 ).count();

    BOOST_TEST_MESSAGE( "ShortestTrace: " << std::to_string(duration_short_post) << " PostStar: " << std::to_string(duration_post));
}

