/*****************************************************************************/
/*  libCUDF - CUDF (Common Upgrade Description Format) manipulation library  */
/*  Copyright (C) 2009  Stefano Zacchiroli <zack@pps.jussieu.fr>             */
/*                                                                           */
/*  This library is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Lesser General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.  A special linking       */
/*  exception to the GNU Lesser General Public License applies to this       */
/*  library, see the COPYING file for more information.                      */
/*****************************************************************************/

// TODO should check / handle exceptions for all invoked caml_callback-s

#include <stdio.h>
#include <string.h>

#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

#include <cudf.h>
#include <cudf-variants.h>

#define Val_none	Val_int(0)
#define Some_val(v)	Field(v,0)

#define CUDF_preamble 0
#define CUDF_universe 1
#define CUDF_request 2

/** generic OCaml binding helpers */

#if 0
static int caml_list_length(value l) {
	int length = 0;

	while (l != Val_emptylist) {
		length++;
		l = Field(l, 1);
	}
	return length;
}
#endif

/** CUDF-specific binding helpers */

static int relop_val(value v) {
	switch (v) {
	case MLPVAR_Eq : return RELOP_EQ ;
	case MLPVAR_Neq : return RELOP_NEQ ;
	case MLPVAR_Geq : return RELOP_GEQ ;
	case MLPVAR_Gt : return RELOP_GT ;
	case MLPVAR_Leq : return RELOP_LEQ ;
	case MLPVAR_Lt : return RELOP_LT ;
	default :
		g_error("Internal error: unexpected variant for \"relop\": %d",
			Int_val(v));
	}
}

cudf_vpkg *vpkg_val(value ml_vpkg) {
	cudf_vpkg *vpkg;
	value ml_constr;

	vpkg = malloc(sizeof(cudf_vpkg));
	vpkg->name = strdup(String_val(Field(ml_vpkg, 0)));
	if (Field(ml_vpkg, 1) != Val_none) {	/* version constraint */
		ml_constr = Some_val(Field(ml_vpkg, 1));
		vpkg->relop = relop_val(Field(ml_constr, 0));
		vpkg->version = Int_val(Field(ml_constr, 1));
	} else {	/* no version constraint */
		vpkg->relop = 0;
		vpkg->version = -1;
	}

	return vpkg;
}

cudf_vpkglist cudf_pkg_vpkglist(cudf_package pkg, int field) {
	GList *l = NULL;
	value ml_vpkg, ml_vpkgs;
	cudf_vpkg *vpkg;
	
	ml_vpkgs = Field(pkg, field);
	while (ml_vpkgs != Val_emptylist) {
		ml_vpkg = Field(ml_vpkgs, 0);
		vpkg = vpkg_val(ml_vpkg);
		l = g_list_append(l, vpkg);
		ml_vpkgs = Field(ml_vpkgs, 1);
	}
	return l;
}

/** libCUDF binding public interface */

cudf_doc cudf_parse_from_file(char *fname) {
	static value *closure_f = NULL;
	cudf_doc doc;
	value ml_doc, ml_pkgs;
	GList *l = NULL;
	value *pkg;
  
	if (closure_f == NULL)
		closure_f = caml_named_value("parse_from_file");
	ml_doc = caml_callback(*closure_f, caml_copy_string(fname));
  
	caml_register_global_root(&doc.preamble);	/* preamble */
	if (Field(ml_doc, CUDF_preamble) != Val_none) {
		doc.has_preamble = 1;
		doc.preamble = Some_val(Field(ml_doc, CUDF_preamble));
	} else {
		doc.has_preamble = 0;
		doc.preamble = Val_none;
	}

	caml_register_global_root(&doc.request);	/* request */
	if (Field(ml_doc, CUDF_request) != Val_none) {
		doc.has_request = 1;
		doc.request = Some_val(Field(ml_doc, CUDF_request));
	} else {
		doc.has_request = 0;
		doc.request = Val_none;
	}

	ml_pkgs = Field(ml_doc, CUDF_universe);		/* packages */
	while (ml_pkgs != Val_emptylist) {
		pkg = malloc(sizeof(value));
		caml_register_global_root(pkg);
		*pkg = Field(ml_pkgs, 0);
		l = g_list_append(l, pkg);
		ml_pkgs = Field(ml_pkgs, 1);
	}
	doc.packages = l;

	return doc;
}

