/* file "bbpart/bbpart.cpp" */
/*
 *     Copyright (c) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 
 *                   2012, 2013, 2014 Nikolaos Kavvadias
 *
 *     This software was written by Nikolaos Kavvadias, Ph.D. candidate
 *     (at the time) at the Physics Department, Aristotle University of 
 *     Thessaloniki, Greece.
 *
 *     This software is provided under the terms described in
 *     the "machine/copyright.h" include file.
 */

#include <machine/copyright.h>

#ifdef USE_PRAGMA_INTERFACE
#pragma implementation "bbpart/bbpart.h"
#endif

#include <machine/machine.h>
//#include <arm/arm.h>
#include <cfg/cfg.h>
#include <bvd/bvd.h>

#include <bbpart/bbpart.h>
#include "utils.h"

#ifdef USE_DMALLOC
#include <dmalloc.h>
#define new D_NEW
#endif


bool is_legal_opnd(Opnd opnd)
{
  if ((is_reg(opnd) || is_var(opnd) || is_addr_sym(opnd)) && !is_null(opnd))
    return true;
  else
    return false;
}

bool is_real_instruction(Instr *mi)
{
  if (!is_label(mi) && !is_dot(mi) && !is_null(mi))
    return true;
  else
    return false;
}

/*
 * Generate vcg format file for viewing with xvcg.
 */
void
generate_vcg_bb(FILE *f, CfgNode *cfgnode, int DAG_adj_matrix[1000][1000])
{
    /* Use minbackward layout algorithm in an attempt to make all backedges  */
    /* true backedges. Doesn't always work, but the back algorithm for this. */
    /* Change these parameters as needed, but they have worked fine so far.  */
    fprintf(f,"graph: { title: \"BB_GRAPH\"\n");
    fprintf(f,"\n");
    fprintf(f,"x: 30\n");
    fprintf(f,"y: 30\n");
    fprintf(f,"height: 800\n");
    fprintf(f,"width: 500\n");
    fprintf(f,"stretch: 60\n");
    fprintf(f,"shrink: 100\n");
    fprintf(f,"layoutalgorithm: minbackward\n");
    fprintf(f,"node.borderwidth: 3\n");
    fprintf(f,"node.color: white\n");
    fprintf(f,"node.textcolor: black\n");
    fprintf(f,"node.bordercolor: black\n");
    fprintf(f,"edge.color: black\n");
    fprintf(f,"\n");

    int i,j;
    
    Printer *printer = target_printer();
    printer->set_file_ptr(f);

    i = 0;
    j = 0;

     for (InstrHandle h=instrs_start(cfgnode); h!=instrs_end(cfgnode); ++h)
     {
        fprintf(f,"node: { title:\"%d\" label:\"",i);
        printer->print_instr(*h);
        fprintf(f,"\"}\n");
        i++;
     }
     fprintf(f,"\n");

    i = 0;
    j = 0;

    /* Put down edges and backedges */
    for (i=0; i<instrs_size(cfgnode); i++) 
   {
      for (j=0; j<instrs_size(cfgnode); j++)
      {
         if (DAG_adj_matrix[i][j]==1)
         {
            fprintf(f, "edge: {sourcename:\"%d\" targetname:\"%d\" }\n", i,j);
         }
       }
     }

   /* Wrap up at the end */
    fprintf(f,"\n");
    fprintf(f, "}\n");
}   

//
// The is_false_dependency flag is used to cancel edges that are WRONGFULLY
// identified within the original DDG extraction algorithm implementation.
// Actually, the DDG extraction algorithm can be used in its basic form
// only for SUIFvm (machine with unlimited register resources) and only
// for SSA-type CFGs!
//
bool is_true_dependency(Liveness *liveness_tmp, CfgNode* current_node, Instr *mid, Instr *mjd)
{
  bool is_true_dependency_flag=false;

  //liveness_tmp->find_kill_and_gen(mid);
  liveness_tmp->find_kill_and_gen(mjd);

  for (NatSetIter kit(liveness_tmp->kill_set()->iter()); kit.is_valid(); kit.next())
  {
    liveness_tmp->find_kill_and_gen(mjd);

    for (NatSetIter git(liveness_tmp->gen_set()->iter()); git.is_valid(); git.next())
    {
      // If a TRUE dependency is found
      if (kit.current() == git.current() || (is_cti(mid) || is_cti(mjd) || writes_memory(mid) || writes_memory(mjd)))
      {
        is_true_dependency_flag = true;
	//printf("Found a real dependency: (%d->%d).\n",(int)kit.current(),(int)git.current());
      }
    }
  }

  return is_true_dependency_flag;
}

