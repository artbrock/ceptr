#include "../src/ceptr.h"
#include "../src/hashfn.h"
#include "../src/semtrex.h"

/*
Semtrex TGF:

Semtrex ::= Root [ / Child]
Root ::= Literal | Or | Any | Question | Group
Child ::=  Root | Plus | Star | Sequence
Plus ::= "PLUS" / Semtrex
Star ::= "STAR" / Semtrex
Question ::= "QUESTION" / Semtrex
Sequence ::= "SEQUENCE" / Semtrex [, Semtrex]...
Or ::= "OR" / Semtrex1 , Semtrex2
Literal ::= "LITERAL(symbol)" [ / Semtrex]
Any ::= "ANY" [ / Semtrex]
Group ::= "GROUP(group_name)" / Semtrex

<STX>            ::= <union> | <simple-STX>
<union>          ::= <STX> "|" <simple-STX>
<simple-STX>     ::= <concatenation> | <basic-STX>
<concatenation>	 ::=  <simple-STX> "," <basic-STX>
<basic-STX>	 ::= <star> | <plus> | <elementary-STX>
<star>	         ::= <elementary-STX> "*"
<plus>	         ::= <elementary-STX> "+"
<elementary-STX> ::= <group> | <any> | <symbol-tree>
<group>	         ::= "(" <STX> ")"
<any>	         ::= "."

*/

Tnode *_makeTestTree1() {
    Tnode *t = _t_new(0,0,"t",2);
    Tnode *t1 = _t_new(t,1,"t1",3);
    Tnode *t11 = _t_new(t1,11,"t11",4);
    Tnode *t111 = _t_new(t11,111,"t111",5);
    Tnode *t2 = _t_new(t,2,"t2",3);
    Tnode *t21 = _t_new(t2,21,"t21",4);
    Tnode *t22 = _t_new(t2,22,"t22",4);
    Tnode *t3 = _t_new(t,3,"t3",3);
    Tnode *t4 = _t_new(t,4,"t4",3);
    return t;
}

Tnode *_makeTestSemtrex1() {
    //#  /0/(1/11/111),2,3
    Tnode *s = _t_newi(0,SEMTREX_SYMBOL_LITERAL,0);
    Tnode *ss = _t_newi(s,SEMTREX_SEQUENCE,0);
    Tnode *s1 = _t_newi(ss,SEMTREX_SYMBOL_LITERAL,1);
    Tnode *s11 = _t_newi(s1,SEMTREX_SYMBOL_LITERAL,11);
    Tnode *s111 = _t_newi(s11,SEMTREX_SYMBOL_LITERAL,111);
    Tnode *s2 = _t_newi(ss,SEMTREX_SYMBOL_LITERAL,2);
    Tnode *s3 = _t_newi(ss,SEMTREX_SYMBOL_LITERAL,3);
    return s;
}

static int dump_id = 99;


void __s_dump(SState *s) {
    if (s->id == dump_id) {printf("X");return;}
    s->id = dump_id;
    switch (s->type) {
    case StateMatch:
	printf("(M)");
	break;
    case StateGroup:
	if (s->symbol & GroupOpen)
	    printf("{%d",s->symbol&0xFFF);
	else
	    printf("%d}",s->symbol);
	break;
    case StateSymbol:
	printf("(%d:%d)",s->symbol,s->transition);
	break;
    case StateAny:
	printf("(.:%d)",s->transition);
	break;
    case StateSplit:
	printf("S");
	break;
    }
    if (s->out) {printf("->");__s_dump(s->out);}
    if (s->out1) {printf("[->");__s_dump(s->out1);printf("]");}
    //        printf("\n");
}

void _s_dump(SState *s) {
    ++dump_id;
    __s_dump(s);
}


#define spec_state_equal(sa,st,tt,s) \
    spec_is_equal(sa->type,st);\
    spec_is_equal(sa->transition,tt);\
    spec_is_equal(sa->symbol,s);\
    spec_is_ptr_equal(sa->out1,NULL);


void testMakeFA() {
    Tnode *s = _makeTestSemtrex1();

    int states = 0;
    SState *sa = _s_makeFA(s,&states);
    spec_is_equal(states,6);

    spec_state_equal(sa,StateSymbol,TransitionDown,0);

    SState *s1 = sa->out;
    spec_state_equal(s1,StateSymbol,TransitionDown,1);

    SState *s2 = s1->out;
    spec_state_equal(s2,StateSymbol,TransitionDown,11);

    SState *s3 = s2->out;
    spec_state_equal(s3,StateSymbol,-2,111);

    SState *s4 = s3->out;
    spec_state_equal(s4,StateSymbol,TransitionNextChild,2);

    SState *s5 = s4->out;
    spec_state_equal(s5,StateSymbol,TransitionUp,3);

    SState *s6 = s5->out;
    spec_is_equal(s6->type,StateMatch);

    spec_is_ptr_equal(s6->out,NULL);

    _s_freeFA(sa);
    _t_free(s);
}