cudf cudf_load_from_file(char *fname) {
	static value *closure_f = NULL;
	cudf cudf;
	value ml_cudf;
  
	if (closure_f == NULL)
		closure_f = caml_named_value("load_from_file");
	ml_cudf = caml_callback(*closure_f, caml_copy_string(fname));

	caml_register_global_root(&cudf.preamble);	/* preamble */
	if (Field(ml_cudf, CUDF_preamble) != Val_none) {
		cudf.has_preamble = 1;
		cudf.preamble = Some_val(Field(ml_cudf, CUDF_preamble));
	} else {
		cudf.has_preamble = 0;
		cudf.preamble = Val_none;
	}

	caml_register_global_root(&cudf.request);	/* request */
	if (Field(ml_cudf, CUDF_request) != Val_none) {
		cudf.has_request = 1;
		cudf.request = Some_val(Field(ml_cudf, CUDF_request));
	} else {
		cudf.has_request = 0;
		cudf.request = Val_none;
	}

	caml_register_global_root(&cudf.universe);	/* universe */
	cudf.universe = Field(ml_cudf, CUDF_universe);

	return cudf;
}

int cudf_pkg_keep(cudf_package p) {
	value keep = Field(p, 6);

	switch (Int_val(keep)) {
	case MLPVAR_Keep_none : return KEEP_NONE;
	case MLPVAR_Keep_version : return KEEP_VERSION;
	case MLPVAR_Keep_package : return KEEP_PACKAGE;
	case MLPVAR_Keep_feature : return KEEP_FEATURE;
	default :
		g_error("Internal error: unexpected variant for \"keep\": %d",
			Int_val(keep));
	}
}

cudf_vpkgformula cudf_pkg_depends(cudf_package pkg) {
	GList *and_l = NULL;	/* top-level formula (CNF) */
	GList *or_l;		/* OR-ed deps */
	value ml_and;	/* iterates over OR-ed deps (which are AND-ed together) */
	value ml_or;	/* iterates over vpkg-s (which are OR-ed together) */
	cudf_vpkg *vpkg;

	ml_and = Field(pkg, 2);	/* 3rd field of "package": "depends" */
	while (ml_and != Val_emptylist) {
		ml_or = Field(ml_and, 0);
		or_l = NULL;
		while (ml_or != Val_emptylist) {
			vpkg = vpkg_val(Field(ml_or, 0));
			or_l = g_list_append(or_l, vpkg);
			ml_or = Field(ml_or, 1);
		}
		and_l = g_list_append(and_l, or_l);
		ml_and = Field(ml_and, 1);
	}

	return and_l;
}

cudf_vpkglist cudf_pkg_conflicts(cudf_package pkg) {
	return cudf_pkg_vpkglist(pkg, 3);	/* 4th pkg field: conflicts */
}

cudf_vpkglist cudf_pkg_provides(cudf_package pkg) {
	return cudf_pkg_vpkglist(pkg, 4);	/* 5th pkg field: conflicts */
}

char *cudf_pkg_property(cudf_package pkg, const char *prop) {
	static value *closure_f = NULL;
	value prop_val;
  
	if (closure_f == NULL)
		closure_f = caml_named_value("lookup_package_property");
	prop_val = caml_callback2_exn(*closure_f, pkg, caml_copy_string(prop));
	return Is_exception_result(prop_val) ? NULL :
		strdup(String_val(prop_val));
}

