#include <LAGraph.h>
#include <GraphBLAS.h>
#include <spla.h>
#include <common.h>

#define SWITCH(M, method)            \
    do {                             \
        spla_Matrix c##M = M;        \
        M = M == M##0 ? M##1 : M##0; \
        method;                      \
    } while (0)

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
        sp(spla_Matrix_set_uint(out, Ai[k], Aj[k], (uint)vals[k]));
    }

    free(vals);
    free(Aj);
    free(Ai);
}

__attribute__((unused)) static void print_matrix(spla_Matrix A, char *name)
{
    spla_MemView mkeys1, mkeys2, mvals;
    sp(spla_Matrix_read(A, &mkeys1, &mkeys2, &mvals));

    uint *keys1, *keys2, *vals;
    sp(spla_MemView_get_buffer(mkeys1, (void **)&keys1));
    sp(spla_MemView_get_buffer(mkeys2, (void **)&keys2));
    sp(spla_MemView_get_buffer(mvals, (void **)&vals));

    size_t n;
    sp(spla_MemView_get_size(mkeys1, &n));
    n /= sizeof(*keys1);

    info("%s:", name);
    for (uint i = 0; i < n; i++) {
        if (vals[i] != FILL_VALUE_UINT) {
            info("  [%u, %u] = %.2u", keys1[i], keys2[i], vals[i]);
        }
    }
}

static spla_Matrix msbfs(spla_Matrix A, uint nrows, uint ncols, spla_Vector src,
                         uint nsrc)
{
    if (nrows != ncols) {
        err("rows (%u) != cols (%u) for the provided matrix", nrows, ncols);
    }
    uint n = nrows;

    spla_Matrix AT = make_matrix(nrows, ncols, tuint);
    sp(spla_Exec_m_transpose(AT, A, spla_OpUnary_IDENTITY_UINT(), NULL, NULL));

    // parent(i, j) is the parent id of node j in source i's BFS
    spla_Matrix parent0 = make_matrix(nsrc, n, tuint);
    spla_Matrix parent1 = make_matrix(nsrc, n, tuint);
    spla_Matrix parent = parent0;

    // front of BFS
    spla_Matrix front0 = make_matrix(nsrc, n, tuint);
    spla_Matrix front1 = make_matrix(nsrc, n, tuint);
    spla_Matrix front = front0;

    // front(i, src) = src
    // parent(i, src) = src
    for (uint i = 0; i < nsrc; i++) {
        for (uint j = 0; j < n; j++) {
            sp(spla_Matrix_set_uint(parent, i, j, FILL_VALUE_UINT));
        }

        uint currsrc = 0;
        sp(spla_Vector_get_uint(src, i, &currsrc));
        sp(spla_Matrix_set_uint(parent, i, (uint)currsrc, currsrc));

        sp(spla_Matrix_set_uint(front, i, (uint)currsrc, currsrc));
    }

    uint nfront = nsrc;
    TIME(for (uint nvisited = nsrc; nvisited < n * nsrc; nvisited += nfront) {
        SWITCH(front, {
            // front = front * AT . !mask
            TIME(sp(spla_Exec_mxmT_maskC_secondI(front, parent, cfront, AT,
                                                 &nfront, NULL, NULL)));
        });

#ifdef DEBUG
        info("nfront: %d", nfront);
#endif

        if (!nfront) {
            break;
        }

        SWITCH(parent, {
            // front(s, i) currently contains the parent id of node i in tree s
            sp(spla_Exec_m_eadd(parent, cparent, front,
                                spla_OpBinary_SECOND_NMAX_UINT(), NULL, NULL));
        });
    });

    return parent;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        err("invalid number of arguments");
    }
    char *path = *(++argv);

    FILE *f = fopen(path, "r");
    if (!f) {
        err("failed to open %s", path);
    }

    la(LAGraph_Init(msg));

    GrB_Matrix A0;
    la(LAGraph_MMRead(&A0, f, msg));
    (void)fclose(f);
    la(LAGraph_Matrix_Print(A0, LAGraph_SUMMARY, stdout, msg));

    GrB_Index nrows, ncols;
    gr(GrB_Matrix_nrows(&nrows, A0));
    gr(GrB_Matrix_ncols(&ncols, A0));

    init_spla();

    spla_Matrix A = make_matrix(nrows, ncols, tuint);
    grb_to_spla(A, A0);
    gr(GrB_Matrix_free(&A0));

    // set every 2nd node as bfs source
    uint nsrc = nrows / 2;
    spla_Vector src = make_vector(nsrc, tuint);
    for (uint i = 0; i < nsrc; i += 1) {
        sp(spla_Vector_set_uint(src, i, i * 2));
    }

    info("start algo");
    spla_Matrix parent = msbfs(A, nrows, ncols, src, nsrc);
    (void)parent;
#ifdef DEBUG
    print_matrix(parent, "parent");
#endif

    exit_spla();
    la(LAGraph_Finalize(msg));
    return 0;
}
