#pragma once
#include <stdio.h>
#include <time.h>

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

#define FILL_VALUE_FLOAT INFINITY
#define FILL_VALUE_UINT UINT_MAX

static spla_Type tfloat;
static spla_Type tuint;

static spla_Scalar fill_float;
static spla_Scalar fill_uint;

static void init_spla(void)
{
    sp(spla_Library_initialize());
    sp(spla_Library_set_default_callback());
    sp(spla_Library_set_accelerator(SPLA_ACCELERATOR_TYPE_OPENCL));

    char accel[512] = { 0 };
    sp(spla_Library_get_accelerator_info(accel, 512));
    info("accelerator: %s", accel);

    tfloat = spla_Type_FLOAT();
    tuint = spla_Type_UINT();

    sp(spla_Scalar_make(&fill_float, spla_Type_FLOAT()));
    sp(spla_Scalar_set_float(fill_float, FILL_VALUE_FLOAT));

    sp(spla_Scalar_make(&fill_uint, spla_Type_UINT()));
    sp(spla_Scalar_set_uint(fill_uint, FILL_VALUE_UINT));
}

static void exit_spla(void)
{
    spla_Library_finalize();
}

static spla_Vector make_vector(uint n, spla_Type type)
{
    spla_Vector V;
    sp(spla_Vector_make(&V, n, type));

    if (type == tfloat) {
        sp(spla_Vector_set_fill_value(V, fill_float));
    } else if (type == tuint) {
        sp(spla_Vector_set_fill_value(V, fill_uint));
    }

    sp(spla_Vector_set_format(V, SPLA_FORMAT_VECTOR_ACC_DENSE));
    return V;
}

static spla_Matrix make_matrix(uint nrows, uint ncols, spla_Type type)
{
    spla_Matrix M;
    sp(spla_Matrix_make(&M, nrows, ncols, type));

    if (type == tfloat) {
        sp(spla_Matrix_set_fill_value(M, fill_float));
    } else if (type == tuint) {
        sp(spla_Matrix_set_fill_value(M, fill_uint));
    }

    return M;
}
#endif

#define TIME(method)                                                  \
    do {                                                              \
        info("start measure");                                        \
        struct timespec start;                                        \
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);                   \
        uint64_t start_ms = start.tv_sec * 1e3 + start.tv_nsec / 1e6; \
                                                                      \
        method;                                                       \
                                                                      \
        struct timespec end;                                          \
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);                     \
        uint64_t end_ms = end.tv_sec * 1e3 + end.tv_nsec / 1e6;       \
        info("spent %lums", end_ms - start_ms);                       \
    } while (0);
