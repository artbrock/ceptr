/**
 * @ingroup receptor
 *
 * @{
 * @file receptor.c
 * @brief receptor implementation
 *
 * @copyright Copyright (C) 2013-2015, The MetaCurrency Project (Eric Harris-Braun, Arthur Brock, et. al).  This file is part of the Ceptr platform and is released under the terms of the license contained in the file LICENSE (GPLv3).
 */

#include "receptor.h"
#include "stream.h"
#include "semtrex.h"
#include "process.h"
#include "accumulator.h"
#include "debug.h"
#include "mtree.h"
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

Xaddr G_null_xaddr  = {0,0};
/*****************  create and destroy receptors */

/* set up the c structures for a receptor from a semantic tree */
Receptor * __r_init(T *t,SemTable *sem,ReceptorAddress addr) {
    Receptor *r = malloc(sizeof(Receptor));
    r->root = t;
    r->sem = sem;
    r->addr = addr;
    r->table = NULL;
    r->instances = NULL;
    r->q = _p_newq(r);
    r->state = Alive;  //@todo, check if this is true on unserialize

    T *defs = _t_child(t,ReceptorDefsIdx);
    r->defs.structures = _t_child(defs,1);
    r->defs.symbols = _t_child(defs,2);
    r->defs.processes = _t_child(defs,3);
    r->defs.protocols = _t_child(defs,4);
    r->defs.scapes = _t_child(defs,5);
    r->flux = _t_child(t,ReceptorFluxIdx);
    r->pending_signals = _t_child(t,ReceptorPendingSignalsIdx);
    r->pending_responses = _t_child(t,ReceptorPendingResponsesIdx);
    return r;
}

Receptor *__r_new(Symbol s,T *defs,SemTable *sem,ReceptorAddress addr) {
    T *t = _t_new_root(s);
    _t_add(t,defs);
    T *f = _t_newr(t,FLUX);
    T *a = _t_newr(f,DEFAULT_ASPECT);
    _t_newr(a,EXPECTATIONS);
    _t_newr(a,SIGNALS);
    _t_newr(t,RECEPTOR_STATE);
    _t_newr(t,PENDING_SIGNALS);
    _t_newr(t,PENDING_RESPONSES);
    Receptor *r = __r_init(t,sem,addr);

    return r;
}

//helper to make empty definitions tree
T *__r_make_definitions() {
    T *defs = _t_new_root(DEFINITIONS);
    _t_newr(defs,STRUCTURES);
    _t_newr(defs,SYMBOLS);
    _t_newr(defs,PROCESSES);
    _t_newr(defs,PROTOCOLS);
    _t_newr(defs,SCAPES);
    _t_newr(defs,ASPECTS);
    return defs;
}

/**
 * @brief Creates a new receptor
 *
 * allocates all the memory needed in the heap
 *
 * @param[in] s symbol for this receptor
 * @returns pointer to a newly allocated Receptor
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/receptor_spec.h testReceptorCreate
 */
Receptor *_r_new(SemTable *st,Symbol s) {
    T *defs = __r_make_definitions();
    return __r_new(s,defs,st,_sem_new_context(st,defs));
}

// set the labels in the label table for the given def
void __r_set_labels(Receptor *r,T *defs,int sem_type) {
    DO_KIDS(
            defs,
            T *def = _t_child(defs,i);
            T *sl = _t_child(def,1);
            __set_label_for_def(r,_t_surface(sl),def,sem_type);
            );
}

/**
 * @brief Creates a new receptor from a receptor package
 *
 * allocates all the memory needed in the heap, cloning the various parts from the package
 * and binding the new receptor to the provided bindings
 *
 * @param[in] s symbol for this receptor
 * @returns pointer to a newly allocated Receptor
 * @todo implement bindings
 */
Receptor *_r_new_receptor_from_package(SemTable *st,Symbol s,T *p,T *bindings) {
    T *defs = _t_clone(_t_child(p,3));
    //    T *aspects = _t_clone(_t_child(p,4));  @todo this should be inside the defs allready
    raise_error("fix receptor address");
    Receptor * r = __r_new(s,defs,st,0);

    //@todo fix this because it relies on SemanticTypes value matching the index order in the definitions.
    DO_KIDS(defs,__r_set_labels(r,_t_child(defs,i),i));

    return r;
}

// helper to build and expectation tree
T *__r_build_expectation(Symbol carrier,T *pattern,T *action,T *with,T *until) {
    T *e = _t_newr(0,EXPECTATION);
    _t_news(e,CARRIER,carrier);
    _t_add(e,pattern);
    _t_add(e,action);
    if (!with) with = _t_new_root(PARAMS);
    _t_add(e,with);
    if (!until) {
        until = _t_new_root(END_CONDITIONS);
        _t_newr(until,UNLIMITED);
    }
    _t_add(e,until);
    return e;
}

/**
 * @brief Adds an expectation to a receptor's aspect.
 *
 * @param[in] r receptor to add to
 * @param[in] aspect aspect on which to install the expectation
 * @param[in] carrier pre-screeing of signals to match against
 * @param[in] pattern semtrex to match against signals
 * @param[in] action process to run if match
 * @param[in] with parameters to pass into that process
 * @param[in] until end conditions for cleaning up this expectation
 *
 */
