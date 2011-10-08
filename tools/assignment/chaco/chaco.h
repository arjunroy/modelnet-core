#ifndef __CHACO_H_
#define __CHACO_H_

int interface(int nvtxs, int* start,int* adjacency,int* vwgts,float* ewgts,float* x,float* y,float* z,
		          char outassignname, char outfilename, short* assignment, int architecture,int ndims_tot, int mesh_dims[], double* goal,
		          int global_method, int local_method, int rqi_flag, int vmax, int ndims, double eigtol, int seed);

#endif
