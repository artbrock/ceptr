#ifndef _CEPTR_SYMBOL_H
#define _CEPTR_SYMBOL_H

#define TEST_SYMBOL 99
#define TEST_SYMBOL2 100

// System defined symbols.
enum
{
    //-----  Basic symbols for underlying data types
    NULL_SYMBOL = -999,
    TREE_PATH,

    //-----  Symbols for the different semantic parts of semtrexes
    SEMTREX_SYMBOL_LITERAL,            // This system symbol matches on the semantic type.         Ex: /TEST_SYMBOL
    SEMTREX_SEQUENCE,                  // Match on a sequence of child nodes which are any valid semtrex's.  Ex: comma separated nodes
    SEMTREX_OR,                        // Logical OR between two Semtrex expressions.      Ex: |
    SEMTREX_SYMBOL_ANY,                // Match any symbol or value of the node.           Ex: .
    SEMTREX_ZERO_OR_MORE,              // Requires one child Semtrex and matches on zero or more of that Semtrex.  Ex: /0/TestSemtrex*
    SEMTREX_ONE_OR_MORE,               // Requires one child Semtrex and matches on one or more of that Semtrex.   Ex: /0/TestSemtrex+
    SEMTREX_ZERO_OR_ONE,               // Requires one child Semtrex and matches on zero or one of that Semtrex.   Ex: /0/TestSemtrex?
    SEMTREX_VALUE_LITERAL,	       // Matches on the semantic type and the data value.
    SEMTREX_GROUP,                     // Grouping                                 Ex: (...)operator
    SEMTREX_MATCH,                     // Returns result and sibling count.        Ex: {name:expr} (verify this is what it's supposed to do)
    SEMTREX_MATCH_RESULTS,             // In the FSA, keeps track of which part matches so it can be referenced
    SEMTREX_MATCH_SIBLINGS_COUNT,      // In the FSA, it's the length of the match

    //-----  Symbols for receptors
    RECEPTOR,
    FLUX,                              // tree to hold all incoming and in process signals on the various aspects
    ASPECT,
    SIGNALS,                           // list of signals on an aspect in the flux
    SIGNAL,                            // a signal on the flux.  It's first child is the contents of the signal
    LISTENERS,                         // list of carrier/expectation/action tress that "listen" to changes on the flux
    LISTENER,                          // surface of the listener is the carrier symbol, and it has two children, expectation semtrex and action code tree
    EXPECTATION,                       // expectation is a semtrex (thus has one child which is the first part of the semtrex)
    ACTION,                            // code tree, which specifies the action to perform when an expectation's semtrex matches
};

typedef int Symbol;

#endif


//	Semantic Type:
//		Structures, like "float", are abstract labels.
//		A semantic type is a label with a particular semantic application or implementation of that labelled structure.
//		"int" is a labelled structure, "age" is a semantic implementation.
//
//	Ex:
//		/TEST_SYMBOL="t"		: the root's semantic type is "semtrex_symbol_literal" and it's literal value = 't'

//  SEMTREX_GROUP
//      Paren-group: A group to which you apply some function.
//      SMTP: mail from         where "from is optional"
//      Ex: /ascii-line/char='M'...etc..., (F R O M)?
//
//  Curly brace is a named (or referenced) group
//      Ex: /ascii-line/char='M'...etc..., (F R O M)?{email-address:.*}