void _r_add_expectation(Receptor *r,Aspect aspect,Symbol carrier,T *pattern,T *action,T *with,T *until) {
    T *e = __r_build_expectation(carrier,pattern,action,with,until);
    __r_add_expectation(r,aspect,e);
}

void __r_add_expectation(Receptor *r,Aspect aspect,T *e) {
    T *a = __r_get_expectations(r,aspect);
    _t_add(a,e);
}

void _r_remove_expectation(Receptor *r,T *expectation) {
    T *a = _t_parent(expectation);
    _t_detach_by_ptr(a,expectation);
    _t_free(expectation);
    // @todo, if there are any processes blocked on this expectation they
    // should actually get cleaned up somehow.  This would mean searching
    // through for them, or something...
}

/**
 * Destroys a receptor freeing all memory it uses.
 */
void _r_free(Receptor *r) {

    _sem_free_context(r->sem,r->addr);
    _t_free(r->root);
    lableTableFree(&r->table);
    _a_free_instances(&r->instances);
    if (r->q) _p_freeq(r->q);
    free(r);
}

/*****************  receptor symbols, structures and processes */

/**
 * we use this for labeling symbols, structures and processes because labels store the full path to the labeled item and we want the labels to be unique across all three
 */
SemanticID __set_label_for_def(Receptor *r,char *label,T *def,int type) {
    int *path = _t_get_path(def);
    labelSet(&r->table,label,path);
    int i = path[_t_path_depth(path)-1];
    free(path);
    SemanticID s = {r->addr,type,i};
    return s;
}

/**
 * Get the child index for a given label.
 *
 * This works for retrieving symbols, structures & processes because the symbol and structure values is just the child index.
 */
SemanticID  __get_label_idx(Receptor *r,char *label) {
    SemanticID s = {r->addr,0,0};
    int *path = labelGet(&r->table,label);
    if (path) {
        s.id = path[_t_path_depth(path)-1];
        s.semtype = path[1]; // definitions index == semantic type!!
    }
    return s;
}

/**
 * define a new symbol
 *
 * @param[in] r receptor to provide a structural context for symbol declarations
 * @param[in] s the structure type for this symbol
 * @param[in] label a c-string label for this symbol
 * @returns the new Symbol
 *
 */
Symbol _r_declare_symbol(Receptor *r,Structure s,char *label){
    __d_validate_structure(r->sem,s,label);
    T *def = __d_declare_symbol(r->defs.symbols,s,label);
    return __set_label_for_def(r,label,def,SEM_TYPE_SYMBOL);
}

/**
 * define a new structure (simple version)
 *
 * this call is handy for building simple STRUCTURE_SEQUENCE style structures
 *
 * @param[in] r receptor to provide a semantic context for new structure definitions
 * @param[in] s the structure type for this symbol
 * @param[in] label a c-string label for this symbol
 * @param[in] num_params number of symbols in the structure
 * @param[in] ... variable list of Symbol type symbols
 * @returns the new Structure
 *
 */
Structure _r_define_structure(Receptor *r,char *label,int num_params,...) {
    va_list params;
    va_start(params,num_params);
    T *def = _dv_define_structure(r->sem,label,r->addr,num_params,params);
    va_end(params);

    return __set_label_for_def(r,label,def,SEM_TYPE_STRUCTURE);
}

/**
 * define a new structure
 *
 * version of _r_define_structure for complex structure defs where caller provides
 * the STRUCTURE_DEF
 *
 * @param[in] r receptor to provide a semantic context for new structure definitions
 * @param[in] s the structure type for this symbol
 * @param[in] label a c-string label for this symbol
 * @param[in] structure_def tree of STRUCTURE_DEF structure
 * @returns the new Structure
 *
 */
Structure __r_define_structure(Receptor *r,char *label,T *structure_def) {
    //@todo validate the structure_def somehow?
    T *def = __d_define_structure(r->defs.structures,label,structure_def);
    return __set_label_for_def(r,label,def,SEM_TYPE_STRUCTURE);
}

/**
 * add a new process coding to a receptor
 *
 * @param[in] r the receptor
 * @param[in] code the code tree for this process
 * @param[in] name the name of the process
 * @param[in] intention a description of what the process intends to do/transform
 * @param[in] in the input signature for the process
 * @param[in] out the output signature for the process
 * @returns the new Process
 *
 */
Process _r_code_process(Receptor *r,T *code,char *name,char *intention,T *signature) {
    T *def = __d_code_process(r->defs.processes,code,name,intention,signature);
    return __set_label_for_def(r,name,def,SEM_TYPE_PROCESS);
}

Protocol _r_define_protocol(Receptor *r,T *p) {
    T *def = __d_define_protocol(r->defs.protocols,p);
    char *label = (char *)_t_surface(_t_child(p,DefLabelIdx));
    return __set_label_for_def(r,label,def,SEM_TYPE_PROTOCOL);
}

/**
 * find a symbol by its label
 */
Symbol _r_get_sem_by_label(Receptor *r,char *label) {
    return __get_label_idx(r,label);
}

/**
 * @brief find a symbol's structure
 * @returns structure id
 */
Structure __r_get_symbol_structure(Receptor *r,Symbol s){
    return _sem_get_symbol_structure(r->sem,s);
}

/**
 * get the size of a structure's surface
 * @returns size
 */
size_t __r_get_structure_size(Receptor *r,Structure s,void *surface) {
    return _d_get_structure_size(r->sem,s,surface);
}
/**
 * get the size of a symbol's surface
 * @returns size
 */
