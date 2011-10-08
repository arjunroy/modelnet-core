/* This software was developed by Bruce Hendrickson and Robert Leland   *
 * at Sandia National Laboratories under US Department of Energy        *
 * contract DE-AC04-76DP00789 and is copyrighted by Sandia Corporation. */

#include <stdio.h>
#include <math.h>
#include "params.h"
#include "defs.h"
#include "structs.h"

void      coarsen(graph, nvtxs, nedges, using_vwgts, using_ewgts, term_wgts,
		            igeom, coords, yvecs, ndims, solver_flag, vmax, eigtol,
		            nstep, step, give_up)
/* Coarsen until nvtxs <= vmax, compute and uncoarsen. */
struct vtx_data **graph;	/* array of vtx data for graph */
int       nvtxs;		/* number of vertices in graph */
int       nedges;		/* number of edges in graph */
int       using_vwgts;		/* are vertices weights being used? */
int       using_ewgts;		/* are edge weights being used? */
float    *term_wgts[];		/* terminal weights */
int       igeom;		/* dimension for geometric information */
float   **coords;		/* coordinates for vertices */
double  **yvecs;		/* eigenvectors returned */
int       ndims;		/* number of eigenvectors to calculate */
int       solver_flag;		/* which eigensolver to use */
int       vmax;			/* largest subgraph to stop coarsening */
double    eigtol;		/* tolerence in eigen calculation */
int       nstep;		/* number of coarsenings between RQI steps */
int       step;			/* current step number */
int       give_up;		/* has coarsening bogged down? */
{
    extern FILE *Output_File;	/* output file or null */
    extern int DEBUG_COARSEN;	/* debug flag for coarsening */
    extern int PERTURB;		/* was matrix perturbed in Lanczos? */
    extern double COARSEN_RATIO_MIN;	/* min vtx reduction for coarsening */
    extern int COARSEN_VWGTS;	/* use vertex weights while coarsening? */
    extern int COARSEN_EWGTS;	/* use edge weights while coarsening? */
    extern double refine_time;	/* time for RQI/Symmlq iterative refinement */
    struct vtx_data **cgraph;	/* array of vtx data for coarsened graph */
    struct orthlink *orthlist;	/* list of lower evecs to suppress */
    struct orthlink *newlink;	/* lower evec to suppress */
    double   *cyvecs[MAXDIMS + 1];	/* eigenvectors for subgraph */
    double    evals[MAXDIMS + 1];	/* eigenvalues returned */
    double    goal[MAXSETS];	/* needed for convergence mode = 1 */
    double   *r1, *r2, *work;	/* space needed by symmlq/RQI */
    double   *v, *w, *x, *y;	/* space needed by symmlq/RQI */
    double   *gvec;		/* rhs vector in extended eigenproblem */
    double    evalest;		/* eigenvalue estimate returned by RQI */
    double    maxdeg;		/* maximum weighted degree of a vertex */
    float   **ccoords;		/* coordinates for coarsened graph */
    float    *cterm_wgts[MAXSETS];	/* coarse graph terminal weights */
    float    *new_term_wgts[MAXSETS];	/* terminal weights for Bui's method*/
    float   **real_term_wgts;	/* one of the above */
    float    *twptr;		/* loops through term_wgts */
    float    *twptr_save;	/* copy of twptr */
    float    *ctwptr;		/* loops through cterm_wgts */
    double   *vwsqrt = NULL;	/* square root of vertex weights */
    double    norm, alpha;	/* values used for orthogonalization */
    double    initshift;	/* initial shift for RQI */
    double    total_vwgt;	/* sum of all the vertex weights */
    double    w1, w2;		/* weights of two sets */
    double    sigma;		/* norm of rhs in extended eigenproblem */
    double    term_tot;		/* sum of all terminal weights */
    short    *space;		/* room for assignment in Lanczos */
    int      *morespace;	/* room for assignment in Lanczos */
    int      *v2cv;		/* mapping from vertices to coarse vtxs */
    int       vwgt_max;		/* largest vertex weight */
    int       oldperturb;	/* saves PERTURB value */
    int       cnvtxs;		/* number of vertices in coarsened graph */
    int       cnedges;		/* number of edges in coarsened graph */
    int       nextstep;		/* next step in RQI test */
    int       nsets;		/* number of sets being created */
    int       i, j;		/* loop counters */
    double    time;		/* time marker */
    int       sfree();
    double   *smalloc(), dot(), normalize(), find_maxdeg(), seconds();
    struct orthlink *makeorthlnk();
    void      makevwsqrt(), eigensolve(), coarsen1(), orthogvec(), rqi_ext();
    void      interpolate(), orthog1(), rqi(), scadd(), free_graph();

    if (DEBUG_COARSEN > 0) {
	printf("<Entering coarsen, step=%d, nvtxs=%d, nedges=%d, vmax=%d>\n",
	       step, nvtxs, nedges, vmax);
    }

    nsets = 1 << ndims;

    /* Is problem small enough to solve? */
    if (nvtxs <= vmax || give_up) {
	if (using_vwgts) {
	    vwsqrt = (double *) smalloc((unsigned) (nvtxs + 1) * sizeof(double));
	    makevwsqrt(vwsqrt, graph, nvtxs);
	}
	else
	    vwsqrt = NULL;
	maxdeg = find_maxdeg(graph, nvtxs, using_ewgts, (float *) NULL);

	if (using_vwgts) {
	    vwgt_max = 0;
	    total_vwgt = 0;
	    for (i = 1; i <= nvtxs; i++) {
		if (graph[i]->vwgt > vwgt_max)
		    vwgt_max = graph[i]->vwgt;
		total_vwgt += graph[i]->vwgt;
	    }
	}
	else {
	    vwgt_max = 1;
	    total_vwgt = nvtxs;
	}
	for (i = 0; i < nsets; i++)
	    goal[i] = total_vwgt / nsets;

	space = (short *) smalloc((unsigned) (nvtxs + 1) * sizeof(short));

	/* If not coarsening ewgts, then need care with term_wgts. */
	if (!using_ewgts && term_wgts[1] != NULL && step != 0) {
	    twptr = (float *) smalloc((unsigned)
		(nvtxs + 1) * (nsets - 1) * sizeof(float));
	    twptr_save = twptr;
	    for (j = 1; j < nsets; j++) {
	        new_term_wgts[j] = twptr;
	        twptr += nvtxs + 1;
	    }

	    for (j = 1; j < nsets; j++) {
	        twptr = term_wgts[j];
	        ctwptr = new_term_wgts[j];
	        for (i = 1; i <= nvtxs; i++) {
		    if (twptr[i] > .5) ctwptr[i] = 1;
		    else if (twptr[i] < -.5) ctwptr[i] = -1;
		    else ctwptr[i] = 0;
		}
	    }
	    real_term_wgts = new_term_wgts;
	}
	else {
	    real_term_wgts = term_wgts;
	    new_term_wgts[1] = NULL;
	}

	eigensolve(graph, nvtxs, nedges, maxdeg, vwgt_max, vwsqrt,
		   using_vwgts, using_ewgts, real_term_wgts, igeom, coords,
		   yvecs, evals, 0, space, goal,
		   solver_flag, FALSE, 0, ndims, 3, eigtol);

	if (real_term_wgts != term_wgts && new_term_wgts[1] != NULL) {
	    sfree((char *) real_term_wgts[1]);
	}
	sfree((char *) space);
	if (vwsqrt != NULL)
	    sfree((char *) vwsqrt);
	return;
    }

    /* Otherwise I have to coarsen. */

    if (coords != NULL) {
	ccoords = (float **) smalloc(igeom * sizeof(float *));
    }
    else {
	ccoords = NULL;
    }
    coarsen1(graph, nvtxs, nedges, &cgraph, &cnvtxs, &cnedges,
	     &v2cv, igeom, coords, ccoords, using_ewgts);

    /* If coarsening isn't working very well, give up and partition. */
    give_up = FALSE;
    if (nvtxs * COARSEN_RATIO_MIN < cnvtxs && cnvtxs > vmax ) {
	printf("WARNING: Coarsening not making enough progress, nvtxs = %d, cnvtxs = %d.\n",
	    nvtxs, cnvtxs);
	printf("         Recursive coarsening being stopped prematurely.\n");
	if (Output_File != NULL) {
	    fprintf(Output_File,
		"WARNING: Coarsening not making enough progress, nvtxs = %d, cnvtxs = %d.\n",
	        nvtxs, cnvtxs);
	    fprintf(Output_File,
		"         Recursive coarsening being stopped prematurely.\n");
	}
	give_up = TRUE;
    }

    /* Create space for subgraph yvecs. */
    for (i = 1; i <= ndims; i++) {
	cyvecs[i] = (double *) smalloc((unsigned) (cnvtxs + 1) * sizeof(double));
    }

    /* Make coarse version of terminal weights. */
    if (term_wgts[1] != NULL) {
	twptr = (float *) smalloc((unsigned)
	    (cnvtxs + 1) * (nsets - 1) * sizeof(float));
	twptr_save = twptr;
	for (i = (cnvtxs + 1) * (nsets - 1); i ; i--) {
	    *twptr++ = 0;
	}
	twptr = twptr_save;
	for (j = 1; j < nsets; j++) {
	    cterm_wgts[j] = twptr;
	    twptr += cnvtxs + 1;
	}
	for (j = 1; j < nsets; j++) {
	    ctwptr = cterm_wgts[j];
	    twptr = term_wgts[j];
	    for (i = 1; i < nvtxs; i++){
	        ctwptr[v2cv[i]] += twptr[i];
	    }
	}
    }
    else {
	cterm_wgts[1] = NULL;
    }

    /* Now recurse on coarse subgraph. */
    nextstep = step + 1;
    coarsen(cgraph, cnvtxs, cnedges, COARSEN_VWGTS, COARSEN_EWGTS, cterm_wgts,
	    igeom, ccoords, cyvecs, ndims, solver_flag, vmax, eigtol,
	    nstep, nextstep, give_up);

    interpolate(yvecs, cyvecs, ndims, graph, nvtxs, v2cv, using_ewgts);

    sfree((char *) cterm_wgts[1]);
    sfree((char *) v2cv);

    /* I need to do Rayleigh Quotient Iteration each nstep stages. */
    time = seconds();
    if (!(step % nstep)) {
	oldperturb = PERTURB;
	PERTURB = FALSE;
	/* Should I do some orthogonalization here against vwsqrt? */
	if (using_vwgts) {
	    vwsqrt = (double *) smalloc((unsigned) (nvtxs + 1) * sizeof(double));
	    makevwsqrt(vwsqrt, graph, nvtxs);

	    for (i = 1; i <= ndims; i++)
		orthogvec(yvecs[i], 1, nvtxs, vwsqrt);
	}
	else
	    for (i = 1; i <= ndims; i++)
		orthog1(yvecs[i], 1, nvtxs);

	/* Allocate space that will be needed in RQI. */
	r1 = (double *) smalloc((unsigned) 7 * (nvtxs + 1) * sizeof(double));
	r2 = &r1[nvtxs + 1];
	v = &r1[2 * (nvtxs + 1)];
	w = &r1[3 * (nvtxs + 1)];
	x = &r1[4 * (nvtxs + 1)];
	y = &r1[5 * (nvtxs + 1)];
	work = &r1[6 * (nvtxs + 1)];

	if (using_vwgts) {
	    vwgt_max = 0;
	    total_vwgt = 0;
	    for (i = 1; i <= nvtxs; i++) {
		if (graph[i]->vwgt > vwgt_max)
		    vwgt_max = graph[i]->vwgt;
		total_vwgt += graph[i]->vwgt;
	    }
	}
	else {
	    vwgt_max = 1;
	    total_vwgt = nvtxs;
	}
	for (i = 0; i < nsets; i++)
	    goal[i] = total_vwgt / nsets;

	space = (short *) smalloc((unsigned) (nvtxs + 1) * sizeof(short));
	morespace = (int *) smalloc((unsigned) (nvtxs) * sizeof(int));

	initshift = 0;
	orthlist = NULL;
	for (i = 1; i < ndims; i++) {
	    normalize(yvecs[i], 1, nvtxs);
	    rqi(graph, yvecs, i, nvtxs, r1, r2, v, w, x, y, work,
		eigtol, initshift, &evalest, vwsqrt, orthlist,
		0, nsets, space, morespace, 3, goal, vwgt_max, ndims);

	    /* Now orthogonalize higher yvecs against this one. */
	    norm = dot(yvecs[i], 1, nvtxs, yvecs[i]);
	    for (j = i + 1; j <= ndims; j++) {
		alpha = -dot(yvecs[j], 1, nvtxs, yvecs[i]) / norm;
		scadd(yvecs[j], 1, nvtxs, alpha, yvecs[i]);
	    }

	    /* Now prepare for next pass through loop. */
	    initshift = evalest;
	    newlink = makeorthlnk();
	    newlink->vec = yvecs[i];
	    newlink->pntr = orthlist;
	    orthlist = newlink;

	}
	normalize(yvecs[ndims], 1, nvtxs);

	if (term_wgts[1] != NULL && ndims == 1) {
	    /* Solve extended eigen problem */

	    /* If not coarsening ewgts, then need care with term_wgts. */
	    if (!using_ewgts && term_wgts[1] != NULL && step != 0) {
	        twptr = (float *) smalloc((unsigned)
		    (nvtxs + 1) * (nsets - 1) * sizeof(float));
	        twptr_save = twptr;
	        for (j = 1; j < nsets; j++) {
	            new_term_wgts[j] = twptr;
	            twptr += nvtxs + 1;
	        }

	        for (j = 1; j < nsets; j++) {
	            twptr = term_wgts[j];
	            ctwptr = new_term_wgts[j];
	            for (i = 1; i <= nvtxs; i++) {
		        if (twptr[i] > .5) ctwptr[i] = 1;
		        else if (twptr[i] < -.5) ctwptr[i] = -1;
		        else ctwptr[i] = 0;
		    }
	        }
	        real_term_wgts = new_term_wgts;
	    }
	    else {
	        real_term_wgts = term_wgts;
	        new_term_wgts[1] = NULL;
	    }

	    /* Following only works for bisection. */
	    w1 = goal[0];
	    w2 = goal[1];
	    sigma = sqrt(4*w1*w2/(w1+w2));
	    gvec = (double *) smalloc((unsigned) (nvtxs+1)*sizeof(double));
	    term_tot = sigma;	/* Avoids lint warning for now. */
	    term_tot = 0;
	    for (j=1; j<=nvtxs; j++) term_tot += (real_term_wgts[1])[j];
	    term_tot /= (w1+w2);
	    if (using_vwgts) {
	        for (j=1; j<=nvtxs; j++) {
		    gvec[j] = (real_term_wgts[1])[j]/graph[j]->vwgt - term_tot;
		}
	    }
	    else {
	        for (j=1; j<=nvtxs; j++) {
		    gvec[j] = (real_term_wgts[1])[j] - term_tot;
		}
	    }

	    rqi_ext();

	    sfree((char *) gvec);
	    if (real_term_wgts != term_wgts && new_term_wgts[1] != NULL) {
		sfree((char *) new_term_wgts[1]);
	    }
	}
	else {
	    rqi(graph, yvecs, ndims, nvtxs, r1, r2, v, w, x, y, work,
	        eigtol, initshift, &evalest, vwsqrt, orthlist,
	        0, nsets, space, morespace, 3, goal, vwgt_max, ndims);
	}
	refine_time += seconds() - time;

	/* Free the space allocated for RQI. */
	sfree((char *) morespace);
	sfree((char *) space);
	while (orthlist != NULL) {
	    newlink = orthlist->pntr;
	    sfree((char *) orthlist);
	    orthlist = newlink;
	}
	sfree((char *) r1);
	if (vwsqrt != NULL)
	    sfree((char *) vwsqrt);
	PERTURB = oldperturb;
    }
    if (DEBUG_COARSEN > 0) {
	printf(" Leaving coarsen, step=%d\n", step);
    }

    /* Free the space that was allocated. */
    if (ccoords != NULL) {
	for (i = 0; i < igeom; i++)
	    sfree((char *) ccoords[i]);
	sfree((char *) ccoords);
    }
    for (i = ndims; i > 0; i--)
	sfree((char *) cyvecs[i]);
    free_graph(cgraph);
}


