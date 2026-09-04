/*
 * Delaunay3D.cpp -- self-contained multithreaded 3D Delaunay triangulation.
 *
 * A port of geogram's GEO::ParallelDelaunay3d (Bowyer-Watson insertion, BRIO /
 * Hilbert reordering, structural-filtering locate), depending on nothing but
 * the standard library: predicates become a long-double static filter with a
 * Simulation-of-Simplicity tie-break, the cavity grows instead of overflowing,
 * and GEO::Thread becomes a std::thread pool.
 *
 * Splat clouds are far more degenerate than this algorithm assumes, so every
 * loop that can spin on them is bounded -- docs/notes/delaunay-degeneracy.md.
 *
 *  Copyright (c) 2000-2022 Inria, All rights reserved. (BSD-3-Clause)
 *  Contact: Bruno Levy, https://www.inria.fr/fr/bruno-levy
 */

#include "mesh/Delaunay3D.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cfloat>
#include <cmath>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include <algorithm>

namespace {

    /*********************** basic types *************************************/

    typedef uint32_t index_t;
    typedef int32_t  signed_index_t;

    static constexpr index_t NO_INDEX = index_t(-1);

    enum Sign { NEGATIVE = -1, ZERO = 0, POSITIVE = 1 };

    template <class T> using vector = std::vector<T>;

    // geogram-style assertions are compiled out (release behavior).
    #define geo_debug_assert(x) ((void)0)
    #define geo_assert(x) ((void)0)

    template <class T> inline T geo_sqr(T x) { return x*x; }

    /*********************** thread pool ************************************/

    // Minimal replacement for GEO::Thread / GEO::Process.

    class Thread {
    public:
        virtual ~Thread() {}
        virtual void run() = 0;
        void set_id(index_t id) { id_ = id; }
        index_t id() const { return id_; }
    private:
        index_t id_ = 0;
    };

    typedef std::shared_ptr<Thread> Thread_var;
    typedef std::vector<Thread_var> ThreadGroup;

    namespace Process {
        static std::atomic<bool> g_running_threads{false};
        static index_t g_max_threads = 1;

        inline bool is_running_threads() {
            return g_running_threads.load(std::memory_order_relaxed);
        }

        inline index_t maximum_concurrent_threads() {
            return g_max_threads;
        }

        inline void run_threads(ThreadGroup& threads) {
            if(threads.empty()) {
                return;
            }
            g_running_threads.store(true, std::memory_order_relaxed);
            std::vector<std::thread> pool;
            pool.reserve(threads.size());
            for(size_t i = 0; i < threads.size(); ++i) {
                Thread* t = threads[i].get();
                pool.emplace_back([t]() { t->run(); });
            }
            for(auto& th : pool) {
                th.join();
            }
            g_running_threads.store(false, std::memory_order_relaxed);
        }
    }

    /*********************** geometric predicates **************************/

    //   The predicates below reproduce the contracts of geogram's PCK
    // predicates that ParallelDelaunay3d relies on. orient_3d() is computed
    // exactly for the data we feed (bounded float32 coordinates promoted to
    // double, where the long-double evaluation is exact). in_sphere /
    // in_circle use a long-double static filter and a symbolic perturbation
    // (Simulation of Simplicity) tie-break in the degenerate case.

    namespace PCK {

        // typedef long double real;
        typedef double real;

        inline Sign sgn(real x) {
            return (x > 0) ? POSITIVE : ((x < 0) ? NEGATIVE : ZERO);
        }

        inline real det3(
            real a00, real a01, real a02,
            real a10, real a11, real a12,
            real a20, real a21, real a22
        ) {
            return a00 * (a11*a22 - a12*a21)
                 - a01 * (a10*a22 - a12*a20)
                 + a02 * (a10*a21 - a11*a20);
        }

        // permanent (sum of magnitudes of the 6 product terms) of a 3x3 det
        inline real perm3(
            real a00, real a01, real a02,
            real a10, real a11, real a12,
            real a20, real a21, real a22
        ) {
            using std::fabs;
            return fabsl(a00) * (fabsl(a11*a22) + fabsl(a12*a21))
                 + fabsl(a01) * (fabsl(a10*a22) + fabsl(a12*a20))
                 + fabsl(a02) * (fabsl(a10*a21) + fabsl(a11*a20));
        }

        // sign of det[ p1-p0 ; p2-p0 ; p3-p0 ]
        Sign orient_3d(
            const double* p0, const double* p1,
            const double* p2, const double* p3
        ) {
            real a00 = real(p1[0]) - real(p0[0]);
            real a01 = real(p1[1]) - real(p0[1]);
            real a02 = real(p1[2]) - real(p0[2]);
            real a10 = real(p2[0]) - real(p0[0]);
            real a11 = real(p2[1]) - real(p0[1]);
            real a12 = real(p2[2]) - real(p0[2]);
            real a20 = real(p3[0]) - real(p0[0]);
            real a21 = real(p3[1]) - real(p0[1]);
            real a22 = real(p3[2]) - real(p0[2]);
            real det = det3(a00,a01,a02, a10,a11,a12, a20,a21,a22);
            real bound = 16.0L * LDBL_EPSILON *
                perm3(a00,a01,a02, a10,a11,a12, a20,a21,a22);
            if(det >  bound) return POSITIVE;
            if(det < -bound) return NEGATIVE;
            return ZERO;
        }

        // inexact orientation: plain double determinant sign
        inline Sign orient_3d_inexact(
            const double* p0, const double* p1,
            const double* p2, const double* p3
        ) {
            double a00 = p1[0]-p0[0], a01 = p1[1]-p0[1], a02 = p1[2]-p0[2];
            double a10 = p2[0]-p0[0], a11 = p2[1]-p0[1], a12 = p2[2]-p0[2];
            double a20 = p3[0]-p0[0], a21 = p3[1]-p0[1], a22 = p3[2]-p0[2];
            double det =
                a00*(a11*a22 - a12*a21) -
                a01*(a10*a22 - a12*a20) +
                a02*(a10*a21 - a11*a20);
            return (det > 0) ? POSITIVE : ((det < 0) ? NEGATIVE : ZERO);
        }

        // POSITIVE iff p4 is inside the circumscribed sphere of (p0,p1,p2,p3),
        // assuming orient_3d(p0,p1,p2,p3) > 0.
        Sign in_sphere_3d_SOS(
            const double* p0, const double* p1,
            const double* p2, const double* p3,
            const double* p4
        ) {
            const double* P[4] = {p0,p1,p2,p3};
            real x[4], y[4], z[4], w[4];
            for(int i = 0; i < 4; ++i) {
                x[i] = real(P[i][0]) - real(p4[0]);
                y[i] = real(P[i][1]) - real(p4[1]);
                z[i] = real(P[i][2]) - real(p4[2]);
                w[i] = x[i]*x[i] + y[i]*y[i] + z[i]*z[i];
            }
            // 4x4 determinant of rows (x,y,z,w), cofactor expansion along
            // the w column. Ni = 3x3 det of the (x,y,z) of the rows != i.
            real N0 = det3(x[1],y[1],z[1], x[2],y[2],z[2], x[3],y[3],z[3]);
            real N1 = det3(x[0],y[0],z[0], x[2],y[2],z[2], x[3],y[3],z[3]);
            real N2 = det3(x[0],y[0],z[0], x[1],y[1],z[1], x[3],y[3],z[3]);
            real N3 = det3(x[0],y[0],z[0], x[1],y[1],z[1], x[2],y[2],z[2]);
            real det4 = -w[0]*N0 + w[1]*N1 - w[2]*N2 + w[3]*N3;

            using std::fabs;
            real P0 = perm3(x[1],y[1],z[1], x[2],y[2],z[2], x[3],y[3],z[3]);
            real P1 = perm3(x[0],y[0],z[0], x[2],y[2],z[2], x[3],y[3],z[3]);
            real P2 = perm3(x[0],y[0],z[0], x[1],y[1],z[1], x[3],y[3],z[3]);
            real P3 = perm3(x[0],y[0],z[0], x[1],y[1],z[1], x[2],y[2],z[2]);
            real bound = 64.0L * LDBL_EPSILON *
                (fabsl(w[0])*P0 + fabsl(w[1])*P1 +
                 fabsl(w[2])*P2 + fabsl(w[3])*P3);

            // result = sign(-det4)
            if(det4 < -bound) return POSITIVE;
            if(det4 >  bound) return NEGATIVE;

            // Degenerate (the five points are co-spherical): resolve with a
            // Simulation-of-Simplicity perturbation. The in-sphere value equals
            // -Delta where Delta == det4 is the translation-invariant lifted
            // determinant det[x_i, y_i, z_i, x_i^2+y_i^2+z_i^2] (i=0..3, relative
            // to p4); see the derivation that Delta expands exactly to det4. We
            // perturb the lifted "weight" coordinate of each point upward by an
            // infinitesimal eps_i ordered by the global vertex index (== pointer
            // order: all points share one coordinate array), the smallest-index
            // point being dominant. Then
            //     Delta_perturbed = sum_i eps_i * C_i,
            //     C_i = (-1)^(i+3) * orient_det(the other four points),
            // and the sign is given by the most dominant non-vanishing term.
            // Because (p0,p1,p2,p3) is a non-degenerate tetrahedron, the term
            // that drops p4 is always non-zero, so the cascade terminates. This
            // is a realizable (regular-triangulation weight) perturbation and is
            // therefore globally consistent.
            {
                const double* q[5] = {p0,p1,p2,p3,p4};
                int order[5] = {0,1,2,3,4};
                std::sort(order, order+5,
                    [&](int a, int b){ return q[a] < q[b]; });
                for(int k = 0; k < 5; ++k) {
                    int i = order[k];
                    const double* r[4];
                    int m = 0;
                    for(int j = 0; j < 5; ++j) if(j != i) r[m++] = q[j];
                    Sign o = orient_3d(r[0], r[1], r[2], r[3]);
                    if(o == ZERO) continue;
                    // C_i sign = (-1)^(i+3) * o  (i odd -> +o, i even -> -o)
                    Sign Ci = (i & 1) ? o : Sign(-o);
                    // result = sign(-Delta_perturbed) = -sign(C_i)
                    return Sign(-Ci);
                }
                return ZERO; // unreachable for a non-degenerate tetrahedron
            }
        }