size_t __r_get_symbol_size(Receptor *r,Symbol s,void *surface) {
    return _d_get_symbol_size(r->sem,s,surface);
}

/**
 * Walks the definition of a symbol to build a semtrex that would match that definiton
 *
 * @param[in] r the receptor context in which things are defined
 * @param[in] s the symbol to build a semtrex for
 * @returns the completed semtrex
 */
T * _r_build_def_semtrex(Receptor *r,Symbol s) {
    return _d_build_def_semtrex(r->sem,s,0);
}

/**
 * Determine whether a tree matches a symbol definition, both structural and semantic
 *
 * @param[in] r the receptor context in which things are defined
 * @param[in] s the symbol we expect this tree to be
 * @param[in] t the tree to match
 * @returns true or false depending on the match
 *
 * @todo currently this just matches on a semtrex.  It should also look at the surface
 sizes to see if they meet the criteria of the structure definitions.
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/receptor_spec.h testReceptorDefMatch
 */
int _r_def_match(Receptor *r,Symbol s,T *t) {
    T *stx = _r_build_def_semtrex(r,s);
    int result = _t_match(stx,t);
    _t_free(stx);
    return result;
}

/*****************  receptor instances and xaddrs */

/**
 * Create a new instance of a tree
 *
 * @param[in] r the receptor context in which things are defined
 * @param[in] t the tree to instantiate
 * @returns xaddr of the instance
 *
 * @todo currently stores instances in a hash of hashes, this will later
 * be handled by interacting with the data-engine.
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/receptor_spec.h testReceptorInstanceNew
 */
Xaddr _r_new_instance(Receptor *r,T *t) {
    return _a_new_instance(&r->instances,t);
}

/**
 * retrieve the instance for a given xaddr
 *
 * @param[in] r the receptor context in which things are defined
 * @param[in] x the xaddr of the instance
 * @returns the instance tree
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/receptor_spec.h testReceptorInstanceNew
 */
T * _r_get_instance(Receptor *r,Xaddr x) {
    return _a_get_instance(&r->instances,x);
}

/**
 * set the instance for a given xaddr
 *
 * @param[in] r the receptor context in which things are defined
 * @param[in] x the xaddr of the instance
 * @param[in] t the new tree to set the instance value to
 * @returns the instance tree
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/receptor_spec.h testReceptorInstanceNew
 */
T * _r_set_instance(Receptor *r,Xaddr x,T *t) {
    return _a_set_instance(&r->instances,x,t);
}

/**
 * get the hash of a tree by Xaddr
 */
TreeHash _r_hash(Receptor *r,Xaddr t) {
    return _t_hash(r->sem,_r_get_instance(r,t));
}

/******************  receptor serialization */

/**
 * Serialize a receptor
 *
 * Allocates a buffer for and serializes a receptor into the buffer
 *
 * @param[in] r Receptor to serialize
 * @param[inout] surfaceP pointer to a void * to hold the resulting serialized data
 * @param[inout] lengthP pointer to a size_t to hold the resulting serialized data length
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/receptor_spec.h testReceptorSerialize
 */
void _r_serialize(Receptor *r,void **surfaceP,size_t *lengthP) {
    /* size_t buf_size = 10000; */
    /* *surfaceP  = malloc(buf_size); */
    /* *lengthP = __t_serialize(&r->defs,r->root,surfaceP,sizeof(size_t),buf_size,0); */
    /* *(size_t *)(*surfaceP) = *lengthP; */

    H h = _m_new_from_t(r->root);
    S *s = _m_serialize(h.m);

    S *is = __a_serialize_instances(&r->instances);
    s = (S *)realloc(s,s->total_size+is->total_size);
    memcpy(((void *)s)+s->total_size,is,is->total_size);

    *lengthP = s->total_size+is->total_size;
    *surfaceP = (void *)s;
    free(is);
    _m_free(h);
}

/**
 * Unserialize a receptor
 *
 * Given a serialized receptor, return an instantiated receptor tree with label table
 *
 * @param[in] surface serialized receptor data
 * @returns Receptor
 */
Receptor * _r_unserialize(void *surface) {

    S *s = (S *)surface;
    H h = _m_unserialize(s);

    T *t = _t_new_from_m(h);
    _m_free(h);

    raise_error("fix semtable and receptor address");
    Receptor *r = __r_init(t,0,0);

    /* size_t length = *(size_t *)surface; */
    /* Receptor *r = _r_new(*(Symbol *)(surface+sizeof(size_t))); */
    /* surface += sizeof(size_t); */
    /* T *t =  _t_unserialize(&r->defs,&surface,&length,0); */
    /* _t_free(r->root); */
    /* r->root = t; */
    T *defs = _t_child(t,1);
    DO_KIDS(defs,__r_set_labels(r,_t_child(defs,i),i));

    // move to the instances
    s = (S *) (surface + s->total_size);
    __a_unserialize_instances(&r->instances,(S *)s);
    return r;
}

/******************  receptor signaling */

// build a receptor address.  This is scaffolding for later receptor
// addressing that will include both ceptrnet addresses and receptor paths
// as a possible options for addressing the receptor.
T *__r_make_addr(T *parent,Symbol type,ReceptorAddress addr) {
    T *a = _t_newr(parent,type);
    _t_newi(a,CONTEXT_NUM,addr);
    return a;
}