//
// The is_false_dependency flag is used to cancel edges that are WRONGFULLY
// identified within the original DDG extraction algorithm implementation.
// Actually, the DDG extraction algorithm can be used in its basic form
// only for SUIFvm (machine with unlimited register resources) and only
// for SSA-type CFGs!
//
bool BbPart::is_false_dependency(CfgNode* current_node, int mi_lo_pos, int mi_hi_pos, Opnd offending_opnd)
{
  bool is_false_dependency_flag=false;
  int mk_cur_pos=0;
  Opnd local_opnd_dst;

  if (enable_mark_false_deps)
  {
  // Iterate through the [mi_lo_pos..mi_hi_pos] range of the basic-block instructions
  for (InstrHandle hk = instrs_start(current_node); hk != instrs_end(current_node); ++hk)
  {
    Instr *mk = *hk;

    // if the current mi is within the specified ranges
    if (mk_cur_pos<mi_hi_pos && mk_cur_pos>mi_lo_pos)
    {
      // Get destination operand of mi
      for (OpndHandle mk_oh=dsts_start(mk); mk_oh!=dsts_end(mk); ++mk_oh)
      {
        if (is_base_disp(get_dst(mk, mk_oh)) && writes_memory(mk))
	{
	  BaseDispOpnd bdo_dst = get_dst(mk, mk_oh);
	  local_opnd_dst = bdo_dst.get_base();
	}
	else if (is_sym_disp(get_dst(mk, mk_oh)) && writes_memory(mk))
	{
          SymDispOpnd sdo_dst = get_dst(mk, mk_oh);
          local_opnd_dst = sdo_dst.get_addr_sym();
	}
	else
	  local_opnd_dst = get_dst(mk, mk_oh);

	// compare the offending operand to a destination operand of mk
	if (local_opnd_dst == offending_opnd)
	{
	  is_false_dependency_flag |= true;
	  fprintf(stdout,"Found false dependency: %d in {%d,%d}\n", mk_cur_pos, mi_lo_pos, mi_hi_pos);
	}
      }
    }

    // Increment BB instruction counter
    mk_cur_pos++;
  }
  }
  
  return is_false_dependency_flag;
}

void 
BbPart::process_sym_table(FILE *out, Printer *printer, CPrinter *cprinter, SymTable *st)
{
    bool st_is_private = is_private(st);

    Iter<SymbolTableObject*> iter = st->get_symbol_table_object_iterator();
    for ( ; iter.is_valid(); iter.next()) {

	SymbolTableObject *the_sto = iter.current();

	if (is_kind_of<VariableSymbol>(the_sto)) {

	    if (is_kind_of<ParameterSymbol>(the_sto))
		continue;			// don't shadow an arg!

	    VarSym *v = to<VariableSymbol>(the_sto); 

	    if (!is_global(st)) {
		printer->print_var_def(v);
//		cprinter->print_var_def(v, false);
	    }
	    else if (v->get_definition() != NULL) {
//		postponed_vars.push_back(v);
		printer->print_var_def(v);
//		cprinter->print_var_def(v, true);
	    }
	    else {
		claim(is_external(v));
		fprintf(out, "extern ");
//		printer->print_sym_decl(v);
//		cprinter->print_sym_decl(v);
		fprintf(out, ";\n");
	    }
	} 
//        else if (is_kind_of<ProcedureSymbol>(the_sto)) {
//	    if (st_is_private) fputs("static ", out);
//	    printer->print_proc_decl(to<ProcedureSymbol>(the_sto));
//
//	} 
        else if (is_kind_of<Type>(the_sto)) {
	    if (is_kind_of<EnumeratedType>(the_sto) ||
		(is_kind_of<GroupType>(the_sto) &&
		 to<GroupType>(the_sto)->get_is_complete())) {
//		printer->print_type(to<Type>(the_sto));
//		cprinter->print_type(to<Type>(the_sto));
		fprintf(out, ";\n");
	    }
	}
    }
}