struct edgeinfo
{
  int source;
  int target;
  float weight;
};

int compareEdges(const void* a, const void* b)
{
  if (((const struct edgeinfo*)a)->weight < ((const struct edgeinfo*)b)->weight)
    return -1;
  else if (((const struct edgeinfo*)a)->weight > ((const struct edgeinfo*)b)->weight)
    return 1;
  else 
    return 0;
}

int compareInts(const void* a, const void* b)
{
  return *(const int*)a - *(const int*)b;
}

int consolidateNumbering(int* numbering, int size)
{
  int* mapping;
  int maxNum = 0;
  int next = 0;
  int i;
  for (i=0; i<size;i++)
    maxNum = numbering[i] > maxNum ? numbering[i] : maxNum;
  mapping = (int*)smalloc(size*sizeof(int));
  for (i=0; i<size; i++)
    mapping[i] = -1;
  for (i=0; i<size;i++)
  {
    if (mapping[numbering[i]] == -1)
      mapping[numbering[i]] = next++;
    numbering[i] = mapping[numbering[i]];
  }
  free(mapping);
  return next;
}

void bottomCoarsen(struct vtx_data** graph, int nvtxs, int nedges, struct vtx_data** cgraph, int* cnvtxs, int* cnedges, int* v2cv)
{
  struct vtx_data* coarseGraph;
  struct edgeinfo* alledges = (struct edgeinfo*)smalloc(nedges*sizeof(struct edgeinfo));
  struct edgeinfo* edge;
  int* coarseEdgeCounts;
  int v, e, cv, index = 0;
  int nextCoarseVertex = 0;
  int merges = 0;
  

  for (v = 0; v < nvtxs; v++)
  {
    for (e = 0; e < graph[v]->nedges; e++, index++)
    {
      alledges[index].source = v;
      alledges[index].target = graph[v]->edges[e];
      alledges[index].weight = graph[v]->ewgts[e];
    }
  }
  memset(v2cv, -1, nvtxs*sizeof(int));
  qsort(alledges, nedges, sizeof(struct edgeinfo), compareEdges);
  for (index = nedges; index >0; --index)
  {
    edge = &alledges[index-1];
    if (v2cv[edge->source] != -1 && v2cv[edge->target] != -1)
    {
      if (v2cv[edge->source] == v2cv[edge->target])
	continue;
      for (v = 0; v < nvtxs; v++)
	if (v2cv[v] == v2cv[edge->source])
	  v2cv[v] = v2cv[edge->target];
    }
    else if (v2cv[edge->source] != -1)
      v2cv[edge->target] = v2cv[edge->source];
    else if (v2cv[edge->target] != -1)
      v2cv[edge->source] = v2cv[edge->target];
    else
      v2cv[edge->source] = v2cv[edge->target] = nextCoarseVertex++;
    merges++;
  }
  for (v = 0; v < nvtxs; v++)
    if (v2cv[v] == -1)
      v2cv[v] = nextCoarseVertex++;
  *cnvtxs = consolidateNumbering(v2cv, nvtxs);

  coarseGraph = (struct vtx_data*)smalloc((*cnvtxs)*sizeof(struct vtx_data*));
  coarseEdgeCounts = (int*)smalloc((*cnvtxs)*sizeof(int));
  memset(coarseEdgeCounts, 0, (*cnvtxs)*sizeof(int));
  for (index = 0; index < nedges; index++)
  {
    edge = *alledges[index];
    if (v2cv[edge->source] != v2cv[edge->target])
      coarseEdgeCounts[v2cv[edge->source]]++;
  }
  for (cv = 0; cv < *cnvtxs; cv++)
  {
    coarseGraph[cv]->edges = (int*)smalloc(coarseEdgeCounts[cv]*sizeof(int));
    coarseGraph[cv]->ewgts = (float*)smalloc(coarseEdgeCounts[cv]*sizeof(float));
    coarseGraph[cv]->vwgt = 0;
    coarseGraph[cv]->nedges = 0;
  }
}
