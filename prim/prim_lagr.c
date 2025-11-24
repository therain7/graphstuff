#include <stdlib.h>
#include <stdio.h>

#include <LAGraph.h>
#include <GraphBLAS.h>

#define info(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define err(fmt, ...)                             \
    do {                                          \
        fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                       \
    } while (0);

char msg[LAGRAPH_MSG_LEN];

#define LAGRAPH_CATCH(r)                                                      \
    err("LAGraph error: (%d): file: %s, line: %d\n%s", r, __FILE__, __LINE__, \
        msg);

#define GRB_CATCH(r) \
    err("GraphBLAS error: (%d): file: %s, line: %d", r, __FILE__, __LINE__);

#define la(...) LAGRAPH_TRY(__VA_ARGS__)
#define gr(...) GRB_TRY(__VA_ARGS__)

#define PRIM_START_NODE 0

struct node {
    GrB_Index idx;
    double val;
};

static struct node find_min(GrB_Vector v, GrB_Index *indices, double *values)
{
    GrB_Index n;
    gr(GrB_Vector_nvals(&n, v));

    gr(GrB_Vector_extractTuples_FP64(indices, values, &n, v));
    double val_min = values[0];
    GrB_Index idx_min = indices[0];

    for (GrB_Index i = 1; i < n; i++) {
        if (values[i] < val_min) {
            val_min = values[i];
            idx_min = indices[i];
        }
    }

    return (struct node){ .idx = idx_min, .val = val_min };
}

// prim's MST algo on adjacency matrix of a graph.
// returns total weight of resulting tree
static double prim(GrB_Matrix A)
{
    GrB_Index n, _cols;
    gr(GrB_Matrix_nrows(&n, A));
    gr(GrB_Matrix_nrows(&_cols, A));
    if (n != _cols) {
        err("rows (%lu) != cols (%lu) for the provided matrix", n, _cols);
    }

    // weight of the cheapest edge for each discovered node.
    // updates continuously with new nodes/edges
    // as they get discovered by the algo
    GrB_Vector weights;
    gr(GrB_Vector_new(&weights, GrB_FP64, n));

    // extract neighbors of the start node
    gr(GrB_Col_extract(weights, NULL, NULL, A, GrB_ALL, n, PRIM_START_NODE,
                       GrB_DESC_T0));

    // keep track of visited nodes with a mask
    GrB_Vector visited;
    gr(GrB_Vector_new(&visited, GrB_BOOL, n));
    gr(GrB_Vector_setElement_BOOL(visited, true, PRIM_START_NODE));
    uint64_t visited_count = 1;

    // auxiliary vectors and arrays
    GrB_Vector to_visit, neighbors;
    gr(GrB_Vector_new(&to_visit, GrB_FP64, n));
    gr(GrB_Vector_new(&neighbors, GrB_FP64, n));

    GrB_Index *indices;
    la(LAGraph_Malloc((void **)&indices, n, sizeof(*indices), msg));
    double *values;
    la(LAGraph_Malloc((void **)&values, n, sizeof(*values), msg));

    double total_weight = 0;
    while (visited_count < n) {
        // `weights` with applied visited' mask
        gr(GrB_Vector_assign(to_visit, visited, NULL, weights, GrB_ALL, n,
                             GrB_DESC_RC));

        // find node with cheapest edge connecting it. "add" this edge to MST
        struct node cheapest_node = find_min(to_visit, indices, values);
        total_weight += cheapest_node.val;
        gr(GrB_Vector_setElement_BOOL(visited, true, cheapest_node.idx));
        visited_count++;

        // find new neighbors. add them to `weights` to process later
        gr(GrB_Col_extract(neighbors, NULL, NULL, A, GrB_ALL, n,
                           cheapest_node.idx, GrB_DESC_RT0));
        gr(GrB_Vector_eWiseAdd_BinaryOp(weights, NULL, NULL, GrB_MIN_FP64,
                                        weights, neighbors, NULL));
    }

    la(LAGraph_Free((void **)&values, msg));
    la(LAGraph_Free((void **)&indices, msg));
    gr(GrB_Vector_free(&neighbors));
    gr(GrB_Vector_free(&to_visit));
    gr(GrB_Vector_free(&visited));
    gr(GrB_Vector_free(&weights));

    return total_weight;
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

    GrB_Matrix A;
    la(LAGraph_MMRead(&A, f, msg));
    (void)fclose(f);
    la(LAGraph_Matrix_Print(A, LAGraph_SUMMARY, stdout, msg));

    double weight = prim(A);
    info("total MST weight = %f\n", weight);

    gr(GrB_Matrix_free(&A));
    la(LAGraph_Finalize(msg));
    return 0;
}