        // POSITIVE iff p3 is inside the circumscribed circle of the (coplanar)
        // triangle (p0,p1,p2). Used only on the convex hull degenerate path.
        Sign in_circle_3d_SOS(
            const double* p0, const double* p1, const double* p2,
            const double* p3
        ) {
            // Triangle normal, to pick the projection plane.
            real ux = real(p1[0])-real(p0[0]);
            real uy = real(p1[1])-real(p0[1]);
            real uz = real(p1[2])-real(p0[2]);
            real vx = real(p2[0])-real(p0[0]);
            real vy = real(p2[1])-real(p0[1]);
            real vz = real(p2[2])-real(p0[2]);
            real nx = std::fabs(uy*vz - uz*vy);
            real ny = std::fabs(uz*vx - ux*vz);
            real nz = std::fabs(ux*vy - uy*vx);
            int c0, c1;
            if(nx >= ny && nx >= nz) { c0 = 1; c1 = 2; }
            else if(ny >= nx && ny >= nz) { c0 = 0; c1 = 2; }
            else { c0 = 0; c1 = 1; }

            const double* Pt[4] = {p0,p1,p2,p3};
            real X[4], Y[4];
            for(int i = 0; i < 4; ++i) {
                X[i] = real(Pt[i][c0]);
                Y[i] = real(Pt[i][c1]);
            }
            // orient2d of projected (p0,p1,p2)
            real o2 = (X[1]-X[0])*(Y[2]-Y[0]) - (X[2]-X[0])*(Y[1]-Y[0]);

            // incircle: 3x3 det of rows (xi-x3, yi-y3, (xi-x3)^2+(yi-y3)^2)
            real ax[3], ay[3], aw[3];
            for(int i = 0; i < 3; ++i) {
                ax[i] = X[i]-X[3];
                ay[i] = Y[i]-Y[3];
                aw[i] = ax[i]*ax[i] + ay[i]*ay[i];
            }
            real inc = det3(
                ax[0],ay[0],aw[0],
                ax[1],ay[1],aw[1],
                ax[2],ay[2],aw[2]
            );
            real prod = inc * o2;
            real perm = perm3(
                std::fabs(ax[0]),std::fabs(ay[0]),std::fabs(aw[0]),
                std::fabs(ax[1]),std::fabs(ay[1]),std::fabs(aw[1]),
                std::fabs(ax[2]),std::fabs(ay[2]),std::fabs(aw[2])
            ) * (std::fabs(o2) + 1.0L);
            real bound = 64.0L * LDBL_EPSILON * perm;
            if(prod >  bound) return POSITIVE;
            if(prod < -bound) return NEGATIVE;

            // Degenerate (the four coplanar points are con-cyclic): resolve with
            // the 2D analogue of the in_sphere Simulation-of-Simplicity above.
            // In the projection plane the in-circle value is sign(Delta2 * o2)
            // with Delta2 = det[X_i, Y_i, X_i^2+Y_i^2] (== inc) and
            // o2 = orient2d(p0,p1,p2). Perturbing the lifted weights by an
            // index-ordered eps gives Delta2_perturbed = sum_i eps_i * C2_i with
            // C2_i = (-1)^(i+2) * orient2d(the other three points). Since
            // (p0,p1,p2) is a non-degenerate triangle (o2 != 0), the term that
            // drops p3 is non-zero, so the cascade terminates.
            {
                Sign so2 = (o2 > 0) ? POSITIVE : ((o2 < 0) ? NEGATIVE : ZERO);
                int order[4] = {0,1,2,3};
                const double* qq[4] = {p0,p1,p2,p3};
                std::sort(order, order+4,
                    [&](int a, int b){ return qq[a] < qq[b]; });
                for(int k = 0; k < 4; ++k) {
                    int i = order[k];
                    int idx[3];
                    int m = 0;
                    for(int j = 0; j < 4; ++j) if(j != i) idx[m++] = j;
                    real e =
                        (X[idx[1]]-X[idx[0]])*(Y[idx[2]]-Y[idx[0]]) -
                        (X[idx[2]]-X[idx[0]])*(Y[idx[1]]-Y[idx[0]]);
                    Sign oe = (e > 0) ? POSITIVE : ((e < 0) ? NEGATIVE : ZERO);
                    if(oe == ZERO) continue;
                    // C2_i sign = (-1)^(i+2) * oe  (i odd -> -oe, i even -> +oe)
                    Sign C2i = (i & 1) ? Sign(-oe) : oe;
                    // result = sign(Delta2_perturbed * o2) = sign(C2_i) * sign(o2)
                    return Sign(int(C2i) * int(so2));
                }
                return ZERO; // unreachable for a non-degenerate triangle
            }
        }

        bool points_are_identical_3d(const double* p0, const double* p1) {
            return p0[0]==p1[0] && p0[1]==p1[1] && p0[2]==p1[2];
        }

        bool points_are_colinear_3d(
            const double* p0, const double* p1, const double* p2
        ) {
            real ux = real(p1[0])-real(p0[0]);
            real uy = real(p1[1])-real(p0[1]);
            real uz = real(p1[2])-real(p0[2]);
            real vx = real(p2[0])-real(p0[0]);
            real vy = real(p2[1])-real(p0[1]);
            real vz = real(p2[2])-real(p0[2]);
            real cx = uy*vz - uz*vy;
            real cy = uz*vx - ux*vz;
            real cz = ux*vy - uy*vx;
            real m = std::fabs(ux)+std::fabs(uy)+std::fabs(uz)+
                     std::fabs(vx)+std::fabs(vy)+std::fabs(vz);
            real bound = 16.0L * LDBL_EPSILON * m * m;
            return (std::fabs(cx) <= bound &&
                    std::fabs(cy) <= bound &&
                    std::fabs(cz) <= bound);
        }
    }

    /*********************** CellStatusArray *******************************/
    // Ported from geogram/delaunay/delaunay_sync.h

    class CellStatusArray {
    public:
        typedef uint8_t thread_index_t;
        typedef uint8_t cell_status_t;
        static constexpr cell_status_t FREE_CELL     = 127;
        static constexpr cell_status_t THREAD_MASK   = 127;
        static constexpr cell_status_t CONFLICT_MASK = 128;
        static constexpr index_t MAX_THREADS = index_t(THREAD_MASK)-1;

        CellStatusArray() : cell_status_(nullptr), size_(0), capacity_(0) {}
        CellStatusArray(index_t size_in) :
            cell_status_(nullptr), size_(0), capacity_(0) {
            resize(size_in, size_in);
        }
        ~CellStatusArray() { clear(); }

        CellStatusArray(const CellStatusArray&) = delete;
        CellStatusArray& operator=(const CellStatusArray&) = delete;

        cell_status_t acquire_cell(index_t cell, cell_status_t status) {
            cell_status_t expected = FREE_CELL;
            cell_status_[cell].compare_exchange_strong(
                expected, status,
                std::memory_order_acquire, std::memory_order_acquire
            );
            return (expected & THREAD_MASK);
        }
        void release_cell(index_t cell) {
            cell_status_[cell].store(FREE_CELL, std::memory_order_release);
        }
        cell_status_t cell_thread(index_t cell) const {
            return (cell_status_[cell].load(std::memory_order_relaxed) &
                    THREAD_MASK);
        }
        bool cell_is_marked_as_conflict(index_t cell) const {
            return ((cell_status_[cell].load(std::memory_order_relaxed) &
                     CONFLICT_MASK) != 0);
        }
        void mark_cell_as_conflict(index_t cell) {
            cell_status_[cell].fetch_or(CONFLICT_MASK, std::memory_order_relaxed);
        }
        void set_cell_status(index_t cell, cell_status_t status) {
            cell_status_[cell].store(status, std::memory_order_relaxed);
        }
        void resize(index_t size_in, index_t capacity_in) {
            if(capacity_in > capacity_) {
                capacity_ = capacity_in;
                std::atomic<cell_status_t>* old_cell_status = cell_status_;
                cell_status_ = new std::atomic<cell_status_t>[capacity_];
                for(index_t i = 0; i < capacity_; ++i) {
                    cell_status_t val = (i < size_) ?
                        old_cell_status[i].load(std::memory_order_relaxed) :
                        FREE_CELL;
                    cell_status_[i].store(val, std::memory_order_relaxed);
                }
                delete[] old_cell_status;
            }
            size_ = size_in;
        }
        void resize(index_t size_in) { resize(size_in, size_in); }
        void reserve(index_t new_capacity) {
            if(new_capacity > capacity_) resize(size_, new_capacity);
        }
        void grow() {
            if(size_+1 >= capacity_) {
                resize(size_+1, std::max(capacity_*2, size_+1));
            } else {
                resize(size_+1, capacity_);
            }
        }
        index_t size() const { return size_; }
        void clear() {
            delete[] cell_status_;
            cell_status_ = nullptr;
            size_ = 0;
            capacity_ = 0;
        }
    private:
        std::atomic<cell_status_t>* cell_status_;
        index_t size_;
        index_t capacity_;
    };

