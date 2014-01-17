#include "../ceptr.h"

#define MAX_RECEPTORS 4

enum { VM, STDIN, STDOUT };

typedef int Address;

typedef struct {
    Receptor receptors[MAX_RECEPTORS];
    int receptor_count;
    pthread_t threads[MAX_RECEPTORS];
} HostReceptor;


void null_proc(Receptor *r) {
    printf("Got a log message.  I do nothing\n");
}

Receptor *resolve(HostReceptor *r, Address addr) {
    Receptor *dr = &r->receptors[addr];
    if (dr == 0) {
        raise_error("whatchu talking about addred %d\n", addr );
        return 0;
    }
    return dr;
}

LogProc getLogProc(Receptor *r) {
    return r->logProc;
}

void wakeup(Receptor *r){
    r->logChange = true;
}

void data_write_log(HostReceptor *h, Receptor *r, Symbol noun, void *surface, size_t length) {
    // HostReceptor h is here just to make it clear this isn't internal operation on r
    int new_log_head = r->data.log_head + 1;
    if (new_log_head >= MAX_LOG_ENTRIES) {
        new_log_head = 0;
    }
    if (new_log_head == r->data.log_tail) {
        raise_error0("buffer overflow");
    }

    assert( pthread_mutex_lock( &r->data.log_mutex) == 0);

    r->data.log[new_log_head].noun = noun;
    memcpy( r->data.log[new_log_head].content, surface, length);
    r->data.log_head = new_log_head;

    assert( pthread_mutex_unlock( &r->data.log_mutex) == 0);

//    wakeup(r);
}

void write_out(HostReceptor *h, Receptor *r, Symbol noun, void *surface, size_t length) {
    for (int i=0; i < r->listenerCount; i++){
        data_write_log(h, r->listeners[i], noun, surface, length);
    }
}

typedef struct {
    Address destination;
    Xaddr value;
} Packet;

void _send_message(HostReceptor *r, Address destination, Symbol noun, void *surface, size_t size) {
    Receptor *dest_receptor = resolve(r, destination);
    data_write_log(r, dest_receptor, noun, surface, size);
}

void send_message(Receptor *r, Packet *p) {
    void *surface = surface_for_xaddr(r, p->value);
    size_t size = size_of_named_surface(r, p->value.noun, surface);
    _send_message(r->parent, p->destination, p->value.noun, surface, size);
}

void listen(Receptor *r, Address addr) {
    HostReceptor *h = (HostReceptor *)r->parent;
    Receptor *l = resolve(h, addr);
    l->listeners[l->listenerCount++] = r;
}

void dump_named_surface(Receptor *r, Symbol noun, void *surface) {
    if (spec_noun_for_noun(r, noun) == r->patternSpecXaddr.noun) {
        dump_pattern_spec(r, surface);
    } else if (spec_noun_for_noun(r, noun) == r->intPatternSpecXaddr.noun) {
        proc_int_print(r, surface);
    } else {
        dump_xaddrs(r);
        raise_error("dunno how to dump named surface %d\n", noun);
    }
}

void stdin_poll_proc(Receptor *r){
    printf("stdin_poll_proc\n");
    int c;
    c = getchar();
    while(c != EOF){
        write_out((HostReceptor *)r->parent, r, r->charIntNoun, &c, 4);
        c = getchar();
    }
}

void stdout_log_proc(Receptor *r, LogEntry *le) {
    dump_named_surface(r, le->noun, &le->content );
}


void *receptor_task(void *arg) {
    Receptor *r = (Receptor *)arg;
    // r->initProc;
    r->alive = true;
    while(r->alive) {
        sleep(1);
        if (r->pollProc != 0) {
            (r->pollProc)(r);
        }
        if (logChange(r) && r->logProc) {
            LogEntry *le = &r->data.log[r->data.log_tail];
            (r->logProc)(r, le);
            int new_log_tail = r->data.log_tail + 1;
            if (new_log_tail >= MAX_LOG_ENTRIES) {
                new_log_tail = 0;
            }
            r->data.log_tail = new_log_tail;
        }
        // onLogChange  r->logProc;
    }
    return NULL;
}
void _vmh_receptor_new(HostReceptor *r, Address addr, LogProc lp, LogProc pp) {
    init(&r->receptors[addr]);
    r->receptors[addr].logProc = lp;
    r->receptors[addr].pollProc = pp;
    r->receptors[addr].parent = r;

    int rc;
printf("creating thread for addr %d\n", addr);

    rc = pthread_create(&r->threads[addr], NULL, receptor_task, (void *) &r->receptors[addr]);
    assert(0 == rc);

}

Receptor *vmh_receptor_new(HostReceptor *r, LogProc lp) {
    _vmh_receptor_new(r, ++r->receptor_count, lp, 0);
    return &r->receptors[r->receptor_count];
}

void vm_host_init(HostReceptor *r){
    r->receptor_count = 0;
    init(&r->receptors[VM]);

    _vmh_receptor_new(r, STDIN, null_proc, stdin_poll_proc);
    r->receptor_count = STDIN;

    _vmh_receptor_new(r, STDOUT, stdout_log_proc, 0);
    r->receptor_count = STDOUT;

}


void vm_host_run(HostReceptor *h) {
//    for (int i=0; i < h->receptor_count; i++){
//        if (h->receptors[i].pollProc != 0) {
//            (h->receptors[i].pollProc)(&h->receptors[i]);
//        }
//    }

    int rc;
    for (int i = 1; i < h->receptor_count; i++) {
        rc = pthread_join(h->threads[i], NULL);
        assert(0 == rc);
    }

    //
//    repl {
//        Code c;
//        Instruction *i = &c;
//        while(true) {
//            switch(i.op) {
//                case LISTEN:
//                case INVOKE:
//                case PUSH:
//                case POP:
//            }
//        }
//    }

}