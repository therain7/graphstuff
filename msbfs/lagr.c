#include <LAGraphX.h>
#include <GraphBLAS.h>
#include <common.h>

__attribute__((unused)) static void print_matrix(GrB_Matrix A, char *name)
{
    GrB_Index n;
    gr(GrB_Matrix_nvals(&n, A));

    GrB_Index *rows, *cols;
    la(LAGraph_Malloc((void **)&rows, n, sizeof(*rows), msg));
    la(LAGraph_Malloc((void **)&cols, n, sizeof(*cols), msg));
    uint64_t *vals;
    la(LAGraph_Malloc((void **)&vals, n, sizeof(*vals), msg));

    gr(GrB_Matrix_extractTuples_UINT64(rows, cols, vals, &n, A));

    info("%s (nvals = %lu):", name, n);
    for (GrB_Index i = 0; i < n; i++) {
        info("  [%lu, %lu] = %lu", rows[i], cols[i], vals[i]);
    }

    la(LAGraph_Free((void **)&vals, msg));
    la(LAGraph_Free((void **)&cols, msg));
    la(LAGraph_Free((void **)&rows, msg));
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

    // parse matrix
    GrB_Matrix A;
    la(LAGraph_MMRead(&A, f, msg));
    (void)fclose(f);
    la(LAGraph_Matrix_Print(A, LAGraph_SUMMARY, stdout, msg));

    GrB_Index nrows, ncols;
    gr(GrB_Matrix_nrows(&nrows, A));
    gr(GrB_Matrix_ncols(&ncols, A));
    if (nrows != ncols) {
        err("rows (%lu) != cols (%lu) for the provided matrix", nrows, ncols);
    }

    // set every 2nd node as bfs source
    GrB_Index nsrc = nrows / 2;
    GrB_Vector src;
    gr(GrB_Vector_new(&src, GrB_UINT64, nsrc));
    for (GrB_Index i = 0; i < nsrc; i += 1) {
        gr(GrB_Vector_setElement(src, i * 2, i));
    }

    LAGraph_Graph G;
    la(LAGraph_New(&G, &A, LAGraph_ADJACENCY_UNDIRECTED, msg));

    info("start algo");
    GrB_Matrix parent;
    la(LAGraph_MultiSourceBFS(NULL, &parent, G, src, msg));
#ifdef DEBUG
    print_matrix(parent, "parent");
#endif

    la(GrB_Matrix_free(&parent));
    la(LAGraph_Delete(&G, msg));
    la(GrB_Vector_free(&src));
    la(LAGraph_Finalize(msg));
    return 0;
}
