#include "dense/Fusion.h"

#include "core/HardwareThreads.h"
#include "dense/DiskArray.h"
#include "dense/DiskTable.h"

#include <atomic>
#include <future>
#include <limits>
#include <mutex>
#include <thread>
#include <tuple>

namespace spirula::dense {
namespace {
namespace fs = std::filesystem;
using Cell = std::array<int64_t,3>;

Cell cell(const Surface& p) { return {p.cell[0],p.cell[1],p.cell[2]}; }

bool less(const Surface& a, const Surface& b) {
    if (cell(a) != cell(b)) return cell(a) < cell(b);
    return std::tie(a.point[0],a.point[1],a.point[2],a.normal[0],a.normal[1],a.normal[2],a.radius,
                    a.color[0],a.color[1],a.color[2],a.support,a.id) <
           std::tie(b.point[0],b.point[1],b.point[2],b.normal[0],b.normal[1],b.normal[2],b.radius,
                    b.color[0],b.color[1],b.color[2],b.support,b.id);
}

bool compatible(const Surface& a, const Surface& b, double voxel) {
    double distance = 0, na = 0, nb = 0, dot = 0, da = 0, db = 0;
    for (int c = 0; c < 3; ++c) {
        const double delta = a.point[c] - b.point[c];
        distance += delta * delta; na += a.normal[c] * a.normal[c]; nb += b.normal[c] * b.normal[c];
        dot += a.normal[c] * b.normal[c]; da += a.normal[c] * delta; db += b.normal[c] * delta;
    }
    if (distance > voxel * voxel) return false;
    const double tolerance = std::min(a.radius,b.radius);
    if (na < 0.25 || nb < 0.25) return distance <= std::pow(std::min(voxel,tolerance),2);
    return std::fabs(dot) > 0.9 && std::fabs(da) <= tolerance && std::fabs(db) <= tolerance;
}

struct Cluster {
    Surface surface;
    double low[3]{}, high[3]{};
    uint64_t parent = 0;
};

Cluster cluster(const Surface& p) {
    Cluster value{}; value.surface = p;
    for (int c = 0; c < 3; ++c) value.low[c] = value.high[c] = p.point[c];
    return value;
}

bool compatible(const Cluster& a, const Cluster& b, double voxel) {
    if (!compatible(a.surface,b.surface,voxel)) return false;
    double diameter = 0;
    for (int c = 0; c < 3; ++c) {
        const double extent = std::max(a.high[c],b.high[c]) - std::min(a.low[c],b.low[c]);
        diameter += extent * extent;
    }
    return diameter <= voxel * voxel;
}

void merge(Cluster& a, const Cluster& b) {
    const double wa = a.surface.support, wb = b.surface.support, sum = wa + wb;
    if (wb > wa || (wb == wa && b.surface.id < a.surface.id)) a.surface.id = b.surface.id;
    for (int c = 0; c < 3; ++c) {
        a.surface.point[c] = (a.surface.point[c] * wa + b.surface.point[c] * wb) / sum;
        a.surface.color[c] = (float)((a.surface.color[c] * wa + b.surface.color[c] * wb) / sum);
        a.low[c] = std::min(a.low[c],b.low[c]); a.high[c] = std::max(a.high[c],b.high[c]);
    }
    a.surface.support = (uint32_t)std::min(sum,(double)UINT32_MAX);
    a.surface.radius = std::min(a.surface.radius,b.surface.radius);
}

struct Partition { uint64_t begin = 0, end = 0; };
struct CellRange { Cell cell{}; uint64_t begin = 0, end = 0; };
struct Edge { uint64_t a = 0, b = 0; };

template<class Process> unsigned parallel_partitions(DiskArray<Partition>& partitions, unsigned requested,
    uint64_t budget, const std::function<void()>& check, Process process) {
    const unsigned hardware = std::max(1u,spirula::hardware_threads());
    const uint64_t minimum = 8 * (sizeof(Cluster) + sizeof(CellRange) + sizeof(Edge));
    const unsigned workers = (unsigned)std::min<uint64_t>(partitions.size(),std::min<uint64_t>(
        requested ? std::min(requested,hardware) : hardware,std::max<uint64_t>(1,budget / minimum)));
    if (!workers) return 1;
    std::mutex mutex;
    uint64_t next = 0;
    std::atomic<bool> failed{false};
    std::exception_ptr failure;
    auto worker = [&] {
        try {
            auto stopped = [&] {
                if (failed.load()) throw std::runtime_error("dense fusion worker stopped");
                if (check) check();
            };
            for (;;) {
                stopped(); Partition part; uint64_t index;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if (next == partitions.size()) return;
                    index = next++; part = partitions[index];
                }
                process(index,part,budget / workers,stopped);
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex);
            if (!failure) failure = std::current_exception();
            failed.store(true);
        }
    };
    std::vector<std::future<void>> futures;
    for (unsigned i = 1; i < workers; ++i) futures.push_back(std::async(std::launch::async,worker));
    worker(); for (auto& future : futures) future.get();
    if (failure) std::rethrow_exception(failure);
    return workers;
}

fs::path part_file(const fs::path& work, const char* phase, uint64_t index) {
    return work / (std::string("fusion-") + phase + "-" + std::to_string(index) + ".bin");
}

uint64_t root(DiskTable<Cluster>& states, uint64_t index, const std::function<void()>& check) {
    auto owner = index;
    for (;;) {
        if (check) check();
        const auto value = states.get(owner);
        if (value.parent == owner) break;
        owner = value.parent;
    }
    while (index != owner) {
        auto value = states.get(index); const auto next = value.parent;
        value.parent = owner; states.set(index,value); index = next;
    }
    return owner;
}

}  // namespace