bool is_mortal(Opnd opnd)
{
	if(is_reg(opnd))
		return true;
	if(is_var(opnd))
	{
		VarSym *vs = get_var(opnd);
		return (!is_addr_taken(vs) && is_auto(vs));
	}
	return false;
}

void query_data_type(Opnd opnd, char dt_info[8])
{
    strcpy(dt_info,"");

    TypeId opnd_type = get_type(opnd);
    int opnd_size = get_bit_size(opnd_type);

    if (is_floating_point(opnd_type))
    {
      // f32, f64, f128
      switch (opnd_size)
      {
        case 32:
          strcpy(dt_info,"f32");
          break;
        case 64:
          strcpy(dt_info,"f64");
          break;
        case 128:
          strcpy(dt_info,"f128");
          break;
        default:
          strcpy(dt_info,"na");
          break;
      }
    }
    else if (is_integral(opnd_type))
    {    
      // u8, u16, u32, u64
      if (!is_signed(opnd_type))
      {
        switch (opnd_size)
        {
          case 8:
            strcpy(dt_info,"u8");
            break;
          case 16:
            strcpy(dt_info,"u16");
            break;
          case 32:
            strcpy(dt_info,"u32");
            break;
          case 64:
            strcpy(dt_info,"u64");
            break;
          default:
            strcpy(dt_info,"na");
            break;
        }
      }
      // s8, s16, s32, s64
      else if (is_signed(opnd_type))
      {
        switch (opnd_size)
        {
          case 8:
            strcpy(dt_info,"s8");
            break;
          case 16:
            strcpy(dt_info,"s16");
            break;
          case 32:
            strcpy(dt_info,"s32");
            break;
          case 64:
            strcpy(dt_info,"s64");
            break;
          default:
            strcpy(dt_info,"na");
            break;
        }
      }
      else
      {
        strcpy(dt_info,"na");
      }
    }         
    else if (is_void(opnd_type) && opnd_size==0)
    {
      // v0
      strcpy(dt_info,"v0");
    }
    else if (is_pointer(opnd_type))
    {
      // p32, p64
      switch (opnd_size)
      {
        case 32:
          strcpy(dt_info,"p32");
          break;
        case 64:
          strcpy(dt_info,"p64");
          break;
        default:
          strcpy(dt_info,"na");
          break;
      }
    }
    // anything else
    else
    {
      strcpy(dt_info,"na");
    }
}

void print_operand_dt_info(FILE *outfile, Opnd opnd, char dt_arr[8], bool enable_flag)
{
  if (enable_flag)
  {
    query_data_type(opnd, dt_arr);
    fprintf(outfile," [%s]",dt_arr);
  }
}