ReceptorAddress __r_get_addr(T *addr) {
    // for now they are all instance nums so we can just get the surface
    // of the first child.
    T *t = _t_child(addr,1);
    return *(ReceptorAddress *)_t_surface(t);
}

/**
 * build a signal
 *
 * @param[in] from source Receptor Xaddr
 * @param[in] to destination Receptor Xaddr
 * @param[in] aspect Aspect over which the message will be sent
 * @param[in] signal_contents the message to be sent, which will be wrapped in a SIGNAL
 * @todo signal should have timestamps and other meta info
 */
T* __r_make_signal(ReceptorAddress from,ReceptorAddress to,Aspect aspect,Symbol carrier,T *signal_contents,UUIDt *in_response_to,T* until) {
    T *s = _t_new_root(SIGNAL);
    T *e = _t_newr(s,ENVELOPE);
    // @todo convert to paths at some point?
    __r_make_addr(e,FROM_ADDRESS,from);
    __r_make_addr(e,TO_ADDRESS,to);
    _t_news(e,ASPECT_IDENT,aspect);
    _t_news(e,CARRIER,carrier);
    UUIDt t = __uuid_gen();
    _t_new(e,SIGNAL_UUID,&t,sizeof(UUIDt));
    T *b = _t_newt(s,BODY,signal_contents);

    if (in_response_to && until) raise_error("attempt to make signal with both response_uuid and until");
    if (in_response_to)
        _t_new(e,IN_RESPONSE_TO_UUID,in_response_to,sizeof(UUIDt));
    else if (until)
        _t_add(e,until);
    return s;
}

// create WAKEUP_REFERENCE symbol used for unblocking a process
T* __r_build_wakeup_info(T *code_point,int process_id) {
    T *wakeup = _t_new_root(WAKEUP_REFERENCE);
    _t_newi(wakeup,PROCESS_IDENT,process_id);
    int *path = _t_get_path(code_point);
    _t_new(wakeup,CODE_PATH,path,sizeof(int)*(_t_path_depth(path)+1));
    free(path);
    return wakeup;
}

// low level send, must be called with pending_signals resource locked!!
T* __r_send(Receptor *r,T *signal) {
    _t_add(r->pending_signals,signal);

    //@todo for now we return the UUID of the signal as the result.  Perhaps later we return an error condition if delivery to address is known to be impossible, or something like that.
    T *envelope = _t_child(signal,SignalEnvelopeIdx);
    return _t_rclone(_t_child(envelope,EnvelopeUUIDIdx));
}

/**
 * send a simple signal (say)
 *
 * @param[in] carrier the carrier on which to expect a response
 * @param[in] signal Signal tree
 * @returns a clone of the UUID of the message sent.
 */
T* _r_send(Receptor *r,T *signal) {
    debug(D_SIGNALS,"sending %s\n",_t2s(r->sem,signal));
    //@todo lock resources
    T *result =__r_send(r,signal);
    //@todo unlock resources
    return result;
}

/**
 * send a request signal
 *
 * @param[in] r sending receptor
 * @param[in] signal Signal tree
 * @param[in] response_carrier the carrier on which to expect a response
 * @param[in] code_point the point in the code to re-awaken when a response comes back
 * @param[in] process_id the id of the process in which that code point exists
 * @returns a clone of the UUID of the message sent.
 * @todo signal should have timestamps and other meta info
 */
T* _r_request(Receptor *r,T *signal,Symbol response_carrier,T *code_point,int process_id) {

    //@todo lock resources
    T *result = __r_send(r,signal);
    T *pr = _t_newr(r->pending_responses,PENDING_RESPONSE);
    _t_add(pr,_t_clone(result));
    _t_news(pr,CARRIER,response_carrier);
    _t_add(pr,__r_build_wakeup_info(code_point,process_id));
    int p[] = {SignalEnvelopeIdx,EnvelopeExtraIdx,TREE_PATH_TERMINATOR};
    T *ec = _t_get(signal,p);
    if (!ec || !semeq(_t_symbol(ec),END_CONDITIONS)) raise_error("request missing END_CONDITIONS");
    _t_add(pr,_t_clone(ec));

    debug(D_SIGNALS,"sending request and adding pending response: %s\n",_td(r,pr));
    //@todo unlock resources

    return result;
}

// check if the end condition has been met
void evaluateEndCondition(T *ec,bool *cleanup,bool *allow) {
    *cleanup = false;
    *allow = false;
    int k = _t_children(ec);
    while (k) {
        T *c = _t_child(ec,k);
        Symbol sym = _t_symbol(c);
        if (semeq(sym,COUNT)) {
            //@todo mutex!!
            int *cP = (int *)_t_surface(c);
            if (*cP <= 1) *cleanup = true;
            if (*cP >= 1) *allow = true;
            (*cP)--;
            debug(D_SIGNALS,"decreasing count to: %d\n",*cP);
            break;  // this is final, even if there's a timeout
        }
        else if (semeq(sym,TIMEOUT_AT)) {
            T *td = _t_child(c,1);
            T *nw = _t_child(c,2);
            int year = *(int *)_t_surface(_t_child(td,1))-1900;
            int mon = *(int *)_t_surface(_t_child(td,2))-1;
            int mday = *(int *)_t_surface(_t_child(td,3));
            int hour = *(int *)_t_surface(_t_child(nw,1));
            int min = *(int *)_t_surface(_t_child(nw,2));
            int sec = *(int *)_t_surface(_t_child(nw,3));

            //debug(D_SIGNALS,"T: y:%d,m:%d,d:%d,h:%d,m:%d,s:%d\n")
            time_t now_t;
            time(&now_t);
            struct tm now;
            gmtime_r(&now_t, &now);
            if ((year > now.tm_year) ||
                (mon > now.tm_mon) ||
                (mday > now.tm_mday) ||
                (hour > now.tm_hour) ||
                (min > now.tm_min) ||
                (sec > now.tm_sec)) {
                *allow = true;
            }

            *cleanup = !*allow;
        }
        else if (semeq(sym,UNLIMITED)) {
            *allow = true;
        }
        else {
            raise_error("unknown end condition %s",t2s(c));
        }

        k--;
    }
    debug(D_SIGNALS,"after end condition %s cleanup=%s allow=%s\n",t2s(ec),*cleanup?"true":"false",*allow?"true":"false");
}