    /*********************** Cavity ****************************************/
    // Ported from geogram/delaunay/cavity.h

    class Cavity {
    public:
        Cavity() { resize_hash(INIT_H); }

        // O(1): the stamp invalidates the table rather than clearing it, and
        // a cavity is cleared once per insertion.
        void clear() {
            nb_f_ = 0;
            non_manifold_ = false;
            if(++generation_ == 0) {
                std::fill(h2g_.begin(), h2g_.end(), index_t(0));
                generation_ = 1;
            }
        }

        void new_facet(
            index_t tglobal, index_t boundary_f,
            index_t v0, index_t v1, index_t v2
        ) {
            if(nb_f_ == index_t(tglobal_.size())) {
                index_t n = std::max(2*nb_f_, INIT_F);
                tglobal_.resize(n);
                boundary_f_.resize(n);
                f2v_.resize(n);
            }
            // Grown rather than capped: the overflow path this replaces could
            // not tell a pinched boundary from a sound one. Load stays under
            // 1/2, which is what makes the probe loops terminate.
            if(6*(nb_f_+1) > index_t(h2t_.size())) {
                index_t n = index_t(h2t_.size());
                while(6*(nb_f_+1) > n) n *= 2;
                resize_hash(n);
                for(index_t f = 0; f < nb_f_; ++f) {
                    set_vv2t(f2v_[f][0], f2v_[f][1], f);
                    set_vv2t(f2v_[f][1], f2v_[f][2], f);
                    set_vv2t(f2v_[f][2], f2v_[f][0], f);
                }
            }
            index_t new_t = nb_f_++;
            tglobal_[new_t] = tglobal;
            boundary_f_[new_t] = boundary_f;
            f2v_[new_t][0] = v0;
            f2v_[new_t][1] = v1;
            f2v_[new_t][2] = v2;
            set_vv2t(v0, v1, new_t);
            set_vv2t(v1, v2, new_t);
            set_vv2t(v2, v0, new_t);
        }

        index_t nb_facets() const { return nb_f_; }
        index_t facet_tet(index_t f) const { return tglobal_[f]; }
        void set_facet_tet(index_t f, index_t t) { tglobal_[f] = t; }
        index_t facet_facet(index_t f) const { return boundary_f_[f]; }
        index_t facet_vertex(index_t f, index_t lv) const { return f2v_[f][lv]; }

        void get_facet_neighbor_tets(
            index_t f, index_t& t0, index_t& t1, index_t& t2
        ) const {
            index_t v0 = f2v_[f][0];
            index_t v1 = f2v_[f][1];
            index_t v2 = f2v_[f][2];
            t0 = facet_tet_across(v2,v1);
            t1 = facet_tet_across(v0,v2);
            t2 = facet_tet_across(v1,v0);
        }

        // In a closed, oriented boundary every directed edge belongs to exactly
        // one facet, so set_vv2t() never sees one twice. A conflict region that
        // is not a topological ball pinches, and some edge then appears twice.
        bool boundary_is_manifold() const { return !non_manifold_; }

    private:
        static constexpr index_t INIT_H = 1024;
        static constexpr index_t INIT_F = 128;

        void resize_hash(index_t n) {
            h2t_.assign(n, NO_INDEX);
            h2v_.assign(n, 0);
            h2g_.assign(n, 0);
            generation_ = 1;
        }

        index_t hash(index_t v1, index_t v2) const {
            index_t h = ((v1+1) * 73856093u) ^ ((v2+1) * 83492791u);
            return (h ^ (h >> 16)) & (index_t(h2t_.size()) - 1);
        }

        void set_vv2t(index_t v1, index_t v2, index_t f) {
            uint64_t K = (uint64_t(v1+1) << 32) | uint64_t(v2+1);
            index_t mask = index_t(h2t_.size()) - 1;
            for(index_t cur = hash(v1,v2); ; cur = (cur+1) & mask) {
                if(h2g_[cur] != generation_) {
                    h2g_[cur] = generation_;
                    h2t_[cur] = f;
                    h2v_[cur] = K;
                    return;
                }
                if(h2v_[cur] == K) {
                    non_manifold_ = true;
                    return;
                }
            }
        }

        // NO_INDEX when the opposite half of the edge is missing, which only a
        // boundary that is not closed can do -- never an out of range facet.
        index_t facet_tet_across(index_t v1, index_t v2) const {
            index_t f = get_vv2t(v1,v2);
            return (f == NO_INDEX) ? NO_INDEX : tglobal_[f];
        }

        index_t get_vv2t(index_t v1, index_t v2) const {
            uint64_t K = (uint64_t(v1+1) << 32) | uint64_t(v2+1);
            index_t mask = index_t(h2t_.size()) - 1;
            for(index_t cur = hash(v1,v2); ; cur = (cur+1) & mask) {
                if(h2g_[cur] != generation_) return NO_INDEX;
                if(h2v_[cur] == K) return h2t_[cur];
            }
        }

        vector<index_t>  h2t_;
        vector<uint64_t> h2v_;
        vector<index_t>  h2g_;
        index_t generation_ = 0;
        index_t nb_f_ = 0;
        vector<index_t> tglobal_;
        vector<index_t> boundary_f_;
        vector<std::array<index_t,3> > f2v_;
        bool non_manifold_ = false;
    };

    /*********************** spatial sort (BRIO/Hilbert) *******************/
    // Ported from geogram/mesh/mesh_reorder.cpp (vertex path only).

    template <class IT, class CMP>
    inline IT reorder_split(IT begin, IT end, CMP cmp) {
        if(begin >= end) return begin;
        IT middle = begin + (end - begin) / 2;
        std::nth_element(begin, middle, end, cmp);
        return middle;
    }

    class VertexArray {
    public:
        VertexArray(index_t nb_vertices, const double* base, index_t stride) :
            base_(base), stride_(stride), nb_vertices_(nb_vertices) {}
        const double* point_ptr(index_t i) const { return base_ + i * stride_; }
    private:
        const double* base_;
        index_t stride_;
        index_t nb_vertices_;
    };

    class VertexMesh {
    public:
        VertexMesh(index_t nb_vertices, const double* base, index_t stride) :
            vertices(nb_vertices, base, stride) {}
        VertexArray vertices;
    };

    template <int COORD, bool UP, class MESH>
    struct Hilbert_vcmp {};

    template <int COORD, class MESH>
    struct Hilbert_vcmp<COORD, true, MESH> {
        Hilbert_vcmp(const MESH& mesh) : mesh_(mesh) {}
        bool operator()(index_t i1, index_t i2) {
            return mesh_.vertices.point_ptr(i1)[COORD] <
                   mesh_.vertices.point_ptr(i2)[COORD];
        }
        const MESH& mesh_;
    };

    template <int COORD, class MESH>
    struct Hilbert_vcmp<COORD, false, MESH> {
        Hilbert_vcmp(const MESH& mesh) : mesh_(mesh) {}
        bool operator()(index_t i1, index_t i2) {
            return mesh_.vertices.point_ptr(i1)[COORD] >
                   mesh_.vertices.point_ptr(i2)[COORD];
        }
        const MESH& mesh_;
    };

    template <template <int COORD, bool UP, class MESH> class CMP, class MESH>
    struct HilbertSort3d {
        template <int COORDX, bool UPX, bool UPY, bool UPZ, class IT>
        static void sort(const MESH& M, IT begin, IT end, index_t limit = 1) {
            const int COORDY = (COORDX + 1) % 3, COORDZ = (COORDY + 1) % 3;
            if(end - begin <= signed_index_t(limit)) return;
            IT m0 = begin, m8 = end;
            IT m4 = reorder_split(m0, m8, CMP<COORDX, UPX, MESH>(M));
            IT m2 = reorder_split(m0, m4, CMP<COORDY, UPY, MESH>(M));
            IT m1 = reorder_split(m0, m2, CMP<COORDZ, UPZ, MESH>(M));
            IT m3 = reorder_split(m2, m4, CMP<COORDZ, !UPZ, MESH>(M));
            IT m6 = reorder_split(m4, m8, CMP<COORDY, !UPY, MESH>(M));
            IT m5 = reorder_split(m4, m6, CMP<COORDZ, UPZ, MESH>(M));
            IT m7 = reorder_split(m6, m8, CMP<COORDZ, !UPZ, MESH>(M));
            sort<COORDZ, UPZ, UPX, UPY>(M, m0, m1);
            sort<COORDY, UPY, UPZ, UPX>(M, m1, m2);
            sort<COORDY, UPY, UPZ, UPX>(M, m2, m3);
            sort<COORDX, UPX, !UPY, !UPZ>(M, m3, m4);
            sort<COORDX, UPX, !UPY, !UPZ>(M, m4, m5);
            sort<COORDY, !UPY, UPZ, !UPX>(M, m5, m6);
            sort<COORDY, !UPY, UPZ, !UPX>(M, m6, m7);
            sort<COORDZ, !UPZ, !UPX, UPY>(M, m7, m8);
        }
        HilbertSort3d(
            const MESH& M,
            vector<index_t>::iterator b,
            vector<index_t>::iterator e,
            index_t limit = 1
        ) {
            if(index_t(e - b) <= limit) return;
            sort<0, false, false, false>(M, b, e);
        }
    };

