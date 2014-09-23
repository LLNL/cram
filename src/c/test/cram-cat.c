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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "cram_file.h"


int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: cram-cat <cramfile>\n");
    fprintf(stderr, "  Prints out the entire contents of a cram file "
            "as cram info would.\n");
    exit(1);
  }
  const char *filename = argv[1];

  cram_file_t file;
  if (!cram_file_open(filename, &file)) {
    printf("failed to map with errno %d: %s\n", errno, strerror(errno));
    exit(1);
  }

  printf("Name:%25s\n", filename);
  cram_file_cat(&file);
}