/**
 * low level function for testing expectation patterns on signals and either adding a new run tree
 * onto the current Q or reawakening the process that's been blocked waiting for the expectation
 * to match
 */
void __r_test_expectation(Receptor *r,T *expectation,T *signal) {
    Q *q = r->q;
    T *signal_contents = (T *)_t_surface(_t_child(signal,SignalBodyIdx));

    //test carriers first because they must match
    T *e_carrier = _t_child(expectation,ExpectationCarrierIdx);
    T *s_carrier = _t_getv(signal,SignalEnvelopeIdx,EnvelopeCarrierIdx,TREE_PATH_TERMINATOR);

    debug(D_SIGNALS,"checking signal carrier %s\n",_td(q->r,s_carrier));
    debug(D_SIGNALS,"against expectation carrier %s\n",_td(q->r,e_carrier));

    Symbol esym = *(Symbol *)_t_surface(e_carrier);
    if (!semeq(esym,*(Symbol *)_t_surface(s_carrier)) && !semeq(esym,NULL_SYMBOL)) return;

    T *pattern,*m;
    pattern = _t_child(expectation,ExpectationPatternIdx);
    // if we get a match, create a run tree from the action, using the match and signal as the parameters
    T *stx = _t_news(0,SEMTREX_GROUP,NULL_SYMBOL);
    _t_add(stx,_t_clone(_t_child(pattern,1)));
    debug(D_SIGNALS,"matching %s\n",_td(q->r,signal_contents));
    debug(D_SIGNALS,"against %s\n",_td(q->r,stx));

    bool allow;
    bool cleanup;
    evaluateEndCondition(_t_child(expectation,ExpectationEndCondsIdx),&cleanup,&allow);

    if (allow && _t_matchr(stx,signal_contents,&m)) {
        debug(D_SIGNALS,"got a match on %s\n",_td(q->r,stx));

        T *rt=0;
        T *action = _t_child(expectation,ExpectationActionIdx);
        if (!action) {
            raise_error("null action in expectation!");
        }

        if (semeq(_t_symbol(action),WAKEUP_REFERENCE)) {
            /* // for now we add the params to the contexts run tree */
            /* /// @todo later this should be integrated into some kind of scoping handling */
            T *params = _t_rclone(_t_child(expectation,ExpectationParamsIdx));
            _p_interpolate_from_match(params,m,signal_contents);
            int process_id = *(int *)_t_surface(_t_child(action,1));
            int *code_path = (int *)_t_surface(_t_child(action,2));

            // @todo figure out how to refactor this with the similar
            // code in _r_deliver.
            debug(D_LOCK,"listen LOCK\n");
            pthread_mutex_lock(&q->mutex);
            Qe *e = __p_find_context(q->blocked,process_id);
            if (e) {
                T *result = _t_get(e->context->run_tree,code_path);
                if (!result) raise_error("failed to find code path when waking up expectation!");
                T *p = _t_parent(result);
                _t_replace(p,_t_node_index(result), params);
                e->context->node_pointer = params;

                __p_unblock(q,e);
                debug(D_LISTEN+D_SIGNALS,"unblocking action at %d,%s\n",process_id,_td(q->r,p));
            }
            else _t_free(params);
            pthread_mutex_unlock(&q->mutex);
            debug(D_LOCK,"listen UNLOCK\n");
            cleanup = true; //always cleanup after a wakeup because the context is gone.
        }
        else {
            Process p = *(Process*) _t_surface(action);
            T *params = _t_rclone(_t_child(expectation,ExpectationParamsIdx));  // __p_make_run_tree assumes rT nodes
            _p_interpolate_from_match(params,m,signal_contents);
            T *processes = _sem_get_defs(r->sem,p);
            rt = __p_make_run_tree(processes,p,params);
            _t_free(params);
            _t_add(signal,rt);
            _p_addrt2q(q,rt);
        }
        _t_free(m);
    }
    if (cleanup) {
        debug(D_SIGNALS,"cleaning up %s\n",_td(q->r,expectation));
        _r_remove_expectation(q->r,expectation);
    }

    _t_free(stx);
}

// what kind of sanatizing do we do of the actual response signal?
T* __r_sanatize_response(Receptor *r,T* response) {
    return _t_rclone(response);
}

