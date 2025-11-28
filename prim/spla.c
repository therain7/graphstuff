#include <LAGraph.h>
#include <GraphBLAS.h>
#include <spla.h>
#include <common.h>

#define PRIM_START_NODE 0

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

// prim's MST algo on adjacency matrix of a graph.
// returns total weight of resulting tree
static float prim(spla_Matrix A, uint nrows, uint ncols)
{
    if (nrows != ncols) {
        err("rows (%u) != cols (%u) for the provided matrix", nrows, ncols);
    }
    uint n = nrows;
    sp(spla_Matrix_set_format(A, SPLA_FORMAT_MATRIX_ACC_CSR));

    // weight of the cheapest edge for each discovered node.
    // updates continuously with new nodes/edges
    // as they get discovered by the algo
    spla_Vector weights0 = make_vector(n, tfloat);
    spla_Vector weights1 = make_vector(n, tfloat);
    spla_Vector weights = weights0;

    // extract neighbors of the start node
    sp(spla_Exec_m_extract_row(weights, A, PRIM_START_NODE, NULL, NULL, NULL));

    // keep track of not yet visited nodes with a mask
    spla_Vector not_visited = make_vector(n, tfloat);
    for (uint i = 0; i < n; i++) {
        if (i != PRIM_START_NODE) {
            sp(spla_Vector_set_float(not_visited, i, 1));
        }
    }
    uint visited_count = 1;

    // auxiliary vectors
    spla_Vector to_visit = make_vector(n, tfloat);
    spla_Vector neighbors = make_vector(n, tfloat);

    float total_weight = 0;
    TIME(while (visited_count < n) {
        // `weights` with applied `not_visited` mask
        sp(spla_Exec_v_emult(to_visit, weights, not_visited,
                             spla_OpBinary_FIRST_FLOAT(), NULL, NULL));

        // find node with cheapest edge connecting it. "add" this edge to MST
        uint cheapest_idx;
        float cheapest_val;
        sp(spla_Exec_v_find_min(&cheapest_idx, &cheapest_val, to_visit, NULL,
                                NULL));
        total_weight += cheapest_val;
        sp(spla_Vector_set_float(not_visited, cheapest_idx, FILL_VALUE_FLOAT));
        visited_count++;

        // find new neighbors. add them to `weights` to process later
        sp(spla_Exec_m_extract_row(neighbors, A, cheapest_idx, NULL, NULL,
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

    spla_Matrix A = make_matrix(nrows, ncols, tfloat);
    grb_to_spla(A, A0);
    gr(GrB_Matrix_free(&A0));

    info("start algo");
    float weight = prim(A, nrows, ncols);
    info("total MST weight = %.2f\n", weight);

    exit_spla();
    la(LAGraph_Finalize(msg));
    return 0;
}
