#include <LAGraph.h>
#include <GraphBLAS.h>
#include <spla.h>
#include <common.h>

#define FILL_VALUE INFINITY
#define PRIM_START_NODE 0

typedef spla_uint uint;

struct node {
    uint idx;
    float val;
};

static struct node find_min(spla_Vector V)
{
    spla_MemView mkeys, mvals;
    sp(spla_Vector_read(V, &mkeys, &mvals));
    uint *keys;
    sp(spla_MemView_get_buffer(mkeys, (void **)&keys));
    float *vals;
    sp(spla_MemView_get_buffer(mvals, (void **)&vals));

    spla_size_t n;
    sp(spla_MemView_get_size(mkeys, &n));
    n /= sizeof(*keys);

    if (!n) {
        err("n = 0 in find_min");
    }

    uint idx_min = keys[0];
    float val_min = vals[0];
    for (uint idx = 1; idx < n; idx++) {
        float val = vals[idx];
        if (val < val_min) {
            idx_min = keys[idx];
            val_min = val;
        }
    }

    return (struct node){ .idx = idx_min, .val = val_min };
}

// prim's MST algo on adjacency matrix of a graph.
// returns total weight of resulting tree
static float prim(spla_Matrix A, uint nrows, uint ncols)
{
    if (nrows != ncols) {
        err("rows (%u) != cols (%u) for the provided matrix", nrows, ncols);
    }
    uint n = nrows;

    // weight of the cheapest edge for each discovered node.
    // updates continuously with new nodes/edges
    // as they get discovered by the algo
    spla_Vector weights0 = make_vector(n);
    spla_Vector weights1 = make_vector(n);
    spla_Vector weights = weights0;

    // extract neighbors of the start node
    sp(spla_Exec_m_extract_row(weights, A, PRIM_START_NODE, NULL, NULL, NULL));

    // keep track of not yet visited nodes with a mask
    spla_Vector not_visited = make_vector(n);
    for (uint i = 0; i < n; i++) {
        if (i != PRIM_START_NODE) {
            sp(spla_Vector_set_float(not_visited, i, 1));
        }
    }
    uint visited_count = 1;

    // auxiliary vectors
    spla_Vector to_visit = make_vector(n);
    spla_Vector neighbors = make_vector(n);

    float total_weight = 0;
    TIME(while (visited_count < n) {
        // `weights` with applied `not_visited` mask
        sp(spla_Exec_v_emult(to_visit, weights, not_visited,
                             spla_OpBinary_FIRST_FLOAT(), NULL, NULL));

        // find node with cheapest edge connecting it. "add" this edge to MST
        struct node cheapest_node = find_min(to_visit);
        total_weight += cheapest_node.val;
        sp(spla_Vector_set_float(not_visited, cheapest_node.idx, FILL_VALUE));
        visited_count++;

        // find new neighbors. add them to `weights` to process later
        sp(spla_Exec_m_extract_row(neighbors, A, cheapest_node.idx, NULL, NULL,
                                   NULL));

        spla_Vector cur = weights;
        weights = weights == weights0 ? weights1 : weights0;
        sp(spla_Exec_v_eadd(weights, neighbors, cur, spla_OpBinary_MIN_FLOAT(),
                            NULL, NULL));
    });

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

    GrB_Matrix A0;
    la(LAGraph_MMRead(&A0, f, msg));
    (void)fclose(f);
    la(LAGraph_Matrix_Print(A0, LAGraph_SUMMARY, stdout, msg));

    GrB_Index nrows, ncols;
    gr(GrB_Matrix_nrows(&nrows, A0));
    gr(GrB_Matrix_ncols(&ncols, A0));

    init_spla();

    spla_Matrix A = make_matrix(nrows, ncols);
    grb_to_spla(A, A0);
    gr(GrB_Matrix_free(&A0));
    sp(spla_Matrix_set_format(A, SPLA_FORMAT_MATRIX_ACC_CSR));

    info("starting algo");
    float weight = prim(A, nrows, ncols);
    info("total MST weight = %.2f\n", weight);

    exit_spla();
    la(LAGraph_Finalize(msg));
    return 0;
}
