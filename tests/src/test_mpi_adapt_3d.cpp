/*  Copyright (C) 2010 Imperial College London and others.
 *
 *  Please see the AUTHORS file in the main source directory for a
 *  full list of copyright holders.
 *
 *  Gerard Gorman
 *  Applied Modelling and Computation Group
 *  Department of Earth Science and Engineering
 *  Imperial College London
 *
 *  g.gorman@imperial.ac.uk
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following
 *  disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 *  BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 *  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *  SUCH DAMAGE.
 */

#include <iostream>
#include <vector>
#include <unistd.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <mpi.h>

#include "Mesh.h"
#include "VTKTools.h"
#include "MetricField.h"

#include "Coarsen.h"
#include "Refine.h"
#include "Smooth.h"

int main(int argc, char **argv){
  int required_thread_support=MPI_THREAD_SINGLE;
  int provided_thread_support;
  MPI_Init_thread(&argc, &argv, required_thread_support, &provided_thread_support);
  assert(required_thread_support==provided_thread_support);
  
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  Mesh<double> *mesh=VTKTools<double>::import_vtu("../data/box10x10x10.vtu");
  mesh->create_boundary();

  MetricField<double, 3> metric_field(*mesh);

  size_t NNodes = mesh->get_number_nodes();
  size_t NElements = mesh->get_number_elements();

  for(size_t i=0;i<NNodes;i++){
    double hx=0.025 + 0.09*mesh->get_coords(i)[0];
    double hy=0.025 + 0.09*mesh->get_coords(i)[1];
    double hz=0.025 + 0.09*mesh->get_coords(i)[2];
    double m[] =
      {1.0/pow(hx, 2), 0.0,            0.0,
       0.0,            1.0/pow(hy, 2), 0.0,
       0.0,            0.0,            1.0/pow(hz, 2)};
    metric_field.set_metric(m, i);
  }
  metric_field.apply_nelements(NElements);
  metric_field.update_mesh();

  double qmean = mesh->get_qmean();
  double qmin = mesh->get_qmin();
  
  if(rank==0) std::cout<<"Initial quality:\n"
                <<"Quality mean:  "<<qmean<<std::endl
                <<"Quality min:   "<<qmin<<std::endl;
  VTKTools<double>::export_vtu("../data/test_mpi_adapt_3d-initial", mesh);

  // See Eqn 7; X Li et al, Comp Methods Appl Mech Engrg 194 (2005) 4915-4950
  double L_up = 1.0;
  double L_low = L_up/2;

  Coarsen<double, 3> coarsen(*mesh);
  coarsen.coarsen(L_low, L_up);
  
  Smooth<double, 3> smooth(*mesh);
  
  double L_max = mesh->maximal_edge_length();
  double alpha = 0.95; //sqrt(2.0)*0.5;
  Refine<double, 3> refine(*mesh);
  for(size_t i=0;i<20;i++){
    double L_ref = std::max(alpha*L_max, L_up);
    
    refine.refine(L_ref);
    coarsen.coarsen(L_low, L_ref);

    L_max = mesh->maximal_edge_length();
    if(L_max<L_up)
      break;
  }

  mesh->defragment();
  
  smooth.smart_laplacian(10);
  smooth.optimisation_linf(10);

  VTKTools<double>::export_vtu("../data/test_mpi_adapt_3d", mesh);
  
  delete mesh;
  
  if(rank==0)
    std::cout<<"pass"<<std::endl;
  
  MPI_Finalize();

  return 0;
}