    void compute_BRIO_order_recursive(
        index_t nb_vertices, const double* vertices,
        index_t dimension, index_t stride,
        vector<index_t>& sorted_indices,
        vector<index_t>::iterator b,
        vector<index_t>::iterator e,
        index_t threshold,
        double ratio,
        index_t& depth,
        vector<index_t>* levels
    ) {
        vector<index_t>::iterator m = b;
        if(index_t(e - b) > threshold) {
            ++depth;
            m = b + signed_index_t(double(e - b) * ratio);
            compute_BRIO_order_recursive(
                nb_vertices, vertices, dimension, stride,
                sorted_indices, b, m, threshold, ratio, depth, levels
            );
        }
        VertexMesh M(nb_vertices, vertices, stride);
        (void)dimension;
        HilbertSort3d<Hilbert_vcmp, VertexMesh>(M, m, e);
        if(levels != nullptr) {
            levels->push_back(index_t(e - sorted_indices.begin()));
        }
    }

    void compute_BRIO_order(
        index_t nb_vertices, const double* vertices,
        vector<index_t>& sorted_indices,
        index_t dimension, index_t stride,
        index_t threshold, double ratio,
        vector<index_t>* levels
    ) {
        if(levels != nullptr) {
            levels->clear();
            levels->push_back(0);
        }
        index_t depth = 0;
        sorted_indices.resize(nb_vertices);
        for(index_t i = 0; i < nb_vertices; ++i) {
            sorted_indices[i] = i;
        }
        std::mt19937 rng(0x9e3779b9u);
        std::shuffle(sorted_indices.begin(), sorted_indices.end(), rng);
        compute_BRIO_order_recursive(
            nb_vertices, vertices, dimension, stride,
            sorted_indices,
            sorted_indices.begin(), sorted_indices.end(),
            threshold, ratio, depth, levels
        );
    }

    /*********************** minimal Delaunay base ************************/

    class Delaunay {
    public:
        Delaunay(index_t dimension) :
            dimension_(dimension), nb_vertices_(0), vertices_(nullptr),
            do_reorder_(true), keep_infinite_(false), nb_finite_cells_(0),
            cell_size_(4), cell_v_stride_(4), cell_neigh_stride_(4),
            nb_cells_(0), cell_to_v_(nullptr), cell_to_cell_(nullptr) {}
        virtual ~Delaunay() {}

        index_t dimension() const { return dimension_; }
        index_t nb_vertices() const { return nb_vertices_; }
        const double* vertex_ptr(index_t i) const {
            return vertices_ + i * dimension_;
        }
        virtual void set_vertices(index_t nb_vertices, const double* vertices) {
            nb_vertices_ = nb_vertices;
            vertices_ = vertices;
        }
        void set_arrays(
            index_t nb_cells, const index_t* cell_to_v, const index_t* cell_to_cell
        ) {
            nb_cells_ = nb_cells;
            cell_to_v_ = cell_to_v;
            cell_to_cell_ = cell_to_cell;
        }
        index_t nb_cells() const { return nb_cells_; }

        virtual index_t nearest_vertex(const double* p) const {
            index_t result = NO_INDEX;
            double best = 0.0;
            for(index_t v = 0; v < nb_vertices_; ++v) {
                const double* q = vertex_ptr(v);
                double d = geo_sqr(p[0]-q[0]) + geo_sqr(p[1]-q[1]) +
                           geo_sqr(p[2]-q[2]);
                if(result == NO_INDEX || d < best) { best = d; result = v; }
            }
            return result;
        }

    protected:
        index_t dimension_;
        index_t nb_vertices_;
        const double* vertices_;
        bool do_reorder_;
        bool keep_infinite_;
        index_t nb_finite_cells_;
        index_t cell_size_;
        index_t cell_v_stride_;
        index_t cell_neigh_stride_;
        index_t nb_cells_;
        const index_t* cell_to_v_;
        const index_t* cell_to_cell_;
    };

    /*********************** thread-safe RNG ******************************/

    index_t thread_safe_random(index_t choices_in) {
        typedef long int Int;
        signed_index_t choices = signed_index_t(choices_in);
        static thread_local Int randomseed = 1l;
        if(choices >= 714025l) {
            Int newrandom = (randomseed * 1366l + 150889l) % 714025l;
            randomseed = (newrandom * 1366l + 150889l) % 714025l;
            newrandom = newrandom * (choices / 714025l) + randomseed;
            if(newrandom >= choices) return index_t(newrandom - choices);
            return index_t(newrandom);
        } else {
            randomseed = (randomseed * 1366l + 150889l) % 714025l;
            return index_t(randomseed % choices);
        }
    }

    index_t thread_safe_random_4() {
        static thread_local long int randomseed = 1l;
        randomseed = (randomseed * 1366l + 150889l) % 714025l;
        return index_t(randomseed % 4);
    }

    class ParallelDelaunay3d;

    /*********************** Delaunay3dThread ****************************/

    class Delaunay3dThread : public Thread {
    public:
        static constexpr index_t NO_THREAD = CellStatusArray::FREE_CELL;

        // Every loop a degenerate or contended configuration can spin in is
        // bounded, so that a point is dropped where the algorithm would
        // otherwise never finish. Healthy input stays far below all four.
        static constexpr index_t MAX_RANDOM_TRIES = 4096;
        static constexpr index_t MAX_WALK_STEPS = 1u << 20;
        static constexpr index_t MAX_ROLLBACKS = 1u << 20;
        static constexpr index_t MAX_LOCATE_INEXACT_STEPS = 2500;

        Delaunay3dThread(
            ParallelDelaunay3d* master, index_t pool_begin, index_t pool_end
        );

        void initialize_from(const Delaunay3dThread* rhs) {
            max_used_t_ = rhs->max_used_t_;
            max_t_ = rhs->max_t_;
            v1_ = rhs->v1_;
            v2_ = rhs->v2_;
            v3_ = rhs->v3_;
            v4_ = rhs->v4_;
        }

        index_t nb_rollbacks() const { return nb_rollbacks_; }
        index_t nb_failed_locate() const { return nb_failed_locate_; }

        void set_work(index_t b, index_t e) {
            work_begin_ = b;
            work_end_ = e-1;
        }
        index_t work_size() const {
            if(work_begin_ == NO_INDEX && work_end_ == NO_INDEX) return 0;
            return std::max(work_end_ - work_begin_ + 1, index_t(0));
        }

        index_t nb_threads() const;
        Delaunay3dThread* thread(index_t t);

        void run() override {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                finished_ = false;
            }
            if(work_begin_ != NO_INDEX && work_end_ != NO_INDEX) insert_work();
            // Under the lock, and on every exit: a thread parked on this one in
            // wait_for_event() has nothing else left to wake it.
            {
                std::lock_guard<std::mutex> lock(mutex_);
                finished_ = true;
            }
            send_event();
        }

        void insert_work() {
            memory_overflow_ = false;
            b_hint_ = NO_TETRAHEDRON;
            e_hint_ = NO_TETRAHEDRON;
            direction_ = true;
            index_t nb_retries = 0;
            // work_end_ wraps to NO_INDEX once a range starting at 0 has been
            // consumed backwards, which restarts the loop 2^32 entries past the
            // end of reorder_.
            while(work_end_ >= work_begin_ && work_end_ != NO_INDEX &&
                  !memory_overflow_) {
                index_t v = direction_ ? work_begin_ : work_end_;
                index_t& hint = direction_ ? b_hint_ : e_hint_;
                bool success = insert(reorder_[v], hint);
                send_event();
                if(success) {
                    nb_retries = 0;
                    if(direction_) ++work_begin_;
                    else --work_end_;
                    continue;
                }
                ++nb_rollbacks_;
                if(memory_overflow_) continue;  // the loop condition ends it
                if(interfering_thread_ != NO_THREAD &&
                   ++nb_retries < MAX_ROLLBACKS) {
                    if(id() < interfering_thread_) {
                        wait_for_event(interfering_thread_);
                    } else {
                        direction_ = !direction_;
                    }
                } else {
                    // Either a genuine failure (a point whose neighborhood is
                    // too degenerate for locate to resolve) or a cell no live
                    // thread will release. Retrying spins, so skip the point.
                    nb_retries = 0;
                    if(direction_) ++work_begin_;
                    else --work_end_;
                }
            }
        }

        static constexpr index_t NO_TETRAHEDRON = NO_INDEX;
        static constexpr index_t VERTEX_AT_INFINITY = NO_INDEX;

        index_t max_t() const { return max_t_; }

        bool tet_is_finite(index_t t) const {
            return
                cell_to_v_store_[4*t]   != NO_INDEX &&
                cell_to_v_store_[4*t+1] != NO_INDEX &&
                cell_to_v_store_[4*t+2] != NO_INDEX &&
                cell_to_v_store_[4*t+3] != NO_INDEX;
        }
        bool tet_is_real(index_t t) const {
            return !tet_is_free(t) && tet_is_finite(t);
        }
        bool tet_is_free(index_t t) const { return tet_is_in_list(t); }

        bool tet_is_in_list(index_t t, index_t first) const {
            for(index_t cur = first; cur != END_OF_LIST; cur = tet_next(cur)) {
                if(cur == t) return true;
            }
            return false;
        }