void testMatchTrees() {
    Tnode *t = _makeTestTree1();
    Tnode *s = _makeTestSemtrex1(t);

    spec_is_true(_t_match(s,t));

    Tnode *s2 = _t_newi(_t_child(s,1),SEMTREX_SYMBOL_LITERAL,99);

    spec_is_true(!_t_match(s,t));

    _t_free(t);
    _t_free(s);
}

void testMatchOr() {
    Tnode *t = _makeTestTree1();
    Tnode *s = _t_newi(0,SEMTREX_OR,0);
    Tnode *s1 = _t_newi(s,SEMTREX_SYMBOL_LITERAL,1);
    Tnode *s11 = _t_newi(s1,SEMTREX_SYMBOL_LITERAL,11);
    Tnode *s2 = _t_newi(s,SEMTREX_SYMBOL_LITERAL,0);

    spec_is_true(_t_match(s,t));

    Tnode *s3 = _t_newi(s2,SEMTREX_SYMBOL_LITERAL,99);

    spec_is_true(!_t_match(s,t));

    _t_free(t);
    _t_free(s);
}

void testMatchAny() {
    Tnode *t = _t_new(0,0,"t",2);
    Tnode *t1 = _t_new(t,1,"t1",3);

    Tnode *s = _t_newi(0,SEMTREX_SYMBOL_ANY,0);
    Tnode *ss = _t_newi(s,SEMTREX_SEQUENCE,0);
    Tnode *s1 = _t_newi(ss,SEMTREX_SYMBOL_LITERAL,1);

    spec_is_true(_t_match(s,t));
    t->contents.symbol = 99;
    spec_is_true(_t_match(s,t));

    _t_free(t);
    _t_free(s);
}

void testMatchStar() {
    //# /0/1*
    Tnode *s = _t_newi(0,SEMTREX_SYMBOL_LITERAL,0);
    Tnode *ss = _t_newi(s,SEMTREX_SEQUENCE,0);
    Tnode *sss = _t_newi(ss,SEMTREX_STAR,0);
    Tnode *s1 = _t_newi(sss,SEMTREX_SYMBOL_LITERAL,1);

    Tnode *t = _t_new(0,0,"t",2);
    spec_is_true(_t_match(s,t));

    Tnode *t1 = _t_new(t,1,"t1",3);
    spec_is_true(_t_match(s,t));

    Tnode *t1x = _t_new(t,1,"t1",3);
    Tnode *t1y = _t_new(t,2,"t2",3);
    spec_is_true(_t_match(s,t));

    //# /0/1*,2
    Tnode *s2 = _t_newi(ss,SEMTREX_SYMBOL_LITERAL,2);
    spec_is_true(_t_match(s,t));
    (*(int *)_t_surface(s2)) = 3;

    spec_is_true(!_t_match(s,t));
    _t_free(t);
    _t_free(s);
}

void testMatchPlus() {
    //# /0/1+
    Tnode *s = _t_newi(0,SEMTREX_SYMBOL_LITERAL,0);
    Tnode *ss = _t_newi(s,SEMTREX_SEQUENCE,0);
    Tnode *sss = _t_newi(ss,SEMTREX_PLUS,0);
    Tnode *s1 = _t_newi(sss,SEMTREX_SYMBOL_LITERAL,1);

    Tnode *t = _t_new(0,0,"t",2);
    spec_is_true(!_t_match(s,t));

    Tnode *t1 = _t_new(t,1,"t1",3);
    spec_is_true(_t_match(s,t));

    Tnode *t1x = _t_new(t,1,"t1",3);
    Tnode *t1y = _t_new(t,2,"t2",3);
    spec_is_true(_t_match(s,t));

    _t_free(t);
    _t_free(s);
}

