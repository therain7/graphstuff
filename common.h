#pragma once

#define info(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define err(fmt, ...)                                                  \
    do {                                                               \
        fprintf(stderr, "%s:%d: error: " fmt "\n", __FILE__, __LINE__, \
                ##__VA_ARGS__);                                        \
        exit(EXIT_FAILURE);                                            \
    } while (0);

#ifdef GRAPHBLAS_H
#define GRB_CATCH(r) err("GraphBLAS error: %d", r);
#define gr(...) GRB_TRY(__VA_ARGS__)
#endif

#ifdef LAGRAPH_H
static char msg[LAGRAPH_MSG_LEN];

#define LAGRAPH_CATCH(r) err("LAGraph error: %d. %s", r, msg);
#define la(...) LAGRAPH_TRY(__VA_ARGS__)
#endif

#ifdef SPLA_SPLA_C_H
#define sp(method)                                                   \
    do {                                                             \
        spla_Status r = method;                                      \
        if (r) {                                                     \
            err("spla error: (%d): file: %s, line: %d", r, __FILE__, \
                __LINE__);                                           \
        }                                                            \
    } while (0);

static spla_Scalar fill;

static void init_spla(void)
{
    sp(spla_Library_initialize());
    sp(spla_Library_set_default_callback());
    sp(spla_Library_set_accelerator(SPLA_ACCELERATOR_TYPE_OPENCL));

    char accel[512] = { 0 };
    sp(spla_Library_get_accelerator_info(accel, 512));
    info("accelerator: %s", accel);

    sp(spla_Scalar_make(&fill, spla_Type_FLOAT()));
    sp(spla_Scalar_set_float(fill, INFINITY));
}

static void exit_spla(void)
{
    spla_Library_finalize();
}

static spla_Vector make_vector(uint n)
{
    spla_Vector V;
    sp(spla_Vector_make(&V, n, spla_Type_FLOAT()));
    sp(spla_Vector_set_fill_value(V, fill));
    sp(spla_Vector_set_format(V, SPLA_FORMAT_VECTOR_ACC_DENSE));
    return V;
}

static spla_Matrix make_matrix(uint nrows, uint ncols)
{
    spla_Matrix M;
    sp(spla_Matrix_make(&M, nrows, ncols, spla_Type_FLOAT()));
    sp(spla_Matrix_set_fill_value(M, fill));
    return M;
}
#endif

#if defined GRAPHBLAS_H && defined SPLA_SPLA_C_H
static void grb_to_spla(spla_Matrix out, GrB_Matrix A)
{
    GrB_Index nvals;
    gr(GrB_Matrix_nvals(&nvals, A));

    GrB_Index *Ai = malloc(nvals * sizeof(*Ai));
    GrB_Index *Aj = malloc(nvals * sizeof(*Aj));
    double *vals = malloc(nvals * sizeof(*vals));
    if (!Ai || !Aj || !vals) {
        err("failed to allocate memory");
    }

    gr(GrB_Matrix_extractTuples_FP64(Ai, Aj, vals, &nvals, A));
    for (GrB_Index k = 0; k < nvals; k++) {
        sp(spla_Matrix_set_float(out, Ai[k], Aj[k], vals[k]))
    }

    free(vals);
    free(Aj);
    free(Ai);
}
#endif