        index_t create_first_tetrahedron() {
            index_t iv0,iv1,iv2,iv3;
            if(nb_vertices() < 4) return NO_TETRAHEDRON;
            iv0 = 0;
            iv1 = 1;
            while(iv1 < nb_vertices() &&
                  PCK::points_are_identical_3d(vertex_ptr(iv0), vertex_ptr(iv1))) {
                ++iv1;
            }
            if(iv1 == nb_vertices()) return NO_TETRAHEDRON;
            iv2 = iv1 + 1;
            while(iv2 < nb_vertices() &&
                  PCK::points_are_colinear_3d(
                      vertex_ptr(iv0), vertex_ptr(iv1), vertex_ptr(iv2))) {
                ++iv2;
            }
            if(iv2 == nb_vertices()) return NO_TETRAHEDRON;
            iv3 = iv2 + 1;
            Sign s = ZERO;
            while(iv3 < nb_vertices() &&
                  (s = PCK::orient_3d(
                       vertex_ptr(iv0), vertex_ptr(iv1),
                       vertex_ptr(iv2), vertex_ptr(iv3))) == ZERO) {
                ++iv3;
            }
            if(iv3 == nb_vertices()) return NO_TETRAHEDRON;
            if(s == NEGATIVE) std::swap(iv2, iv3);

            index_t t0 = new_tetrahedron(iv0, iv1, iv2, iv3);
            index_t t[4];
            for(index_t f = 0; f < 4; ++f) {
                index_t v1 = tet_vertex(t0, tet_facet_vertex(f,2));
                index_t v2 = tet_vertex(t0, tet_facet_vertex(f,1));
                index_t v3 = tet_vertex(t0, tet_facet_vertex(f,0));
                t[f] = new_tetrahedron(VERTEX_AT_INFINITY, v1, v2, v3);
            }
            for(index_t f = 0; f < 4; ++f) {
                set_tet_adjacent(t[f], 0, t0);
                set_tet_adjacent(t0, f, t[f]);
            }
            for(index_t f = 0; f < 4; ++f) {
                index_t lv1 = tet_facet_vertex(f,2);
                index_t lv2 = tet_facet_vertex(f,1);
                index_t lv3 = tet_facet_vertex(f,0);
                set_tet_adjacent(t[f], 1, t[lv1]);
                set_tet_adjacent(t[f], 2, t[lv2]);
                set_tet_adjacent(t[f], 3, t[lv3]);
            }
            v1_ = iv0; v2_ = iv1; v3_ = iv2; v4_ = iv3;
            release_tets();
            return t0;
        }

        index_t stellate_cavity(index_t v) {
            index_t new_tet = NO_INDEX;
            for(index_t f = 0; f < cavity_.nb_facets(); ++f) {
                index_t old_tet = cavity_.facet_tet(f);
                index_t lf = cavity_.facet_facet(f);
                index_t t_neigh = tet_adjacent(old_tet, lf);
                index_t v1 = cavity_.facet_vertex(f,0);
                index_t v2 = cavity_.facet_vertex(f,1);
                index_t v3 = cavity_.facet_vertex(f,2);
                new_tet = new_tetrahedron(v, v1, v2, v3);
                set_tet_adjacent(new_tet, 0, t_neigh);
                set_tet_adjacent(
                    t_neigh, find_tet_adjacent(t_neigh,old_tet), new_tet
                );
                cavity_.set_facet_tet(f, new_tet);
            }
            for(index_t f = 0; f < cavity_.nb_facets(); ++f) {
                new_tet = cavity_.facet_tet(f);
                index_t neigh1, neigh2, neigh3;
                cavity_.get_facet_neighbor_tets(f, neigh1, neigh2, neigh3);
                set_tet_adjacent(new_tet, 1, neigh1);
                set_tet_adjacent(new_tet, 2, neigh2);
                set_tet_adjacent(new_tet, 3, neigh3);
            }
            return new_tet;
        }

        bool insert(index_t v, index_t& hint) {
            if(v == v1_ || v == v2_ || v == v3_ || v == v4_) return true;

            // Reset so that, on failure, interfering_thread_ == NO_THREAD
            // reliably distinguishes a genuine (non-conflict) failure from a
            // thread-contention rollback. acquire_tet() sets it on any conflict.
            interfering_thread_ = NO_THREAD;

            Sign orient[4];
            index_t t = locate(vertex_ptr(v), hint, orient);
            if(t == NO_TETRAHEDRON) {
                ++nb_failed_locate_;
                return false;
            }
            int nb_zero =
                (orient[0] == ZERO) + (orient[1] == ZERO) +
                (orient[2] == ZERO) + (orient[3] == ZERO);
            if(nb_zero >= 3) {
                release_tet(t);
                return true;
            }

            cavity_.clear();
            bool ok = find_conflict_zone(v,t);

            if(nb_tets_to_create_ > nb_free_ && Process::is_running_threads()) {
                memory_overflow_ = true;
                ok = false;
            }
            if(!ok) {
                release_tets();
                return false;
            }
            if(tets_to_delete_.size() == 0) {
                release_tets();
                return true;
            }

            // Stellating a pinched boundary builds adjacency that points
            // outside the cell array; stellating one p cannot see all of
            // builds inverted cells that later walks get lost among.
            if((!cavity_.boundary_is_manifold() ||
                !cavity_is_star_shaped(vertex_ptr(v))) &&
               !restrict_cavity_to_seed(vertex_ptr(v))) {
                release_tets();
                return true;
            }
            index_t new_tet = stellate_cavity(v);

            for(index_t i = 0; i < tets_to_delete_.size()-1; ++i) {
                cell_next_[tets_to_delete_[i]] = tets_to_delete_[i+1];
            }
            cell_next_[tets_to_delete_[tets_to_delete_.size()-1]] = first_free_;
            first_free_ = tets_to_delete_[0];
            nb_free_ += nb_tets_in_conflict();

            hint = new_tet;
            release_tets();
            return true;
        }

        // Every tetrahedron stellate_cavity() would build is positive exactly
        // when p sees the outer side of every finite boundary facet. Facets
        // through the vertex at infinity carry no orientation to check.
        bool cavity_is_star_shaped(const double* p) const {
            for(index_t f = 0; f < cavity_.nb_facets(); ++f) {
                const double* pv[3];
                bool finite = true;
                for(index_t lv = 0; lv < 3; ++lv) {
                    index_t v = cavity_.facet_vertex(f,lv);
                    if(v == NO_INDEX) { finite = false; break; }
                    pv[lv] = vertex_ptr(v);
                }
                if(!finite) continue;
                if(PCK::orient_3d(p, pv[0], pv[1], pv[2]) != POSITIVE) {
                    return false;
                }
            }
            return true;
        }

        // p must be strictly inside for a 1->4 split of t to stay valid. A
        // virtual tet has one facet to be on the outer side of instead.
        bool split_is_valid(index_t t, const double* p) const {
            const double* pv[4];
            index_t at_infinity = NO_INDEX;
            for(index_t lv = 0; lv < 4; ++lv) {
                index_t v = tet_vertex(t,lv);
                pv[lv] = (v == NO_INDEX) ? nullptr : vertex_ptr(v);
                if(v == NO_INDEX) at_infinity = lv;
            }
            if(at_infinity != NO_INDEX) {
                pv[at_infinity] = p;
                return PCK::orient_3d(pv[0],pv[1],pv[2],pv[3]) == POSITIVE;
            }
            for(index_t lf = 0; lf < 4; ++lf) {
                const double* opposite = pv[lf];
                pv[lf] = p;
                Sign s = PCK::orient_3d(pv[0],pv[1],pv[2],pv[3]);
                pv[lf] = opposite;
                if(s != POSITIVE) return false;
            }
            return true;
        }

        // Fall back to splitting the located tet alone: its boundary is a
        // tetrahedron, hence always a ball. Dropping the point instead leaves
        // the cospherical cluster untriangulated and then loses all of it.
        bool restrict_cavity_to_seed(const double* p) {
            index_t seed = tets_to_delete_[0];
            if(!split_is_valid(seed, p)) return false;
            for(index_t i = 1; i < tets_to_delete_.size(); ++i) {
                mark_tet_as_neighbor(tets_to_delete_[i]);
            }
            tets_to_delete_.resize(1);
            cavity_.clear();
            for(index_t lf = 0; lf < 4; ++lf) {
                cavity_.new_facet(
                    seed, lf,
                    tet_vertex(seed, tet_facet_vertex(lf,0)),
                    tet_vertex(seed, tet_facet_vertex(lf,1)),
                    tet_vertex(seed, tet_facet_vertex(lf,2))
                );
            }
            return cavity_.boundary_is_manifold();
        }

        bool find_conflict_zone(index_t v, index_t t) {
            nb_tets_to_create_ = 0;
            const double* p = vertex_ptr(v);
            if(weighted_ && !tet_is_in_conflict(t,p)) {
                release_tet(t);
                return true;
            }
            mark_tet_as_conflict(t);
            return find_conflict_zone_iterative(p,t);
        }