void testMatchQ() {
    //# /0/1?
    Tnode *s = _t_newi(0,SEMTREX_SYMBOL_LITERAL,0);
    Tnode *ss = _t_newi(s,SEMTREX_SEQUENCE,0);
    Tnode *sss = _t_newi(ss,SEMTREX_QUESTION,0);
    Tnode *s1 = _t_newi(sss,SEMTREX_SYMBOL_LITERAL,1);

    Tnode *t = _t_new(0,0,"t",2);
    spec_is_true(_t_match(s,t));

    Tnode *t1 = _t_new(t,1,"t1",3);
    spec_is_true(_t_match(s,t));

    Tnode *t1x = _t_new(t,1,"t1",3);
    Tnode *t1y = _t_new(t,2,"t2",3);

    //# /0/1?,2
    Tnode *s2 = _t_newi(ss,SEMTREX_SYMBOL_LITERAL,2);
    spec_is_true(!_t_match(s,t));

    _t_free(t);
    _t_free(s);
}

void testMatchGroup() {

    //# /0/(.*,(.)),4
    Tnode *s = _t_newi(0,SEMTREX_SYMBOL_LITERAL,0);
    Tnode *ss = _t_newi(s,SEMTREX_SEQUENCE,0);
    Tnode *sg = _t_newi(ss,SEMTREX_GROUP,0);
    Tnode *ss2 = _t_newi(sg,SEMTREX_SEQUENCE,0);
    Tnode *st = _t_newi(ss2,SEMTREX_STAR,0);
    _t_newi(st,SEMTREX_SYMBOL_ANY,0);
    Tnode *sg2 = _t_newi(ss2,SEMTREX_GROUP,0);
    _t_newi(sg2,SEMTREX_SYMBOL_ANY,0);
    Tnode *s3 = _t_newi(ss,SEMTREX_SYMBOL_LITERAL,4);

    Tnode *t = _makeTestTree1();

    Tnode *r;
    spec_is_true(_t_matchr(s,t,&r));
    spec_is_equal(_t_symbol(r),SEMTREX_MATCH_RESULTS);
    spec_is_equal(_t_children(r),2);

    Tnode *p1 = _t_child(r,1);
    spec_is_equal(_t_symbol(p1),SEMTREX_MATCH);
    spec_is_equal(_t_children(p1),2);

    int rp1[] = {1,TREE_PATH_TERMINATOR};
    Tnode *p1c = _t_child(p1,2);

    //    printf("%s\n",_td(r));
    spec_is_equal(_t_symbol(p1c),SEMTREX_MATCH_SIBLINGS_COUNT);
    spec_is_equal(*(int *)_t_surface(p1c),3);
    spec_is_path_equal(_t_surface(_t_child(p1,1)),rp1);


    Tnode *p2 = _t_child(r,2);
    spec_is_equal(_t_symbol(p2),SEMTREX_MATCH);

    int rp2[] = {3,TREE_PATH_TERMINATOR};
    Tnode *p2c = _t_child(p2,2);
    spec_is_equal(_t_symbol(p2c),SEMTREX_MATCH_SIBLINGS_COUNT);
    spec_is_equal(*(int *)_t_surface(p2c),1);
    spec_is_path_equal(_t_surface(_t_child(p2,1)),rp2);

    _t_free(r);
    _t_free(t);
    _t_free(s);
}

void testMatchLiteralValue() {
    Tnode *t = _makeTestTree1();
    Svalue sv;
    sv.symbol = 0;
    sv.length = 2;
    ((char *)&sv.value)[0] = 't';
    ((char *)&sv.value)[1] = 0;
    Tnode *s = _t_new(0,SEMTREX_VALUE_LITERAL,&sv,sizeof(Svalue));
    spec_is_true(_t_match(s,t));
    _t_free(s);

    // don't match if value is wrong
    ((char *)&sv.value)[0] = 'x';
    s = _t_new(0,SEMTREX_VALUE_LITERAL,&sv,sizeof(Svalue));
    spec_is_true(!_t_match(s,t));
    _t_free(s);

    // don't match on wrong symbol
    ((char *)&sv.value)[0] = 't';
    sv.symbol = 1;
    s = _t_new(0,SEMTREX_VALUE_LITERAL,&sv,sizeof(Svalue));
    spec_is_true(!_t_match(s,t));
    _t_free(s);

    // don't match if value length is wrong
    ((char *)&sv.value)[0] = 't';
    sv.length = 1;
    s = _t_new(0,SEMTREX_VALUE_LITERAL,&sv,sizeof(Svalue));
    spec_is_true(!_t_match(s,t));
    _t_free(s);


    _t_free(t);


}

void testSemtrex() {
    testMakeFA();
    testMatchTrees();
    testMatchOr();
    testMatchAny();
    testMatchStar();
    testMatchPlus();
    testMatchQ();
    testMatchGroup();
    testMatchLiteralValue();
}
