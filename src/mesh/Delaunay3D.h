/*
 * Delaunay3D.h
 *
 * Self-contained, multithreaded 3D Delaunay triangulation.
 *
 * This is a standalone port of geogram's parallel 3D Delaunay
 * (GEO::ParallelDelaunay3d, src/lib/geogram/delaunay/parallel_delaunay_3d.{h,cpp}),
 * including the pieces it depends on (cavity, per-cell atomic status array,
 * BRIO/Hilbert spatial sort, geometric predicates and a small std::thread based
 * thread pool). It has NO external dependency other than the C++ standard
 * library, so it can be compiled on its own (the two files Delaunay3D.{h,cpp}).
 *
 * Original algorithm: Bruno Levy / Inria (geogram), based on ideas of
 * Bowyer-Watson incremental insertion with spatial reordering. See the
 * copyright notice in Delaunay3D.cpp.
 */

#ifndef SPIRULA_DELAUNAY_3D_H
#define SPIRULA_DELAUNAY_3D_H

#include <cstdint>
#include <vector>

namespace delaunay3d {

    /**
     * \brief Largest point count compute_delaunay_3d() accepts. 7 tetrahedra
     *  per point are preallocated and addressed as 4*t+lv in 32 bits, so beyond
     *  this the cell arrays silently wrap.
     */
    constexpr int kMaxPoints = 150000000;

    /**
     * \brief Result of a 3D Delaunay triangulation.
     */
    struct Delaunay3DResult {
        /** number of input vertices */
        int nb_vertices = 0;
        /** number of (finite) tetrahedra */
        int nb_cells = 0;
        /** 4*nb_cells vertex indices (into the input point array). Unsigned so
         *  the solver's own store can be moved out instead of copied. */
        std::vector<uint32_t> cell_vertices;
        /** 4*nb_cells neighbor tet indices, or -1 on the convex hull.
         *  Only filled when compute_adjacency is true. */
        std::vector<int> cell_adjacents;
        /** number of worker threads actually used */
        int num_threads = 0;
    };

    /**
     * \brief Computes the 3D Delaunay triangulation of a set of points.
     * \param[in] points pointer to nb_points*3 doubles, (x,y,z) interleaved
     * \param[in] nb_points number of points
     * \param[in] num_threads number of worker threads, or <=0 to use all
     *  available hardware threads
     * \param[in] compute_adjacency if true, also fill cell_adjacents
     * \return the triangulation (only finite tetrahedra are returned)
     */
    Delaunay3DResult compute_delaunay_3d(
        const double* points,
        int nb_points,
        int num_threads = 0,
        bool compute_adjacency = true
    );

} // namespace delaunay3d

#endif
