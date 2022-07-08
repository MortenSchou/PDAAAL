import pdaaal

pda = pdaaal.PDA()

automaton = pdaaal.PAutomaton(pda, [], False)

print(automaton.to_dot())
