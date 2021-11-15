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
 * File:   IsabellePrettyPrinter.h
 * Author: Morten K. Schou <morten@h-schou.dk>
 *
 * Created on 05-11-2021.
 */

#ifndef PDAAAL_ISABELLEPRETTYPRINTER_H
#define PDAAAL_ISABELLEPRETTYPRINTER_H

#include <pdaaal/PAutomaton.h>

class IsabellePrettyPrinter {
public:
    explicit IsabellePrettyPrinter(std::ostream& out) : _out(out) { };

    void print_query(const pdaaal::TypedPDA<char>& pda, const pdaaal::PAutomaton<>& initial_automaton, const pdaaal::PAutomaton<>& final_automaton);
    void print_begin();
    void print_proofs();
    void print_lemma(bool answer);
    void print_end();

//protected:
    std::ostream& print_automaton_state(size_t state, const pdaaal::PAutomaton<>& automaton, bool print_type = true);
    std::ostream& print_rules(const std::string& name, const pdaaal::TypedPDA<char>& pda);
    std::ostream& print_automaton(const std::string& name_prefix, const pdaaal::PAutomaton<>& automaton, const pdaaal::TypedPDA<char>& pda);

private:
    std::ostream& _out;
};


#endif //PDAAAL_ISABELLEPRETTYPRINTER_H
