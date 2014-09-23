//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Cram.
// Written by Todd Gamblin, tgamblin@llnl.gov, All rights reserved.
// LLNL-CODE-661100
//
// For details, see https://github.com/scalability-llnl/cram.
// Please also see the LICENSE file for our notice and the LGPL.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License (as published by
// the Free Software Foundation) version 2.1 dated February 1999.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
// conditions of the GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//////////////////////////////////////////////////////////////////////////////

// These are defined in the cram library -- they're set when MPI_INIT is called.
extern int cram_argc;
extern const char **cram_argv;


///
/// Cram version of iargc intrinsic supported by many (nearly all?)
/// Fortran 77/90 compilers.  This just returns the argc set by Cram
/// in MPI_INIT.
///
static inline int cram_iargc() {
    return cram_argc;
}


///
/// Cram version of getarg intrinsic supported by many (nearly all?)
/// Fortran 77/90 compilers.  This version copies cram arguments into
/// variables it's called on.
///
static inline void cram_getarg(int *i, char *var, int var_len) {
    int c = 0;

    if (*i > 0 && *i < cram_argc) {
        // copy value of argument into destination.
        for (; c < var_len && cram_argv[*i][c]; c++) {
            var[c] = cram_argv[*i][c];
        }
    }

    // fill the rest with null chars because Fortran.
    for (; c < var_len; c++) {
        var[c] = '\0';
    }
}


///
/// This macro defines versions of iargc and getarg for each compiler.
/// Each definition just calls the functions above.
///
/// The names here are temporary; we name them after compilers, but the
/// intrinsics have symbol names like getarg@@XLF_1.0, which a C compiler
/// won't emit.  So we write the names out with various suffixes, and we use
/// objcopy afterwards to give the symbols the names we need to override
/// compiler intrinsics.
///
#define define_for_suffix(s)                                                  \
    int iargc_##s() { return cram_iargc(); }                                  \
    void getarg_##s(int *i, char *var, int var_len) {                         \
        cram_getarg(i, var, var_len);                                         \
    }

define_for_suffix(gnu)
define_for_suffix(xl)