char *cudf_req_property(cudf_request req, const char *prop) {
	static value *closure_f = NULL;
	value prop_val;
  
	if (closure_f == NULL)
		closure_f = caml_named_value("lookup_request_property");
	prop_val = caml_callback2_exn(*closure_f, req, caml_copy_string(prop));
	return Is_exception_result(prop_val) ? NULL :
		strdup(String_val(prop_val));
}


/** Universe management */

void cudf_load_universe(cudf_universe *univ, GList *packages) {
	static value *closure_f = NULL;
	value ml_pkgs = Val_emptylist;
	value cons;
	GList *l = packages;

	while (l != NULL) {
		cons = caml_alloc(2, 0);
		Store_field(cons, 0, * (cudf_package *) g_list_nth_data(l, 0));
		Store_field(cons, 1, ml_pkgs);
		ml_pkgs = cons;
		l = g_list_next(l);
	}

	if (closure_f == NULL)
		closure_f = caml_named_value("load_universe");
	caml_register_global_root(univ);
	*univ = caml_callback(*closure_f, ml_pkgs);
}

int cudf_universe_size(cudf_universe univ) {
	static value *closure_f = NULL;

	if (closure_f == NULL)
		closure_f = caml_named_value("universe_size");
	return Int_val(caml_callback(*closure_f, univ));
}

int cudf_installed_size(cudf_universe univ) {
	static value *closure_f = NULL;

	if (closure_f == NULL)
		closure_f = caml_named_value("installed_size");
	return Int_val(caml_callback(*closure_f, univ));
}

int cudf_is_consistent(cudf_universe univ) {
	static value *closure_f = NULL;

	if (closure_f == NULL)
		closure_f = caml_named_value("is_consistent");
	return Bool_val(Field(caml_callback(*closure_f, univ), 0));
}

int cudf_is_solution(cudf cudf, cudf_universe solution) {
	static value *closure_f = NULL;
	value ml_cudf;

	if (closure_f == NULL)
		closure_f = caml_named_value("is_solution");
	if (! cudf.has_request)
		g_error("Given CUDF has no request: cannot compare it with a solution.");
	ml_cudf = caml_alloc(3, 0);
	Store_field(ml_cudf, CUDF_preamble, cudf.preamble);
	Store_field(ml_cudf, CUDF_universe, cudf.universe);
	Store_field(ml_cudf, CUDF_request, cudf.request);

	return Bool_val(Field(caml_callback2(*closure_f, ml_cudf, solution), 0));
}

/** Memory management.
    free-like functions to free binding-specific data structures */

void cudf_free_doc(cudf_doc doc) {
	GList *l;

	caml_remove_global_root(&doc.preamble);
	caml_remove_global_root(&doc.request);
	l = doc.packages;
	while (l != NULL) {
		caml_remove_global_root(g_list_nth_data(l, 0));
		l = g_list_next(l);
	}
	g_list_free(l);
}

void cudf_free_cudf(cudf cudf) {
	caml_remove_global_root(&cudf.preamble);
	caml_remove_global_root(&cudf.request);
	caml_remove_global_root(&cudf.universe);
}

void cudf_free_universe(cudf_universe *univ) {
	caml_remove_global_root(univ);
}

void cudf_free_vpkglist(cudf_vpkglist l) {
	cudf_vpkg *vpkg;

	vpkg = (cudf_vpkg *) l;
	while (vpkg != NULL) {
		if (vpkg->name != NULL)
			free(vpkg->name);
		vpkg = (cudf_vpkg *) g_list_next(vpkg);
	}
	g_list_free(l);
}

void cudf_free_vpkgformula(cudf_vpkgformula fmla) {
	GList *l = fmla;

	while (l != NULL) {
		cudf_free_vpkglist(g_list_nth_data(l, 0));
		l = g_list_next(l);
	}
	g_list_free(fmla);
}