void BbPart::do_opt_unit(OptUnit *unit)
{
  OptUnit *cur_unit;
  /**/
  int bb_num = 0;           // ID number of the current basic block
  int bb_num_itrack = 0;    // As above, but only for bb_instr_num tracking
  int instr_num = 0;        // Number of instruction in the current basic block
  char bb_num_s[25];        // String equivalent to bb_num
  char local_dt_info[8];    // Buffer for data type information (of operands)
  /**/
  // FILE to store the dependency matrices for all basic blocks
  FILE *f_adjmat_file,*f_bbcounters_file,*f_bbinstrnum_file;
  /**/
  int mi_opcode;
  //
  static int procedure_count = 0;

  // Open dependency matrix output file
  if (procedure_count==0)
  {
    f_adjmat_file = fopen("adjmat.txt","w");
    fprintf(f_adjmat_file,"Title ;\n");
  }
  else
    f_adjmat_file = fopen("adjmat.txt","a");

   // Open BB counters output file
  if (procedure_count==0)
    f_bbcounters_file = fopen("bb_counters.txt","w");
  else
    f_bbcounters_file = fopen("bb_counters.txt","a");

   // Open num. of instructions output file
  if (procedure_count==0)
    f_bbinstrnum_file = fopen("bb_instr_num.rpt","w");
  else
    f_bbinstrnum_file = fopen("bb_instr_num.rpt","a");

  Printer *printer = target_printer();
  printer->set_file_ptr(f_adjmat_file);
  CPrinter *c_printer = target_c_printer();

  // Report global symbol table for this FileBlock.
  // Should be done only once (for procedure_count==0)
  if (enable_global_symbol_table)
  {
  if (procedure_count==0)
  {
    fprintf(f_adjmat_file,"Global_symbol_table;\n");

//    printer->print_global_decl(the_file_block);
//    c_printer->process_sym_table(SymTable *st);

    process_sym_table(f_adjmat_file, printer, c_printer, external_sym_table());
    process_sym_table(f_adjmat_file, printer, c_printer, file_set_sym_table());
    process_sym_table(f_adjmat_file, printer, c_printer, get_sym_table(the_file_block));

    fprintf(f_adjmat_file,"end Global_symbol_table;\n");
  }
  }

  // Make local copy of the optimization unit
  cur_unit = unit;

  // Report name of the CFG under processing
  const char *cur_proc_name = get_name(cur_unit).chars();

  // Get the body of the OptUnit
  AnyBody *cur_body = get_body(cur_unit);

  // verify that it is a CFG
  claim(is_kind_of<Cfg>(cur_body), "expected OptUnit's body in CFG form");

  // Create a local copy of the input CFG
  Cfg *cfg = (Cfg *)cur_body;
  //canonicalize(cfg,false,true,false);
  remove_unreachable_nodes(cfg);

  OpndCatalog *opnd_catalog = new RegSymCatalog(true, is_mortal);
  DefUseAnalyzer *du_analyzer = new DefUseAnalyzer(opnd_catalog);
  Liveness *liveness = new Liveness(cfg, opnd_catalog, du_analyzer);


  // Report which is the current procedure
  fprintf(f_adjmat_file, "Procedure name : %s;\n",cur_proc_name);

  // Iterate through the nodes of the CFG
  for (CfgNodeHandle cfg_nh=nodes_start(cfg); cfg_nh!=nodes_end(cfg); ++cfg_nh)
  {
    // Get the current node
    CfgNode* cnode = get_node(cfg, cfg_nh);

    /****************************************************************/
    /********************* INSIDE A CFGNODE ***********************/
    /****************************************************************/

    // Clear temporary variables
    Opnd opnd_src, opnd_dst, opnd_dst2, opnd_dependency;
    Opnd opnd_src2, opnd_dst3, opnd_src_tmp2;

    // Allocate memory for the adjacency matrix
    int DAG_adj_matrix[1000][1000];

    // Obtain bb_num_s alphanumeric representation of bb_num integer
    itoa(bb_num, bb_num_s);

    // Create the actual name of the adjmat identifier for the current cfgnode (BB)
    //char *bb_counter_name = (new char[strlen(cur_proc_name) + strlen(bb_num_s) + 10]);

    // Print the string for "adjmat_name" e.g. classify_6_adjmat
    //sprintf(bb_counter_name, "cnt_%s.%s", cur_proc_name, bb_num_s);

    // Print the entry for "bb_name" e.g. BB name : 2
    fprintf(f_adjmat_file, "BB name : %s;\n", bb_num_s);

    char *adjmat_name_suffix = "edglst";
    // Create the actual name of the adjmat identifier for the current cfgnode (BB)
    char *adjmat_name = new char[strlen(cur_proc_name) + strlen(bb_num_s) + strlen(adjmat_name_suffix) + 10];
    // Print the string for "adjmat_name" e.g. classify_6_adjmat
    sprintf(adjmat_name, "%s:%s:%s", cur_proc_name, bb_num_s, adjmat_name_suffix);

    int i = 0;
    int j = 0;

    // Clear dependency matrix contents
    for (i=0; i<instrs_size(cnode); i++)
    {
      for (j=0; j<instrs_size(cnode); j++)
      {
          DAG_adj_matrix[i][j] = 0;
      }
    }

    i = 0;
    j = 0;

    // Construct dependency matrix
    for (InstrHandle h1 = instrs_start(cnode); h1 != instrs_end(cnode); ++h1)
    {
      Instr *mi = *h1;

    //  j = 0;

      if (is_real_instruction(mi))
      {
       j = 0;

        for (InstrHandle h2 = instrs_start(cnode); h2 != instrs_end(cnode); ++h2)
        {
          Instr *mj = *h2;

          opnd_dependency = opnd_null();

          if (is_real_instruction(mj))
          {
            for (OpndHandle mi_oh=dsts_start(mi); mi_oh!=dsts_end(mi); ++mi_oh)
            {

              if (is_base_disp(get_dst(mi, mi_oh)) && writes_memory(mi))
              {
                BaseDispOpnd bdo_dst = get_dst(mi, mi_oh);
                opnd_dst = bdo_dst.get_base();
              }
              else if (is_sym_disp(get_dst(mi, mi_oh)) && writes_memory(mi))
              {
                SymDispOpnd sdo_dst = get_dst(mi, mi_oh);
                opnd_dst = sdo_dst.get_addr_sym();
              }
              else if (is_index_sym_disp(get_dst(mi, mi_oh)) && writes_memory(mi))
              {
                IndexSymDispOpnd isdo_dst = get_dst(mi, mi_oh);
                opnd_dst = isdo_dst.get_addr_sym();
              }
              else
                 opnd_dst = get_dst(mi, mi_oh);

              /* Dst->Src operand data dependencies */
              for (OpndHandle mj_oh=srcs_start(mj); mj_oh!=srcs_end(mj); ++mj_oh)
              {
                if (is_base_disp(get_src(mj, mj_oh)) && reads_memory(mj))
                {
                  BaseDispOpnd bdo_src = get_src(mj, mj_oh);
                  opnd_src = bdo_src.get_base();
                }
                else if (is_sym_disp(get_src(mj, mj_oh)) && reads_memory(mj))
                {
                  SymDispOpnd sdo_src = get_src(mj, mj_oh);
                  opnd_src = sdo_src.get_addr_sym();
                }
		else if (is_addr_sym(get_src(mj, mj_oh)) && reads_memory(mj))
		{
		  opnd_src = get_src(mj, mj_oh);
		}
		else if (is_index_sym_disp(get_src(mj, mj_oh)) && reads_memory(mj))
                {
                  IndexSymDispOpnd isdo_src = get_src(mj, mj_oh);
                  opnd_src = isdo_src.get_addr_sym();
		  opnd_src2 = isdo_src.get_index();
                }
                else
                  opnd_src = get_src(mj, mj_oh);

                /*
		if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_src) && is_legal_opnd(opnd_dst) && opnd_src==opnd_dst &&
		    is_true_dependency(liveness, cnode, mi, mj) && (i<j))
		*/
                if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_src) && is_legal_opnd(opnd_dst) && opnd_src==opnd_dst &&
		    !is_false_dependency(cnode, i, j, opnd_src) && (i<j))
		{
                   DAG_adj_matrix[i][j] = 1;
                   opnd_dependency = opnd_src;
                   //
                   fprintf(f_adjmat_file,"%s[%d][%d] =",adjmat_name,i,j);
                   printer->print_opnd(opnd_dependency);
                   print_operand_dt_info(f_adjmat_file, opnd_dependency, local_dt_info, enable_dt);
                   fprintf(f_adjmat_file,";\n");
                }
		/*
		else if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_src2) && is_legal_opnd(opnd_dst) && opnd_src2==opnd_dst &&
		    is_true_dependency(liveness, cnode, mi, mj) && (i<j))
		*/
		else if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_src2) && is_legal_opnd(opnd_dst) && opnd_src2==opnd_dst &&
		    !is_false_dependency(cnode, i, j, opnd_src2) && (i<j))
		{
                   DAG_adj_matrix[i][j] = 1;
                   opnd_dependency = opnd_src2;
                   //
                   fprintf(f_adjmat_file,"%s[%d][%d] =",adjmat_name,i,j);
                   printer->print_opnd(opnd_dependency);
                   fprintf(f_adjmat_file,";\n");
                }
                else
                {
                  DAG_adj_matrix[i][j] = 0;
                }
              } /* mj_oh */

              /* Dst->Dst operand data dependencies */
              for (OpndHandle mj_oh=dsts_start(mj); mj_oh!=dsts_end(mj); ++mj_oh)
              {
                if (is_base_disp(get_dst(mj, mj_oh)) && writes_memory(mj))
                {
                  BaseDispOpnd bdo_dst2 = get_dst(mj, mj_oh);
                  opnd_dst2 = bdo_dst2.get_base();
                }
               else if (is_sym_disp(get_dst(mj, mj_oh)) && writes_memory(mj))
                {
                  SymDispOpnd sdo_dst2 = get_dst(mj, mj_oh);
                  opnd_dst2 = sdo_dst2.get_addr_sym();
                }
		else if (is_index_sym_disp(get_dst(mj, mj_oh)) && writes_memory(mj))
                {
                  IndexSymDispOpnd isdo_dst2 = get_dst(mj, mj_oh);
                  opnd_dst2 = isdo_dst2.get_addr_sym();
		  Opnd opnd_dst3 = isdo_dst2.get_index();
                }
                else
                  opnd_dst2 = opnd_null();

                /*
		if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_dst2) && is_legal_opnd(opnd_dst) && opnd_dst2==opnd_dst &&
		    is_true_dependency(liveness, cnode, mi, mj) && (i<j))
		*/
		if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_dst2) && is_legal_opnd(opnd_dst) && opnd_dst2==opnd_dst &&
		    !is_false_dependency(cnode, i, j, opnd_dst2) && (i<j))
		{
                   DAG_adj_matrix[i][j] = 1;
                   opnd_dependency = opnd_dst; // src?
                   //
                   fprintf(f_adjmat_file,"%s[%d][%d] =",adjmat_name,i,j);
                   printer->print_opnd(opnd_dependency);
                   fprintf(f_adjmat_file,";\n");
                }
		/*
		else if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_dst3) && is_legal_opnd(opnd_dst) && opnd_dst3==opnd_dst &&
		    is_true_dependency(liveness, cnode, mi, mj) && (i<j))
                */
		else if (is_real_instruction(mi) && is_real_instruction(mj) && is_legal_opnd(opnd_dst3) && is_legal_opnd(opnd_dst) && opnd_dst3==opnd_dst &&
		    !is_false_dependency(cnode, i, j, opnd_dst3) && (i<j))
		{
                   DAG_adj_matrix[i][j] = 1;
                   opnd_dependency = opnd_dst;
                   //
                   fprintf(f_adjmat_file,"%s[%d][%d] =",adjmat_name,i,j);
                   printer->print_opnd(opnd_dependency);
                   fprintf(f_adjmat_file,";\n");
                }
                else
                {
                  DAG_adj_matrix[i][j] |= 0;
                }
              } /* mj_oh */

            } /* mi_oh */
	    j++;
          } /* is_real_instruction */
          /*j++;*/
        }
        i++;
      }
      /*i++;*/
    }

    /****************************************************************/
    /****************** CDFG generation in VCG format ***************/
    /****************************************************************/ 

    // Create the actual name of the VCG representation of the current BB
    char *vcg_name = new char[strlen(cur_proc_name) + strlen(bb_num_s) + 10];
    // Print the string for "vcg_name" e.g. classify_6_adjmat
    sprintf(vcg_name, "%s_%s", cur_proc_name, bb_num_s);

    // Print the actual name of the VCG output file for the current BB
    char *f_vcg_name = new char[strlen(vcg_name) + strlen(".vcg") + 10];

    // Print the correct string for "f_vcg_name" e.g. main_001.vcg
    sprintf(f_vcg_name, "%s.vcg", vcg_name);

    // Create output file for writing the CFG
    FILE *f_vcg_file = fopen(f_vcg_name, "w");      

    // Generate VCG representation of the DAG for the current basic block
    generate_vcg_bb(f_vcg_file, cnode, DAG_adj_matrix);

    // Close VCG file
    fclose(f_vcg_file);

    // Delete names since these are temporaries
    // Delete vcg_name
    delete vcg_name;
    vcg_name = NULL;
    // Delete f_vcg_name
    delete f_vcg_name;
    f_vcg_name = NULL;

    /****************************************************************/
    /****************** end of CDFG generation in VCG format ********/
    /****************************************************************/ 

    //-----------------------------------------------------------------
    //
    Opnd opnd_src_tmp, opnd_dst_tmp;
    int count_dst=0,count_src=0,count_immint=0,count_cti_target=0;
    char *mi_opcode_s;
    Sym *call_target0, *cti_target;
    //

    instr_num = 0;

    // Print machine instruction opcode + operands (except immediate)
    for (InstrHandle h1 = instrs_start(cnode); h1 != instrs_end(cnode); ++h1)
    {
      Instr *mi = *h1;

      count_src = 0;
      count_dst = 0;
      count_immint = 0;
      count_cti_target = 0;
      LabelSym *label_target;

      if (is_label(mi))
      {
        // Print label string to file
        fprintf(f_adjmat_file,"label=");

        // Print the actual label entry
        // It will be stored in the "target" storage.
        label_target = get_label(mi);
        printer->print_sym(label_target);
        fprintf(f_adjmat_file,";\n");
      }

      if (is_real_instruction(mi))
      {
        // Get opcode of mi
        mi_opcode = get_opcode(mi);

	// Print opcode in assembly-like notation
	mi_opcode_s = opcode_name(mi_opcode);

        // Print opcode string to file
        fprintf(f_adjmat_file,"opcode=%s;",mi_opcode_s);
/*
	// Print addressing mode (shift_type) if instructions has shifter-based
	// addressing mode
	if (has_note(mi,k_arm_shift))
	{
	  fprintf(f_adjmat_file,"shift_type=%d;",(int)get_note(mi,k_arm_shift));
	}
*/

        // Print call target if mi is a call instruction
        if (is_call(mi))
        {
          // Print the call target as a "target<i>" entry.
          call_target0 = get_target(mi);
          fprintf(f_adjmat_file,"target0=");
          printer->print_sym(call_target0);
          fprintf(f_adjmat_file,";");
        }

        // Print ubr/cbr target if mi is a ubr or cbr instruction
        // NOTE: Does not handle mbr.
        if (is_cbr(mi) || is_ubr(mi))
        {
          // Print the ubr/cbr target as a "target<i>" entry.
          cti_target = get_target(mi);
          fprintf(f_adjmat_file,"target%d=", count_cti_target);
          printer->print_sym(cti_target);
          fprintf(f_adjmat_file,";");
          count_cti_target++;
        }

        // Get all destination operands of current instruction (mi)
        for (OpndHandle mi_oh=dsts_start(mi); mi_oh!=dsts_end(mi); ++mi_oh)
        {

         if (!writes_memory(mi))
         {
          // Get destination operand of mi
          opnd_dst_tmp = get_dst(mi, mi_oh);

          // Print destination operand
          if (!is_null(opnd_dst_tmp) && !is_immed(opnd_dst_tmp))
          {
            fprintf(f_adjmat_file,"dst%d=",count_dst);
            printer->print_opnd(opnd_dst_tmp);
            print_operand_dt_info(f_adjmat_file, opnd_dst_tmp, local_dt_info, enable_dt);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_dst
            count_dst++;
          }
          }
          else /* for STR */
          {
           if (is_base_disp(get_dst(mi, mi_oh)))
          {
            BaseDispOpnd bdo_dst = get_dst(mi, mi_oh);
            opnd_src_tmp = bdo_dst.get_base();
          }
          else if (is_sym_disp(get_dst(mi, mi_oh)))
          {
            SymDispOpnd sdo_dst = get_dst(mi, mi_oh);
            opnd_src_tmp = sdo_dst.get_addr_sym();
          }
	  else if (is_index_sym_disp(get_dst(mi, mi_oh)))
          {
            IndexSymDispOpnd isdo_dst = get_dst(mi, mi_oh);
            opnd_src_tmp = isdo_dst.get_addr_sym();
	    Opnd opnd_src_tmp2 = isdo_dst.get_index();
          }

          // Print source operand
          if (!is_null(opnd_src_tmp) && !is_immed(opnd_src_tmp) && !is_addr_sym(opnd_src_tmp))
          {
            fprintf(f_adjmat_file,"src%d=",count_src);
            printer->print_opnd(opnd_src_tmp);
            print_operand_dt_info(f_adjmat_file, opnd_src_tmp, local_dt_info, enable_dt);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_src
            count_src++;
          }
	  // print the source operand if it is an addr_sym (for LDA)
          else if (!is_null(opnd_src_tmp) && !is_immed(opnd_src_tmp) && is_addr_sym(opnd_src_tmp))
          {
            fprintf(f_adjmat_file,"src%d=",count_src);
            printer->print_opnd(opnd_src_tmp);
            print_operand_dt_info(f_adjmat_file, opnd_src_tmp, local_dt_info, enable_dt);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_src
            count_src++;
          }


	  if (!is_null(opnd_src_tmp2) && !is_immed(opnd_src_tmp2) && !is_addr_sym(opnd_src_tmp2))
          {
            fprintf(f_adjmat_file,"src%d=",count_src);
            printer->print_opnd(opnd_src_tmp2);
            print_operand_dt_info(f_adjmat_file, opnd_src_tmp2, local_dt_info, enable_dt);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_src
            count_src++;
          }
          }
        }

        // Get all source operands of current instruction (mi)
        for (OpndHandle mi_oh=srcs_start(mi); mi_oh!=srcs_end(mi); ++mi_oh)
        {
          if (is_base_disp(get_src(mi, mi_oh)))
          {
            BaseDispOpnd bdo_src = get_src(mi, mi_oh);
            opnd_src_tmp = bdo_src.get_base();
          }
          else if (is_sym_disp(get_src(mi, mi_oh)))
          {
            SymDispOpnd sdo_src = get_src(mi, mi_oh);
            opnd_src_tmp = sdo_src.get_addr_sym();
          }
	  else if (is_index_sym_disp(get_src(mi, mi_oh)))
          {
            IndexSymDispOpnd isdo_src = get_src(mi, mi_oh);
            opnd_src_tmp = isdo_src.get_addr_sym();
	    Opnd opnd_src_tmp2 = isdo_src.get_index();
          }
          else
            opnd_src_tmp = get_src(mi, mi_oh);

          // Print source operand
          if (!is_null(opnd_src_tmp) && !is_immed(opnd_src_tmp) && !is_addr_sym(opnd_src_tmp))
          {
            fprintf(f_adjmat_file,"src%d=",count_src);
            printer->print_opnd(opnd_src_tmp);
            print_operand_dt_info(f_adjmat_file, opnd_src_tmp, local_dt_info, enable_dt);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_src
            count_src++;
          }
	  // print the source operand if it is an addr_sym (for LDA)
          else if (!is_null(opnd_src_tmp) && is_addr_sym(opnd_src_tmp))
          {
            fprintf(f_adjmat_file,"src%d=",count_src);
            printer->print_opnd(opnd_src_tmp);
            print_operand_dt_info(f_adjmat_file, opnd_src_tmp, local_dt_info, enable_dt);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_src
            count_src++;
          }
	  
	  if (!is_null(opnd_src_tmp2) && !is_immed(opnd_src_tmp2) && !is_addr_sym(opnd_src_tmp2))
          {
            fprintf(f_adjmat_file,"src%d=",count_src);
            printer->print_opnd(opnd_src_tmp2);
            print_operand_dt_info(f_adjmat_file, opnd_src_tmp2, local_dt_info, enable_dt);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_src
            count_src++;
          }
        }

        // Get all immediate integer operands of current instruction (mi)
        for (OpndHandle mi_oh=srcs_start(mi); mi_oh!=srcs_end(mi); mi_oh++)
        {
          if (is_immed_integer(get_src(mi, mi_oh)))
          {
            Opnd opnd_immint_src = get_src(mi, mi_oh);

	    // Print source operand if it is an immediate integer
	    fprintf(f_adjmat_file,"immint%d=",count_immint);
            printer->print_opnd(opnd_immint_src);
            fprintf(f_adjmat_file,";");
            //
            // Increment count_immint
            count_immint++;
          }
        }

      // Finalize current line in adjmat info file
      fprintf(f_adjmat_file,"\n");

      instr_num++;

      }  // if is_real_instruction

      //i++;
    }  // h1

    //-----------------------------------------------------------------

    // Delete names since these are temporaries
    // Delete adjmat_name
    delete adjmat_name;
    adjmat_name = NULL;

    fprintf(f_bbinstrnum_file,"%d\t%d\t%d\n",procedure_count,bb_num_itrack,instr_num);

    // The number of current basic block
    bb_num++;

    // At least one real instruction is found in the BB
    if (instr_num>0)
      bb_num_itrack++;

    // Report end of current BB
    fprintf(f_adjmat_file, "end bb;\n");

    /****************************************************************/
    /****************** end of INSIDE A CFGNODE *********************/
    /****************************************************************/

  } // end of for each CFGnode

  delete opnd_catalog;
  delete du_analyzer;
  delete liveness;
  
  //
  fprintf(f_bbcounters_file,"%s\t%d\t%d\n", cur_proc_name, procedure_count, bb_num-1);
  procedure_count++;

  fclose(f_bbcounters_file);
  fclose(f_bbinstrnum_file);

  // Report end of current procedure
  fprintf(f_adjmat_file, "end procedure;\n");

  fclose(f_adjmat_file);
}

/*** END OF bbpart.cpp */

