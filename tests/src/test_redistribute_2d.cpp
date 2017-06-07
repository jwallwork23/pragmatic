#include <iostream>
#include <vector>
#include <unistd.h>

#include <mpi.h>

#include "Mesh.h"
#ifdef HAVE_VTK
#include "VTKTools.h"
#endif
#include "MetricField.h"

#include "Coarsen.h"
#include "Refine.h"
#include "Smooth.h"
#include "Swapping.h"



int main(int argc, char **argv)
{
    int rank=0, num_processes=1;
    int required_thread_support=MPI_THREAD_SINGLE;
    int provided_thread_support;
    MPI_Init_thread(&argc, &argv, required_thread_support, &provided_thread_support);
    assert(required_thread_support==provided_thread_support);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);

//    if (num_processes==1) 
//        return 0;

    bool verbose = false;
    if(argc>1) {
        verbose = std::string(argv[1])=="-v";
    }

//    int imax = atoi(argv[1]);

#ifdef HAVE_VTK
    Mesh<double> *mesh=VTKTools<double>::import_vtu("../data/box10x10.vtu");
    mesh->create_boundary();

    MetricField<double,2> metric_field(*mesh);

    size_t NNodes = mesh->get_number_nodes();

    double m[3] = {0};
    for(size_t i=0; i<NNodes; i++) {
        double x = mesh->get_coords(i)[0];
        double h = 0.3*fabs(1-exp(-fabs(x-0.5))) + 0.003;
        double lbd = 1/(h*h);
        double lmax = 1/(0.3*0.3);
        m[0] = lbd;
        m[1] = lmax;
        m[2] = lmax;
        metric_field.set_metric(m, i);
    }
    metric_field.update_mesh();

    if(verbose) {
        std::cout<<"Initial quality:\n";
        mesh->verify();
    }

    mesh->redistribute_halo(1);


    double L_up = sqrt(2.0);
    double L_low = L_up*0.5;

    Coarsen<double, 2> coarsen(*mesh);
    Smooth<double, 2> smooth(*mesh);
    Refine<double, 2> refine(*mesh);
    Swapping<double, 2> swapping(*mesh);

    double L_max = mesh->maximal_edge_length();

    double alpha = sqrt(2.0)/2.0;
    size_t i=0;
    for(i=0; i<15; i++) {
//        printf("DEBUG  ite adapt: %d\n", i);
        double L_ref = std::max(alpha*L_max, L_up);

        coarsen.coarsen(L_low, L_ref, false);
        swapping.swap(0.7);
        refine.refine(L_ref);

        L_max = mesh->maximal_edge_length();

//        if ( i%5==0 ) 
//            mesh->redistribute_halo();

    }

    mesh->defragment();

    smooth.smart_laplacian(20);
    smooth.optimisation_linf(20);


//    char name[256];
//    sprintf(name, "../data/test_redistribute_2d.%d", i);
//    VTKTools<double>::export_vtu(name, mesh);

    VTKTools<double>::export_vtu("../data/test_redistribute_2d", mesh);
    
    if (mesh->verify())
        std::cout<<"pass"<<std::endl;



#else
    std::cerr<<"Pragmatic was configured without VTK"<<std::endl;
#endif

    MPI_Finalize();

    return 0;

}