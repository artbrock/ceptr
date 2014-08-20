#ifndef _CEPTR_RECEPTOR_H
#define _CEPTR_RECEPTOR_H

#include "tree.h"
#include "label.h"

struct Receptor {
    Tnode *root;
    Tnode *structures;
    Tnode *symbols;
    Tnode *flux;
    LabelTable table;
};
typedef struct Receptor Receptor;

// for now aspects are just identified as the child index in the flux receptor
enum {DEFAULT_ASPECT=1};
typedef int Aspect;

/******************  create and destroy receptors */
Receptor * _r_new();
void _r_add_listener(Receptor *r,Aspect aspect,Symbol carrier,Tnode *semtrex,Tnode *action);
void _r_free(Receptor *r);

/*****************  receptor symbols and structures */
Symbol _r_def_symbol(Receptor *r,Structure s,char *label);
Symbol _r_get_symbol_by_label(Receptor *r,char *label);
Structure _r_def_structure(Receptor *r,char *label,int num_params,...);
Structure _r_get_structure_by_label(Receptor *r,char *label);
Structure __r_get_symbol_structure(Receptor *r,Symbol s);
size_t __r_get_symbol_size(Receptor *r,Symbol s,void *surface);

/*****************  receptor instances and xaddrs */
struct Instance {
    void *surface;
};
typedef struct Instance Instance;

struct Xaddr {
    Symbol symbol;
    int addr;
};
typedef struct Xaddr Xaddr;
Xaddr _r_new_instance(Receptor *r,Symbol s,void * surface);
Instance _r_get_instance(Receptor *r,Xaddr x);

/******************  receptor signaling */
void _r_reduce(Tnode *run_tree);
Tnode *_r_make_run_tree(Tnode *code,int num_params,...);
//*TODO: for now the signal is appended directly to the flux.  Later it should probably be copied
Tnode * _r_send(Receptor *r,Receptor *from,Aspect aspect, Tnode *signal);

/******************  internal utilities */
Tnode *__r_get_aspect(Receptor *r,Aspect aspect);
Tnode *__r_get_listeners(Receptor *r,Aspect aspect);
Tnode *__r_get_signals(Receptor *r,Aspect aspect);


/*****************  Tree debugging utilities */
char *_td(Receptor *r,Tnode *t);

#define spec_is_symbol_equal(r,got, expected) spec_total++; if (expected==got){putchar('.');} else {putchar('F');sprintf(failures[spec_failures++],"%s:%d expected %s to be %s but was %s",__FUNCTION__,__LINE__,#got,_s_get_symbol_name(r,expected),_s_get_symbol_name(r,got));}

#define spec_is_structure_equal(r,got, expected) spec_total++; if (expected==got){putchar('.');} else {putchar('F');sprintf(failures[spec_failures++],"%s:%d expected %s to be %s but was %s",__FUNCTION__,__LINE__,#got,_s_get_symbol_name(r,expected),_s_get_structure_name(r,got));}

char *_s_get_symbol_name(Receptor *r,Symbol s);
char *_s_get_structure_name(Receptor *r,Structure s);


#endif