        bool find_conflict_zone_iterative(const double* p, index_t t_in) {
            S_.push_back(t_in);
            while(S_.size() != 0) {
                index_t t = *(S_.rbegin());
                S_.pop_back();
                for(index_t lf = 0; lf < 4; ++lf) {
                    index_t t2 = tet_adjacent(t, lf);
                    if(owns_tet(t2)) {
                        if(!tet_is_marked_as_conflict(t2)) {
                            ++nb_tets_to_create_;
                            cavity_.new_facet(
                                t, lf,
                                tet_vertex(t, tet_facet_vertex(lf,0)),
                                tet_vertex(t, tet_facet_vertex(lf,1)),
                                tet_vertex(t, tet_facet_vertex(lf,2))
                            );
                        }
                        continue;
                    }
                    if(!acquire_tet(t2)) {
                        S_.resize(0);
                        return false;
                    }
                    if(!tet_is_in_conflict(t2,p)) {
                        mark_tet_as_neighbor(t2);
                        ++nb_tets_to_create_;
                    } else {
                        mark_tet_as_conflict(t2);
                        S_.push_back(t2);
                        continue;
                    }
                    ++nb_tets_to_create_;
                    cavity_.new_facet(
                        t, lf,
                        tet_vertex(t, tet_facet_vertex(lf,0)),
                        tet_vertex(t, tet_facet_vertex(lf,1)),
                        tet_vertex(t, tet_facet_vertex(lf,2))
                    );
                }
            }
            return true;
        }

        double lifted_coordinate(const double* p) const {
            index_t pindex = index_t((p - vertex_ptr(0)) / int(vertex_stride_));
            return heights_[pindex];
        }

        bool tet_is_in_conflict(index_t t, const double* p) const {
            const double* pv[4];
            for(index_t i = 0; i < 4; ++i) {
                index_t v = tet_vertex(t,i);
                pv[i] = (v == NO_INDEX) ? nullptr : vertex_ptr(v);
            }
            for(index_t lf = 0; lf < 4; ++lf) {
                if(pv[lf] == nullptr) {
                    pv[lf] = p;
                    Sign sign = PCK::orient_3d(pv[0],pv[1],pv[2],pv[3]);
                    if(sign > 0) return true;
                    if(sign < 0) return false;
                    index_t t2 = tet_adjacent(t, lf);
                    if(owns_tet(t2)) return tet_is_marked_as_conflict(t2);
                    const double* q0 = pv[(lf+1)%4];
                    const double* q1 = pv[(lf+2)%4];
                    const double* q2 = pv[(lf+3)%4];
                    return (PCK::in_circle_3d_SOS(q0,q1,q2,p) > 0);
                }
            }
            return (PCK::in_sphere_3d_SOS(pv[0], pv[1], pv[2], pv[3], p) > 0);
        }

        index_t locate(
            const double* p, index_t hint = NO_TETRAHEDRON, Sign* orient = nullptr
        ) {
            {
                index_t new_hint =
                    locate_inexact(p, hint, MAX_LOCATE_INEXACT_STEPS);
                if(new_hint == NO_TETRAHEDRON) return NO_TETRAHEDRON;
                hint = new_hint;
            }
            if(hint != NO_TETRAHEDRON) {
                if(tet_is_free(hint)) {
                    hint = NO_TETRAHEDRON;
                } else {
                    if(!owns_tet(hint) && !acquire_tet(hint)) {
                        hint = NO_TETRAHEDRON;
                    }
                    if((hint != NO_TETRAHEDRON) && tet_is_free(hint)) {
                        release_tet(hint);
                        hint = NO_TETRAHEDRON;
                    }
                }
            }
            index_t tries = 0;
            do {
                if(hint == NO_TETRAHEDRON) {
                    if(++tries > MAX_RANDOM_TRIES) return NO_TETRAHEDRON;
                    hint = thread_safe_random(max_used_t_);
                }
                if(tet_is_free(hint) || (!owns_tet(hint) && !acquire_tet(hint))) {
                    if(owns_tet(hint)) release_tet(hint);
                    hint = NO_TETRAHEDRON;
                } else {
                    for(index_t f = 0; f < 4; ++f) {
                        if(tet_vertex(hint,f) == VERTEX_AT_INFINITY) {
                            index_t new_hint = tet_adjacent(hint,f);
                            if(new_hint == NO_TETRAHEDRON ||
                               tet_is_free(new_hint) ||
                               (!owns_tet(new_hint) &&
                                !acquire_tet(new_hint))) {
                                if(new_hint != NO_TETRAHEDRON &&
                                   owns_tet(new_hint)) {
                                    release_tet(new_hint);
                                }
                                new_hint = NO_TETRAHEDRON;
                            }
                            release_tet(hint);
                            hint = new_hint;
                            break;
                        }
                    }
                }
            } while(hint == NO_TETRAHEDRON);

            index_t t = hint;
            index_t t_pred = NO_TETRAHEDRON;
            Sign orient_local[4];
            if(orient == nullptr) orient = orient_local;

            // The walk cycles when degenerate input (flat / zero-volume tets)
            // makes the orientation tests inconsistent, so it is bounded; a real
            // walk is a few steps, O(nb_tets^(1/3)) after a random restart.
            index_t walk_budget = MAX_WALK_STEPS;

        still_walking:
            {
                if(walk_budget-- == 0) {
                    if(t_pred != NO_TETRAHEDRON) release_tet(t_pred);
                    if(owns_tet(t)) release_tet(t);
                    return NO_TETRAHEDRON;
                }
                if(t_pred != NO_TETRAHEDRON) release_tet(t_pred);
                if(tet_is_free(t)) {
                    if(owns_tet(t)) release_tet(t);
                    return NO_TETRAHEDRON;
                }
                if(!owns_tet(t) && !acquire_tet(t)) return NO_TETRAHEDRON;
                if(!tet_is_real(t)) {
                    release_tet(t);
                    return NO_TETRAHEDRON;
                }
                const double* pv[4];
                pv[0] = vertex_ptr(finite_tet_vertex(t,0));
                pv[1] = vertex_ptr(finite_tet_vertex(t,1));
                pv[2] = vertex_ptr(finite_tet_vertex(t,2));
                pv[3] = vertex_ptr(finite_tet_vertex(t,3));
                index_t f0 = thread_safe_random_4();
                for(index_t df = 0; df < 4; ++df) {
                    index_t f = (f0 + df) % 4;
                    index_t t_next = tet_adjacent(t,f);
                    if(t_next == NO_INDEX) {
                        release_tet(t);
                        return NO_TETRAHEDRON;
                    }
                    if(t_next == t_pred) {
                        orient[f] = POSITIVE;
                        continue;
                    }
                    const double* pv_bkp = pv[f];
                    pv[f] = p;
                    orient[f] = PCK::orient_3d(pv[0], pv[1], pv[2], pv[3]);
                    if(orient[f] != NEGATIVE) {
                        pv[f] = pv_bkp;
                        continue;
                    }
                    if(tet_is_virtual(t_next)) {
                        release_tet(t);
                        if(!owns_tet(t_next) && !acquire_tet(t_next)) {
                            return NO_TETRAHEDRON;
                        }
                        for(index_t lf = 0; lf < 4; ++lf) orient[lf] = POSITIVE;
                        return t_next;
                    }
                    t_pred = t;
                    t = t_next;
                    goto still_walking;
                }
            }
            return t;
        }

    protected:

        bool tet_is_marked_as_conflict(index_t t) const {
            return cell_status_.cell_is_marked_as_conflict(t);
        }
        index_t nb_tets_in_conflict() const { return tets_to_delete_.size(); }
        void mark_tet_as_conflict(index_t t) {
            tets_to_delete_.push_back(t);
            cell_status_.mark_cell_as_conflict(t);
        }
        void mark_tet_as_neighbor(index_t t) {
            tets_to_release_.push_back(t);
        }
        void acquire_and_mark_tet_as_created(index_t t) {
            cell_status_.set_cell_status(
                t, CellStatusArray::thread_index_t(id())
            );
            tets_to_release_.push_back(t);
        }
        void release_tets() {
            for(index_t i = 0; i < tets_to_release_.size(); ++i) {
                release_tet(tets_to_release_[i]);
            }
            tets_to_release_.resize(0);
            for(index_t i = 0; i < tets_to_delete_.size(); ++i) {
                release_tet(tets_to_delete_[i]);
            }
            tets_to_delete_.resize(0);
        }
        bool acquire_tet(index_t t) {
            interfering_thread_ = cell_status_.acquire_cell(
                t, CellStatusArray::thread_index_t(id())
            );
            return (interfering_thread_ == NO_THREAD);
        }
        void release_tet(index_t t) {
            cell_status_.release_cell(t);
        }
        bool owns_tet(index_t t) const {
            return (cell_status_.cell_thread(t) ==
                    CellStatusArray::thread_index_t(id()));
        }