/**
 * Send a signal to a receptor
 *
 * This function checks to see if the signal is a response and if so activates the run-tree/action that's
 * waiting for that response with the signal contents as the response value/param
 * or, if it's a new signal, adds it to the flux, and then runs through all the
 * expectations on the aspect the signal was sent on to see if it matches any expectation, and if so, builds
 * action run-trees and adds them to receptor's process queue.
 *
 * @param[in] r destination receptor
 * @param[in] signal signal to be delivered to the receptor
 * @todo for now the signal param is added directly to the flux.  Later it should probably be cloned? or there should be a parameter to choose?
 *
 * @returns Error
 * @todo figure out what kinds of errors could be returned by _r_deliver
 *
 * <b>Examples (from test suite):</b>
 * @snippet spec/receptor_spec.h testReceptorAction
 */
Error _r_deliver(Receptor *r, T *signal) {
    T *l;

    T *envelope = _t_child(signal,SignalEnvelopeIdx);

    // see if this is more than a plain send signal we are delivering
    // if there are END_CONDITIONS we know this is a request
    // if there is a an IN_RESPONSE_TO_UUID then we know it's a response
    T *extra=_t_child(envelope,EnvelopeExtraIdx);
    if (extra && semeq(IN_RESPONSE_TO_UUID,_t_symbol(extra))) {
        // responses don't trigger expectation matching, instead they
        // go through the pending_responses list to see where the signal goes
        UUIDt *u = (UUIDt*)_t_surface(extra);
        debug(D_SIGNALS,"Delivering response: %s\n",_td(r,signal));
        DO_KIDS(r->pending_responses,
                l = _t_child(r->pending_responses,i);
                if (__uuid_equal(u,(UUIDt *)_t_surface(_t_child(l,PendingResponseUUIDIdx)))) {

                    // get the end conditions so we can see if we should actually respond
                    T *ec = _t_child(l,PendingResponseEndCondsIdx);
                    bool allow;
                    bool cleanup;
                    evaluateEndCondition(ec,&cleanup,&allow);

                    if (allow) {
                        Symbol carrier = *(Symbol *)_t_surface(_t_child(l,PendingResponseCarrierIdx));
                        T *wakeup = _t_child(l,PendingResponseWakeupIdx);
                        int process_id = *(int *)_t_surface(_t_child(wakeup,WakeupReferenceProcessIdentIdx));
                        int *code_path = (int *)_t_surface(_t_child(wakeup,WakeupReferenceCodePathIdx));
                        T *response = (T *)_t_surface(_t_child(signal,SignalBodyIdx));
                        // now set up the signal so when it's freed below, the body doesn't get freed too
                        signal->context.flags &= ~TFLAG_SURFACE_IS_TREE;
                        Symbol signal_carrier = *(Symbol *)_t_surface(_t_child(envelope,EnvelopeCarrierIdx));
                        if (!semeq(carrier,signal_carrier)) {
                            debug(D_SIGNALS,"response failed carrier check, expecting %s, but got %s!\n",_r_get_symbol_name(r,carrier),_r_get_symbol_name(r,signal_carrier));
                            //@todo what kind of logging of these kinds of events?
                            break;
                        }

                        response = __r_sanatize_response(r,response);
                        // if the response isn't safe just break
                        if (!response) {
                            //@todo figure out if this means we should throw away the pending response too
                            break;
                        }

                        Q *q = r->q;
                        debug(D_LOCK,"deliver LOCK\n");
                        pthread_mutex_lock(&q->mutex);
                        Qe *e = __p_find_context(q->blocked,process_id);
                        if (e) {
                            T *result = _t_get(e->context->run_tree,code_path);
                            // if the code path was from a signal on the flux, it's root
                            // will be the receptor, so the first get will fail so try again
                            if (!result) result = _t_get(r->root,code_path);
                            // @todo that really wasn't very pretty, and what happens if that
                            // fails?  how do we handle it!?
                            if (!result) raise_error("failed to find code path when delivering response!");

                            debug(D_SIGNALS,"unblocking for response %s\n",_td(r,response));
                            _t_replace(_t_parent(result),_t_node_index(result), response);
                            e->context->node_pointer = response;

                            __p_unblock(q,e);
                        }
                        pthread_mutex_unlock(&q->mutex);
                        debug(D_LOCK,"deliver UNLOCK\n");
                    }

                    if (cleanup) {
                        debug(D_SIGNALS,"removing pending response: %s\n",_td(r,l));
                        _t_detach_by_idx(r->pending_responses,i);
                        //i--;
                        _t_free(l);
                    }
                    break;
                }
                );
        _t_free(signal);
    }
    else {

        if (extra && semeq(END_CONDITIONS,_t_symbol(extra))) {
            // determine if we will honor the request conditions?
            // perhaps that all happens at the protocol level
            // @todo anything specific we need to store here??
        }

        Aspect aspect = *(Aspect *)_t_surface(_t_child(envelope,EnvelopeAspectIdx));

        T *as = __r_get_signals(r,aspect);

        debug(D_SIGNALS,"Delivering: %s\n",_td(r,signal));
        _t_add(as,signal);
        // walk through all the expectations on the aspect and see if any expectations match this incoming signal
        T *es = __r_get_expectations(r,aspect);

        DO_KIDS(es,
                l = _t_child(es,i);
                __r_test_expectation(r,l,signal);
                );
    }
    return noDeliveryErr;
}

/******************  internal utilities */