FusionStatistics fuse_surfaces(const fs::path& source, const fs::path& destination, const fs::path& work,
    double voxel, uint64_t budget, unsigned requested_workers, const std::function<void()>& check,
    const std::function<void(uint64_t,uint64_t)>& progress) {
    if (!(voxel > 0) || !std::isfinite(voxel)) throw std::runtime_error("invalid dense fusion cell size");
    const auto keyed = work / "fusion-keyed.bin", ordered = work / "fusion-ordered.bin";
    const uint64_t partition_records = std::max<uint64_t>(1,budget / (8 * sizeof(Cluster)));
    uint64_t count = 0;
    // Steps in order with rough shares of the time, reported as permille of the whole.
    enum Step { Key, SortSurfaces, Split, Local, Gather, Neighbours, SortEdges, Merge, kSteps };
    static constexpr uint32_t kWeight[kSteps] = {5, 15, 5, 20, 5, 30, 10, 10};
    std::mutex report_mutex;
    uint32_t reported = 0;
    auto report = [&](Step step, uint64_t done, uint64_t total) {
        uint32_t base = 0;
        for (int i = 0; i < step; ++i) base += kWeight[i];
        const double part = total ? std::min(1.0, (double)done / (double)total) : 1.0;
        const uint32_t permille = base * 10 + (uint32_t)(part * kWeight[step] * 10);
        std::lock_guard<std::mutex> lock(report_mutex);
        if (!progress || permille <= reported) return;
        reported = permille;
        progress(permille, 1000);
    };
    std::error_code size_error;
    const uint64_t source_records = fs::file_size(source,size_error) / sizeof(Surface);
    const uint64_t tick = std::max<uint64_t>(1, source_records / 200);
    {
        std::ifstream input(source,std::ios::binary);
        std::ofstream output(keyed,std::ios::binary | std::ios::trunc);
        if (!input || !output) throw std::runtime_error("cannot open dense fusion surfaces");
        Surface p;
        while (read_disk_record(input,p)) {
            if (check) check();
            if (!p.support || !std::isfinite(p.radius) || p.radius < 0) throw std::runtime_error("invalid dense surface support or radius");
            for (int c = 0; c < 3; ++c) {
                const double k = std::floor(p.point[c] / voxel);
                if (!std::isfinite(k) || k <= (double)INT64_MIN || k >= (double)INT64_MAX ||
                    !std::isfinite(p.normal[c]) || !std::isfinite(p.color[c]))
                    throw std::runtime_error("dense fusion cannot represent this surface");
                p.cell[c] = (int64_t)k;
            }
            write_disk_record(output,p); ++count;
            if (count % tick == 0) report(Key,count,source_records);
        }
        flush_disk_output(output);
    }
    external_sort<Surface>(keyed,ordered,budget / 2,less,check,
        [&](uint64_t done,uint64_t total) { report(SortSurfaces,done,total); });
    fs::remove(keyed);
    DiskArray<Partition> partitions(work / "fusion-partitions.bin",budget / 16);
    {
        std::ifstream input(ordered,std::ios::binary); Surface p; Cell previous{}; uint64_t index = 0, begin = 0;
        while (read_disk_record(input,p)) {
            if (check) check();
            if (index > begin && cell(p) != previous && index - begin >= partition_records) {
                partitions.push_back({begin,index}); begin = index;
            }
            previous = cell(p); ++index;
            if (index % tick == 0) report(Split,index,count);
        }
        if (index > begin) partitions.push_back({begin,index});
    }
    FusionStatistics statistics; statistics.partitions = partitions.size();
    std::atomic<uint64_t> local_done{0}, neighbour_done{0};
    statistics.workers = parallel_partitions(partitions,requested_workers,budget / 2,check,
        [&](uint64_t index,Partition part,uint64_t worker_budget,const auto& stopped) {
            std::ifstream input(ordered,std::ios::binary); input.seekg((std::streamoff)(part.begin * sizeof(Surface)));
            std::ofstream output(part_file(work,"local",index),std::ios::binary | std::ios::trunc);
            DiskArray<Cluster> group(part_file(work,"cell",index),worker_budget / 2);
            auto publish = [&] { for (const auto& value : group) write_disk_record(output,value); group.clear(); };
            for (auto i = part.begin; i < part.end; ++i) {
                stopped(); Surface p; if (!read_disk_record(input,p)) throw std::runtime_error("truncated fusion partition");
                if (!group.empty() && cell(group.front().surface) != cell(p)) publish();
                auto value = cluster(p); bool joined = false;
                for (uint64_t j = 0; j < group.size(); ++j) {
                    stopped(); auto candidate = group[j];
                    if (!compatible(candidate,value,voxel)) continue;
                    merge(candidate,value); group.set(j,candidate); joined = true; break;
                }
                if (!joined) group.push_back(value);
                if ((i + 1 - part.begin) % 4096 == 0) report(Local,local_done += 4096,count);
            }
            publish();
            flush_disk_output(output);
        });
    const auto clusters = work / "fusion-clusters.bin", cells = work / "fusion-cells.bin";
    uint64_t cluster_count = 0, cell_count = 0;
    {
        std::ofstream output(clusters,std::ios::binary | std::ios::trunc), index(cells,std::ios::binary | std::ios::trunc);
        CellRange range;
        for (uint64_t i = 0; i < partitions.size(); ++i) {
            if (check) check();
            report(Gather,i,partitions.size());
            {
                std::ifstream input(part_file(work,"local",i),std::ios::binary); Cluster value;
                while (read_disk_record(input,value)) {
                    if (cluster_count && range.cell != cell(value.surface)) {
                        write_disk_record(index,range); ++cell_count; range.begin = cluster_count;
                    }
                    range.cell = cell(value.surface); range.end = cluster_count + 1;
                    value.parent = cluster_count++; write_disk_record(output,value);
                }
            }
            fs::remove(part_file(work,"local",i));
        }
        if (cluster_count) { write_disk_record(index,range); ++cell_count; }
        flush_disk_output(output); flush_disk_output(index);
    }
    partitions.clear(); fs::remove(ordered);
    for (uint64_t i = 0; i < cell_count; i += partition_records)
        partitions.push_back({i,i + std::min(partition_records,cell_count - i)});
    statistics.partitions = std::max(statistics.partitions,partitions.size());
    statistics.workers = std::max(statistics.workers,parallel_partitions(partitions,requested_workers,budget / 2,check,
        [&](uint64_t index,Partition part,uint64_t worker_budget,const auto& stopped) {
            DiskTable<CellRange> cell_index(cells,worker_budget / 4);
            DiskTable<Cluster> points(clusters,worker_budget / 2);
            std::ofstream output(part_file(work,"edges",index),std::ios::binary | std::ios::trunc);
            for (auto i = part.begin; i < part.end; ++i) {
                stopped(); const auto core = cell_index.get(i);
                for (int x = -1; x <= 1; ++x) for (int y = -1; y <= 1; ++y) for (int z = -1; z <= 1; ++z) {
                    Cell key{core.cell[0] + x,core.cell[1] + y,core.cell[2] + z};
                    if (key < core.cell) continue;
                    uint64_t lo = i, hi = cell_count;
                    while (lo < hi) {
                        const auto mid = lo + (hi - lo) / 2;
                        if (cell_index.get(mid).cell < key) lo = mid + 1; else hi = mid;
                    }
                    if (lo == cell_count) continue;
                    const auto halo = cell_index.get(lo); if (halo.cell != key) continue;
                    for (auto a = core.begin; a < core.end; ++a) {
                        const auto pa = points.get(a);
                        for (auto b = std::max(halo.begin,a + 1); b < halo.end; ++b) {
                            stopped(); const auto pb = points.get(b);
                            if (compatible(pa,pb,voxel)) write_disk_record(output,Edge{a,b});
                        }
                    }
                }
                if ((i + 1 - part.begin) % 256 == 0) report(Neighbours,neighbour_done += 256,cell_count);
            }
            flush_disk_output(output);
        }));
    const auto edges = work / "fusion-edges.bin", sorted_edges = work / "fusion-edges-ordered.bin";
    {
        std::ofstream output(edges,std::ios::binary | std::ios::trunc);
        for (uint64_t i = 0; i < partitions.size(); ++i) {
            {
                std::ifstream input(part_file(work,"edges",i),std::ios::binary); Edge value;
                while (read_disk_record(input,value)) { if (check) check(); write_disk_record(output,value); ++statistics.boundary_candidates; }
            }
            fs::remove(part_file(work,"edges",i));
        }
        flush_disk_output(output);
    }
    partitions.clear();
    external_sort<Edge>(edges,sorted_edges,budget / 2,[](const auto& a,const auto& b) {
        return std::tie(a.a,a.b) < std::tie(b.a,b.b);
    },check,[&](uint64_t done,uint64_t total) { report(SortEdges,done,total); }); fs::remove(edges);
    {
        DiskTable<Cluster> states(clusters,budget / 2,true);
        std::ifstream input(sorted_edges,std::ios::binary); Edge edge;
        const uint64_t edge_tick = std::max<uint64_t>(1,statistics.boundary_candidates / 100);
        uint64_t merged = 0;
        while (read_disk_record(input,edge)) {
            if (check) check();
            if (++merged % edge_tick == 0) report(Merge,merged,2 * statistics.boundary_candidates);
            auto a = root(states,edge.a,check), b = root(states,edge.b,check);
            if (a == b) continue;
            if (a > b) std::swap(a,b);
            auto va = states.get(a), vb = states.get(b);
            if (!compatible(va,vb,voxel)) continue;
            merge(va,vb); vb.parent = a; states.set(a,va); states.set(b,vb);
        }
        std::ofstream output(destination,std::ios::binary | std::ios::trunc);
        for (uint64_t i = 0; i < cluster_count; ++i) {
            if (check) check();
            const auto value = states.get(i);
            if (value.parent == i) { write_disk_record(output,value.surface); ++statistics.surfaces; }
            if ((i + 1) % partition_records == 0)
                report(Merge,statistics.boundary_candidates + (i + 1) * statistics.boundary_candidates / std::max<uint64_t>(1,cluster_count),
                       2 * statistics.boundary_candidates);
        }
        flush_disk_output(output);
        states.flush();
    }
    fs::remove(sorted_edges); fs::remove(clusters); fs::remove(cells);
    if (progress) progress(1000,1000);
    return statistics;
}

}  // namespace spirula::dense