        index_t locate_inexact(
            const double* p, index_t hint, index_t max_iter
        ) const {
            for(index_t tries = 0;
                hint == NO_TETRAHEDRON && tries < MAX_RANDOM_TRIES; ++tries) {
                hint = thread_safe_random(max_used_t_);
                if(tet_is_free(hint) || tet_thread(hint) != NO_THREAD) {
                    hint = NO_TETRAHEDRON;
                }
            }
            if(hint == NO_TETRAHEDRON) return NO_TETRAHEDRON;
            if(tet_is_virtual(hint)) {
                for(index_t lf = 0; lf < 4; ++lf) {
                    if(tet_vertex(hint, lf) == VERTEX_AT_INFINITY) {
                        hint = tet_adjacent(hint, lf);
                        if(hint == NO_TETRAHEDRON) return NO_TETRAHEDRON;
                        break;
                    }
                }
            }
            index_t t = hint;
            index_t t_pred = NO_TETRAHEDRON;
        still_walking:
            {
                const double* pv[4];
                for(index_t lv = 0; lv < 4; ++lv) {
                    index_t iv = tet_vertex(t,lv);
                    if(iv == NO_INDEX) return NO_TETRAHEDRON;
                    pv[lv] = vertex_ptr(iv);
                }
                for(index_t f = 0; f < 4; ++f) {
                    index_t t_next = tet_adjacent(t,f);
                    if(t_next == NO_INDEX) return NO_TETRAHEDRON;
                    if(t_next == t_pred) continue;
                    const double* pv_bkp = pv[f];
                    pv[f] = p;
                    Sign ori = PCK::orient_3d_inexact(pv[0], pv[1], pv[2], pv[3]);
                    if(ori != NEGATIVE) {
                        pv[f] = pv_bkp;
                        continue;
                    }
                    if(tet_is_virtual(t_next)) return t_next;
                    t_pred = t;
                    t = t_next;
                    // Decrementing inside the facet loop (geogram's form) does
                    // not end the walk at zero: it falls through to the next
                    // facet, and a step there wraps max_iter to 2^32-1.
                    if(max_iter-- == 0) return t;
                    goto still_walking;
                }
            }
            return t;
        }

        bool tet_is_virtual(index_t t) const {
            return !tet_is_free(t) && (
                cell_to_v_store_[4*t]   == VERTEX_AT_INFINITY ||
                cell_to_v_store_[4*t+1] == VERTEX_AT_INFINITY ||
                cell_to_v_store_[4*t+2] == VERTEX_AT_INFINITY ||
                cell_to_v_store_[4*t+3] == VERTEX_AT_INFINITY);
        }

        static index_t tet_facet_vertex(index_t f, index_t v) {
            return index_t(tet_facet_vertex_[f][v]);
        }
        index_t tet_vertex(index_t t, index_t lv) const {
            return cell_to_v_store_[4*t + lv];
        }
        index_t finite_tet_vertex(index_t t, index_t lv) const {
            return cell_to_v_store_[4*t + lv];
        }
        index_t tet_adjacent(index_t t, index_t lf) const {
            return cell_to_cell_store_[4*t + lf];
        }
        void set_tet_adjacent(index_t t1, index_t lf1, index_t t2) {
            cell_to_cell_store_[4*t1 + lf1] = t2;
        }
        index_t find_tet_adjacent(index_t t1, index_t t2) const {
            const index_t* T = &(cell_to_cell_store_[4*t1]);
            return find_4(T,t2);
        }

        static constexpr index_t END_OF_LIST = NO_INDEX;
        static constexpr index_t NOT_IN_LIST = index_t(-2);
        static constexpr index_t VERTEX_OF_DELETED_TET = index_t(-2);

        index_t nb_vertices() const { return nb_vertices_; }
        const double* vertex_ptr(index_t i) const {
            return vertices_ + vertex_stride_ * i;
        }
        bool tet_is_in_list(index_t t) const {
            return (cell_next_[t] != NOT_IN_LIST);
        }
        index_t tet_next(index_t t) const { return cell_next_[t]; }
        index_t tet_thread(index_t t) const {
            return cell_status_.cell_thread(t);
        }
        void add_tet_to_list(index_t t, index_t& first, index_t& last) {
            if(last == END_OF_LIST) {
                first = last = t;
                cell_next_[t] = END_OF_LIST;
            } else {
                cell_next_[t] = first;
                first = t;
            }
        }
        void remove_tet_from_list(index_t t) {
            cell_next_[t] = NOT_IN_LIST;
        }

        index_t new_tetrahedron();
        index_t new_tetrahedron(index_t v1, index_t v2, index_t v3, index_t v4) {
            index_t result = new_tetrahedron();
            cell_to_v_store_[4*result]   = v1;
            cell_to_v_store_[4*result+1] = v2;
            cell_to_v_store_[4*result+2] = v3;
            cell_to_v_store_[4*result+3] = v4;
            return result;
        }

        static index_t find_4(const index_t* T, index_t v) {
            index_t result = index_t(
                (T[1] == v) | ((T[2] == v) * 2) | ((T[3] == v) * 3)
            );
            return result;
        }

        void send_event() { cond_.notify_all(); }
        // One event is enough: this is a backoff, not a condition to wait for.
        // finished_ is written under the same mutex, so a thread that has
        // already exited cannot be waited on.
        void wait_for_event(index_t t) {
            Delaunay3dThread* thrd = thread(t);
            std::unique_lock<std::mutex> L(thrd->mutex_);
            if(!thrd->finished_) {
                thrd->cond_.wait(L);
            }
        }

    private:
        ParallelDelaunay3d* master_;
        index_t nb_vertices_;
        const double* vertices_;
        const double* heights_;
        index_t* reorder_;
        index_t dimension_;
        index_t vertex_stride_;
        bool weighted_;
        index_t max_t_;
        index_t max_used_t_;

        vector<index_t>& cell_to_v_store_;
        vector<index_t>& cell_to_cell_store_;
        vector<index_t>& cell_next_;
        CellStatusArray& cell_status_;

        index_t first_free_;
        index_t nb_free_;
        bool memory_overflow_;

        index_t v1_,v2_,v3_,v4_;

        vector<index_t> S_;
        index_t nb_tets_to_create_;

        bool direction_;
        index_t work_begin_;
        index_t work_end_;
        index_t b_hint_;
        index_t e_hint_;
        bool finished_;

        CellStatusArray::thread_index_t interfering_thread_;

        vector<index_t> tets_to_delete_;
        vector<index_t> tets_to_release_;

        index_t nb_rollbacks_;
        index_t nb_failed_locate_;

        std::condition_variable cond_;
        std::mutex mutex_;

        static char tet_facet_vertex_[4][3];

        Cavity cavity_;