T *__r_get_aspect(Receptor *r,Aspect aspect) {
    int i;
    T *a;
    DO_KIDS(r->flux,
            a = _t_child(r->flux,i);
            if (semeq(aspect,_t_symbol(a)))
                return a;
            );
    return NULL;
}
T *__r_get_expectations(Receptor *r,Aspect aspect) {
    return _t_child(__r_get_aspect(r,aspect),1);
}
T *__r_get_signals(Receptor *r,Aspect aspect) {
    return _t_child(__r_get_aspect(r,aspect),2);
}


/**
 * get the Receptor structure from an installed receptor
 */
Receptor * __r_get_receptor(T *installed_receptor) {
    // the receptor itself is the surface of the first child of the INSTALLED_RECEPTOR (bleah)
    if (!semeq(_t_symbol(installed_receptor),INSTALLED_RECEPTOR)) {
        raise_error("expecting an INSTALLED_RECEPTOR!");
    }
    return (Receptor *)_t_surface(_t_child(installed_receptor,1));
}

/*****************  Tree debugging utilities */

char *_r_get_symbol_name(Receptor *r,Symbol s) {
    return _sem_get_name(r->sem,s);
}

char *_r_get_structure_name(Receptor *r,Structure s) {
    return _sem_get_name(r->sem,s);
}

char *_r_get_process_name(Receptor *r,Process p) {
    return _sem_get_name(r->sem,p);
}

char __t_dump_buf[10000];

char *_td(Receptor *r,T *t) {
    __td(r,t,__t_dump_buf);
}

char *__td(Receptor *r,T *t,char *buf) {
    if (!t) sprintf(buf,"<null-tree>");
    else
        __t_dump(r->sem,t,0,buf);
    return buf;
}

/*****************  Built-in core and edge receptors */

Receptor *_r_makeStreamReaderReceptor(SemTable *sem,Symbol receptor_symbol,Symbol stream_symbol,Stream *st,ReceptorAddress to) {
    Receptor *r = _r_new(sem,receptor_symbol);

    // code is something like:
    // (do (not stream eof) (send to (read_stream stream line)))

    T *p = _t_new_root(ITERATE);
    T *params = _t_newr(p,PARAMS);
    T *eof = _t_newr(p,STREAM_ALIVE);

    _t_new_stream(eof,stream_symbol,st);
    //    _t_newi(p,TEST_INT_SYMBOL,2);  // two repetitions
    T *say = _t_newr(p,SAY);

    __r_make_addr(say,TO_ADDRESS,to);
    _t_news(say,ASPECT_IDENT,DEFAULT_ASPECT);
    _t_news(say,CARRIER,LINE);

    T *s = _t_new(say,STREAM_READ,0,0);
    _t_new_stream(s,stream_symbol,st);
    _t_new(s,RESULT_SYMBOL,&LINE,sizeof(Symbol));

    T *run_tree = __p_build_run_tree(p,0);
    _t_free(p);
    _p_addrt2q(r->q,run_tree);

    return r;
}

Receptor *_r_makeStreamWriterReceptor(SemTable *sem,Symbol receptor_symbol,Symbol stream_symbol,Stream *st) {
    Receptor *r = _r_new(sem,receptor_symbol);

    T *expect = _t_new_root(PATTERN);

    char *stx = "/<LINE:LINE>";

    // @fixme for some reason parseSemtrex doesn't clean up after itself
    // valgrind reveals that some of the state in the FSA that match the
    // semtrex are left un freed.  So I'm doing this one manually below.
    /* T *t = parseSemtrex(&r->defs,stx); */
    /*  _t_add(expect,t); */

    //    T *t =_t_news(expect,SEMTREX_GROUP,NULL_SYMBOL);
    T *t =_t_newr(expect,SEMTREX_SYMBOL_ANY);
    //    _t_news(x,SEMTREX_SYMBOL,LINE);

    /* char buf[1000]; */
    /* _dump_semtrex(r->sem,t,buf); */
    /* puts(buf); */

    T *x = _t_new_root(STREAM_WRITE);

    _t_new_stream(x,TEST_STREAM_SYMBOL,st);
    int pt1[] = {2,1,TREE_PATH_TERMINATOR};
    _t_new(x,PARAM_REF,pt1,sizeof(int)*4);

    T* params = _t_new_root(PARAMS);
    _t_news(params,INTERPOLATE_SYMBOL,NULL_SYMBOL);

    T *signature = __p_make_signature("result",SIGNATURE_SYMBOL,NULL_SYMBOL,
                                      "stream",SIGNATURE_STRUCTURE,STREAM,
                                      NULL);

    Process proc = _r_code_process(r,x,"echo input to stream","long desc...",signature);
    T *act = _t_newp(0,ACTION,proc);

    _r_add_expectation(r,DEFAULT_ASPECT,NULL_SYMBOL,expect,act,params,0);

    return r;
}

