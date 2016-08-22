/* file "bbpart/bbpart.h" */
/*
 *     Copyright (c) 2004, 2005, 2006, 2007, 2008, 2009, 2010,
 *                   2011, 2012, 2013, 2014, 2015, 2016 Nikolaos Kavvadias
 *
 *     This software was written by Nikolaos Kavvadias, Ph.D. candidate
 *     at the Physics Department, Aristotle University of Thessaloniki,
 *     Greece (at the time).
 *
 *     This software is provided under the terms described in
 *     the "machine/copyright.h" include file.
 */

#ifndef BBPART_BBPART_H
#define BBPART_BBPART_H

#include <machine/copyright.h>
#include <cfg/cfg.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma interface "bbpart/bbpart.h"
#endif

#include <machine/machine.h>

class BbPart {
  public:
    BbPart() { }

    void initialize() { }
    void do_opt_unit(OptUnit*);
    void finalize() { }
    bool is_false_dependency(CfgNode*, int, int, Opnd);
    void process_sym_table(FILE*, Printer*, CPrinter*, SymTable*);


    // set pass options 
//    void set_show_layout(bool sl)                { show_layout = sl; }
//    void set_show_code(bool sc)             { show_code = sc; }
    void set_enable_dt(bool et)             { enable_dt = et; }
    void set_enable_mark_false_deps(bool fd)       { enable_mark_false_deps = fd; }
    void set_enable_global_symbol_table(bool gst)       { enable_global_symbol_table = gst; }

  protected:
//    bool show_layout;
//    bool show_code;
    bool enable_dt;
    bool enable_mark_false_deps;
    bool enable_global_symbol_table;
};

#endif /* BBPART_BBPART_H */