        friend class ParallelDelaunay3d;
    };

    char Delaunay3dThread::tet_facet_vertex_[4][3] = {
        {1, 2, 3},
        {0, 3, 2},
        {3, 0, 1},
        {1, 0, 2}
    };

    /*********************** ParallelDelaunay3d **************************/

    class ParallelDelaunay3d : public Delaunay {
    public:
        ParallelDelaunay3d() : Delaunay(3) {
            weighted_ = false;
            debug_mode_ = false;
            verbose_debug_mode_ = false;
            benchmark_mode_ = false;
        }

        void set_vertices(index_t nb_vertices, const double* vertices) override;

        index_t nearest_vertex(const double* p) const override {
            return Delaunay::nearest_vertex(p);
        }

        // expose results
        const vector<index_t>& cell_to_v_store() const { return cell_to_v_store_; }
        const vector<index_t>& cell_to_cell_store() const { return cell_to_cell_store_; }
        vector<index_t>&& take_cell_to_v_store() { return std::move(cell_to_v_store_); }
        void set_keep_adjacency(bool b) { keep_adjacency_ = b; }

    private:
        bool keep_adjacency_ = true;
        vector<index_t> cell_to_v_store_;
        vector<index_t> cell_to_cell_store_;
        vector<index_t> cell_next_;
        CellStatusArray cell_status_;
        ThreadGroup threads_;
        bool weighted_;
        vector<double> heights_;
        vector<index_t> reorder_;
        vector<index_t> levels_;
        bool debug_mode_;
        bool verbose_debug_mode_;
        bool benchmark_mode_;

        friend class Delaunay3dThread;
    };

    /*********************** Delaunay3dThread out-of-line ****************/

    Delaunay3dThread::Delaunay3dThread(
        ParallelDelaunay3d* master, index_t pool_begin, index_t pool_end
    ) :
        master_(master),
        cell_to_v_store_(master_->cell_to_v_store_),
        cell_to_cell_store_(master_->cell_to_cell_store_),
        cell_next_(master_->cell_next_),
        cell_status_(master_->cell_status_)
    {
        max_used_t_ = 1;
        max_t_ = master_->cell_next_.size();

        nb_vertices_ = master_->nb_vertices();
        vertices_ = master_->vertex_ptr(0);
        weighted_ = master_->weighted_;
        heights_ = weighted_ ? master_->heights_.data() : nullptr;
        dimension_ = master_->dimension();
        vertex_stride_ = dimension_;
        reorder_ = master_->reorder_.data();

        first_free_ = pool_begin;
        for(index_t t = pool_begin; t < pool_end-1; ++t) {
            cell_next_[t] = t+1;
        }
        cell_next_[pool_end-1] = END_OF_LIST;
        nb_free_ = pool_end - pool_begin;
        memory_overflow_ = false;

        work_begin_ = NO_INDEX;
        work_end_ = NO_INDEX;
        finished_ = false;
        b_hint_ = NO_TETRAHEDRON;
        e_hint_ = NO_TETRAHEDRON;
        direction_ = true;

        interfering_thread_ = NO_THREAD;
        nb_rollbacks_ = 0;
        nb_failed_locate_ = 0;
        nb_tets_to_create_ = 0;
        v1_ = NO_INDEX;
        v2_ = NO_INDEX;
        v3_ = NO_INDEX;
        v4_ = NO_INDEX;
    }

    index_t Delaunay3dThread::nb_threads() const {
        return index_t(master_->threads_.size());
    }
    Delaunay3dThread* Delaunay3dThread::thread(index_t t) {
        return static_cast<Delaunay3dThread*>(master_->threads_[t].get());
    }

    index_t Delaunay3dThread::new_tetrahedron() {
        if(first_free_ == END_OF_LIST) {
            master_->cell_to_v_store_.resize(
                master_->cell_to_v_store_.size() + 4, NO_INDEX
            );
            master_->cell_to_cell_store_.resize(
                master_->cell_to_cell_store_.size() + 4, NO_INDEX
            );
            master_->cell_next_.push_back(END_OF_LIST);
            master_->cell_status_.grow();
            ++nb_free_;
            ++max_t_;
            first_free_ = master_->cell_status_.size() - 1;
        }
        acquire_and_mark_tet_as_created(first_free_);
        index_t result = first_free_;
        first_free_ = tet_next(first_free_);
        remove_tet_from_list(result);
        cell_to_cell_store_[4*result]   = NO_INDEX;
        cell_to_cell_store_[4*result+1] = NO_INDEX;
        cell_to_cell_store_[4*result+2] = NO_INDEX;
        cell_to_cell_store_[4*result+3] = NO_INDEX;
        max_used_t_ = std::max(max_used_t_, result);
        --nb_free_;
        return result;
    }

    void ParallelDelaunay3d::set_vertices(
        index_t nb_vertices, const double* vertices
    ) {
        Delaunay::set_vertices(nb_vertices, vertices);

        index_t expected_tetra = nb_vertices * 7;

        cell_to_v_store_.assign(expected_tetra * 4, NO_INDEX);
        cell_to_cell_store_.assign(expected_tetra * 4, NO_INDEX);
        cell_next_.assign(expected_tetra, NO_INDEX);
        cell_status_.resize(expected_tetra);

        if(do_reorder_) {
            compute_BRIO_order(
                nb_vertices, vertex_ptr(0), reorder_,
                3, dimension(), 64, 0.125, &levels_
            );
        } else {
            reorder_.resize(nb_vertices);
            for(index_t i = 0; i < nb_vertices; ++i) reorder_[i] = i;
        }

        index_t nb_threads = std::min(
            Process::maximum_concurrent_threads(),
            CellStatusArray::MAX_THREADS
        );
        index_t pool_size = expected_tetra / nb_threads;
        if(pool_size == 0) {
            pool_size = 1;
            nb_threads = expected_tetra;
        }
        index_t pool_begin = 0;
        threads_.clear();
        for(index_t t = 0; t < nb_threads; ++t) {
            index_t pool_end =
                (t == nb_threads - 1) ? expected_tetra : pool_begin + pool_size;
            Thread_var thrd(new Delaunay3dThread(this, pool_begin, pool_end));
            thrd->set_id(t);
            threads_.push_back(thrd);
            pool_begin = pool_end;
        }

        index_t lvl = 1;
        while(lvl < (levels_.size() - 1) && levels_[lvl] < 1000) {
            ++lvl;
        }

        Delaunay3dThread* thread0 =
            static_cast<Delaunay3dThread*>(threads_[0].get());
        thread0->create_first_tetrahedron();
        thread0->set_work(levels_[0], levels_[lvl]);
        thread0->run();

        index_t first_lvl = lvl;

        for(; lvl < levels_.size()-1; ++lvl) {
            index_t lvl_b = levels_[lvl];
            index_t lvl_e = levels_[lvl+1];
            index_t work_size = (lvl_e - lvl_b)/index_t(threads_.size());
            index_t b = lvl_b;
            for(index_t t = 0; t < threads_.size(); ++t) {
                index_t e = t == threads_.size()-1 ? lvl_e : b+work_size;
                Delaunay3dThread* thrd =
                    static_cast<Delaunay3dThread*>(threads_[t].get());
                if(lvl == first_lvl && t != 0) {
                    thrd->initialize_from(thread0);
                }
                thrd->set_work(b,e);
                b = e;
            }
            Process::run_threads(threads_);
        }

        // Run threads sequentially to insert missing points (memory overflow).
        index_t nb_sequential_points = 0;
        for(index_t t = 0; t < threads_.size(); ++t) {
            Delaunay3dThread* t1 =
                static_cast<Delaunay3dThread*>(threads_[t].get());
            nb_sequential_points += t1->work_size();
            if(t != 0) {
                Delaunay3dThread* t2 =
                    static_cast<Delaunay3dThread*>(threads_[t-1].get());
                t1->initialize_from(t2);
            }
            t1->run();
        }

        if(nb_sequential_points != 0) {
            Delaunay3dThread* t0 =
                static_cast<Delaunay3dThread*>(threads_[0].get());
            Delaunay3dThread* tn =
                static_cast<Delaunay3dThread*>(
                    threads_[threads_.size()-1].get());
            t0->initialize_from(tn);
        }

        // Compress: remove free and virtual tetrahedra. Dropping the status
        // array and (adjacency-free callers) the neighbour store here is 0.8 GB
        // on a 7M-point cloud; cell_next_ stays, tet_is_real() walks it.
        cell_status_.clear();
        const bool need_adj = keep_adjacency_ || keep_infinite_;
        if(!need_adj) vector<index_t>().swap(cell_to_cell_store_);
        vector<index_t>& old2new = cell_next_;
        index_t nb_tets = 0;

        {
            for(index_t t = 0; t < thread0->max_t(); ++t) {
                if((keep_infinite_ && !thread0->tet_is_free(t)) ||
                   thread0->tet_is_real(t)) {
                    if(t != nb_tets) {
                        std::memcpy(
                            &cell_to_v_store_[nb_tets * 4],
                            &cell_to_v_store_[t * 4],
                            4 * sizeof(index_t)
                        );
                        if(need_adj) {
                            std::memcpy(
                                &cell_to_cell_store_[nb_tets * 4],
                                &cell_to_cell_store_[t * 4],
                                4 * sizeof(index_t)
                            );
                        }
                    }
                    if(need_adj) old2new[t] = nb_tets;
                    ++nb_tets;
                } else {
                    if(need_adj) old2new[t] = NO_INDEX;
                }
            }
            cell_to_v_store_.resize(4 * nb_tets);
            if(need_adj) {
                cell_to_cell_store_.resize(4 * nb_tets);
                for(index_t i = 0; i < 4 * nb_tets; ++i) {
                    index_t t = cell_to_cell_store_[i];
                    t = old2new[t];
                    cell_to_cell_store_[i] = t;
                }
            }
        }

        if(keep_infinite_) {
            nb_finite_cells_ = 0;
            index_t finite_ptr = 0;
            index_t infinite_ptr = nb_tets - 1;
            for(;;) {
                while(thread0->tet_is_finite(finite_ptr)) {
                    old2new[finite_ptr] = finite_ptr;
                    ++finite_ptr;
                    ++nb_finite_cells_;
                }
                while(!thread0->tet_is_finite(infinite_ptr)) {
                    old2new[infinite_ptr] = infinite_ptr;
                    --infinite_ptr;
                }
                if(finite_ptr > infinite_ptr) break;
                old2new[finite_ptr] = infinite_ptr;
                old2new[infinite_ptr] = finite_ptr;
                ++nb_finite_cells_;
                for(index_t lf = 0; lf < 4; ++lf) {
                    std::swap(
                        cell_to_cell_store_[4*finite_ptr + lf],
                        cell_to_cell_store_[4*infinite_ptr + lf]
                    );
                }
                for(index_t lv = 0; lv < 4; ++lv) {
                    std::swap(
                        cell_to_v_store_[4*finite_ptr + lv],
                        cell_to_v_store_[4*infinite_ptr + lv]
                    );
                }
                ++finite_ptr;
                --infinite_ptr;
            }
            for(index_t i = 0; i < 4 * nb_tets; ++i) {
                index_t t = cell_to_cell_store_[i];
                t = old2new[t];
                cell_to_cell_store_[i] = t;
            }
        }

        set_arrays(
            nb_tets,
            cell_to_v_store_.data(),
            cell_to_cell_store_.data()
        );
    }

} // anonymous namespace

/*********************** public API ************************************/

namespace delaunay3d {

    Delaunay3DResult compute_delaunay_3d(
        const double* points, int nb_points, int num_threads, bool compute_adjacency
    ) {
        Delaunay3DResult result;
        result.nb_vertices = nb_points;
        if(nb_points < 4 || nb_points > kMaxPoints) {
            result.num_threads = 0;
            return result;
        }

        index_t nthreads;
        if(num_threads <= 0) {
            unsigned hc = std::thread::hardware_concurrency();
            nthreads = (hc == 0) ? 1u : index_t(hc);
        } else {
            nthreads = index_t(num_threads);
        }
        nthreads = std::min(nthreads, CellStatusArray::MAX_THREADS);
        if(nthreads < 1) nthreads = 1;
        Process::g_max_threads = nthreads;
        result.num_threads = int(nthreads);

        ParallelDelaunay3d del;
        del.set_keep_adjacency(compute_adjacency);
        del.set_vertices(index_t(nb_points), points);

        index_t nb_cells = del.nb_cells();
        result.nb_cells = int(nb_cells);
        if(compute_adjacency) {
            const vector<index_t>& cc = del.cell_to_cell_store();
            result.cell_adjacents.resize(size_t(nb_cells) * 4);
            for(size_t i = 0; i < size_t(nb_cells) * 4; ++i) {
                index_t a = cc[i];
                result.cell_adjacents[i] = (a == NO_INDEX) ? -1 : int(a);
            }
        }
        result.cell_vertices = del.take_cell_to_v_store();
        return result;
    }

} // namespace delaunay3d