Receptor *_r_makeClockReceptor(SemTable *st) {
    Receptor *r = _r_new(st,CLOCK_RECEPTOR);

    T *expect = _t_new_root(PATTERN);
    T *s = _sl(expect,CLOCK_TELL_TIME);

    T *resp = _t_new_root(RESPOND);
    T *tick = __r_make_tick();  // initial tick, will get updated by clock thread.
    Xaddr x = _r_new_instance(r,tick);

    _t_news(resp,CARRIER,TICK);
    T *g = _t_newr(resp,GET);
    _t_new(g,GET_XADDR,&x,sizeof(Xaddr));

    T *signature = __p_make_signature("result",SIGNATURE_SYMBOL,NULL_SYMBOL,NULL);
    Process proc = _r_code_process(r,resp,"respond with current time","long desc...",signature);

    T *act = _t_newp(0,ACTION,proc);
    T *params = _t_new_root(PARAMS);
    _r_add_expectation(r,DEFAULT_ASPECT,CLOCK_TELL_TIME,expect,act,params,0);

    // this stuff was for the old clock which installed a expectation on itself to send back
    // the TICK when in next arrrived
    /* s = _t_news(s,SEMTREX_GROUP,PATTERN); */
    /* _sl(s,PATTERN); */

    /* T *x = _t_newr(0,LISTEN); */
    /* int pt1[] = {2,1,TREE_PATH_TERMINATOR}; */
    /* _t_new(x,PARAM_REF,pt1,sizeof(int)*4);  // param is our expectation semtrex */
    /* _t_news(x,CARRIER,PATTERN); */
    /* T *params =_t_newr(x,PARAMS); */

    /* ReceptorAddress to =  __r_get_self_address(r); */
    /* _t_newi(params,RECEPTOR_ADDRESS,to); */

    /* //interpolate on null_symbol matches the whole semtrex that triggered this */
    /* // expectation, which would be the current tick as it arrives */
    /* _t_news(params,INTERPOLATE_SYMBOL,NULL_SYMBOL); */
    /* _t_news(params,RESPONSE_CARRIER,NULL_SYMBOL); */

    /* T *action = _t_newp(x,ACTION,SEND); */

    /* T *signature = __p_make_signature("result",SIGNATURE_SYMBOL,NULL_SYMBOL, */
    /*                                   "time stx",SIGNATURE_SYMBOL,PATTERN, */
    /*                                   NULL); */
    /* Process proc = _r_code_process(r,x,"plant a expectation to send the time","long desc...",signature); */
    /* T *act = _t_newp(0,ACTION,proc); */

    /* params = _t_new_root(PARAMS); */
    /* _t_news(params,INTERPOLATE_SYMBOL,PATTERN); */

    /* _r_add_expectation(r,DEFAULT_ASPECT,CLOCK_TELL_TIME,expect,act,params,0);*/

    return r;
}

/**
    bad implementation of the clock receptor thread (but easy):
   - wake up every second
   - build a TICK symbol based on the current time.
   - set the Xaddr of the current tick

  @todo: a better implementation would be to analyze the semtrex expectations that have been planted
  and only wakeup when needed based on those semtrexes

 *
 * @param[in] the clock receptor
 */
void *___clock_thread(void *arg){
    Receptor *r = (Receptor*)arg;
    debug(D_CLOCK,"clock started\n");
    int err =0;
    ReceptorAddress self = __r_get_self_address(r);
    while (r->state == Alive) {
        T *tick =__r_make_tick();
        debug(D_CLOCK,"%s\n",_td(r,tick));
        Xaddr x = {TICK,1};
        _r_set_instance(r,x,tick);
        //        T *signal = __r_make_signal(self,self,DEFAULT_ASPECT,TICK,tick,0,0);
        //        _r_deliver(r,signal);
        sleep(1);
        /// @todo this will skip some seconds over time.... make more sophisticated
        //       with nano-sleep so that we get every second?
    }
    debug(D_CLOCK,"clock stopped\n");
    pthread_exit(&err); //@todo determine if we should use pthread_exit or just return 0
    return 0;
}

T * __r_make_timestamp(Symbol s,int delta) {
    struct tm t;
    time_t clock;
    time(&clock);
    clock += delta;
    gmtime_r(&clock, &t);
    T *tick = _t_new_root(s);
    T *today = _t_newr(tick,TODAY);
    T *now = _t_newr(tick,NOW);
    _t_newi(today,YEAR,t.tm_year+1900);
    _t_newi(today,MONTH,t.tm_mon+1);
    _t_newi(today,DAY,t.tm_mday);
    _t_newi(now,HOUR,t.tm_hour);
    _t_newi(now,MINUTE,t.tm_min);
    _t_newi(now,SECOND,t.tm_sec);
    return tick;
}

void __r_kill(Receptor *r) {
    r->state = Dead;
    /* pthread_mutex_lock(&shutdownMutex); */
    /* G_shutdown = val; */
    /* pthread_mutex_unlock(&shutdownMutex); */
}

ReceptorAddress __r_get_self_address(Receptor *r) {
    return r->addr;
}

void __r_dump_instances(Receptor *r) {
    Instances *i = &r->instances;
    T *t = _t_new_root(PARAMS);  // bogus symbol, just using to build the tree
    instances_elem *cur,*tmp;
    HASH_ITER(hh, *i, cur, tmp) {
        T *sym = _t_news(t,STRUCTURE_SYMBOL,cur->s);  // just using this symbol to store the symbol type
        int is_receptor = semeq(cur->s,INSTALLED_RECEPTOR);
        Instance *iP = &cur->instances;
        instance_elem *curi,*tmpi;
        HASH_ITER(hh, *iP, curi, tmpi) {
            T *c = _t_clone(curi->instance);
            _t_add(sym,c);
        }
    }
    printf("INSTANCES:%s\n",_t2s(r->sem,t));
    _t_free(t);
}
/** @}*/
