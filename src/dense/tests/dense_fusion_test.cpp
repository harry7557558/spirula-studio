#include "core/HardwareThreads.h"
#include "dense/Fusion.h"
#include "dense/DiskTable.h"

#include <chrono>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>

namespace fs = std::filesystem;
using namespace spirula::dense;

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Fixture {
    fs::path root = fs::temp_directory_path() /
        ("spirula-fusion-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { fs::create_directories(root); }
    ~Fixture() { std::error_code error; fs::remove_all(root,error); }
};
Surface point(double x, double y, double z, double radius = 0.03, bool normal = true) {
    Surface p{}; p.point[0] = x; p.point[1] = y; p.point[2] = z;
    p.normal[2] = normal ? 1 : 0; p.radius = radius; p.support = 3; p.color[0] = 0.4f;
    return p;
}
std::vector<Surface> read(const fs::path& path) {
    std::ifstream input(path,std::ios::binary); std::vector<Surface> output; Surface p;
    while (read_disk_record(input,p)) output.push_back(p);
    return output;
}
}

int main() {
    try {
        Fixture fixture;
        const auto source = fixture.root / "input.bin";
        std::vector<Surface> input;
        for (int i = 0; i < 240; ++i) {
            input.push_back(point(10 + 3 * i + 0.99,0.5,0.5));
            auto p = point(10 + 3 * i + 1.01,0.5,0.5); p.support = 5; p.color[0] = 0.8f; input.push_back(p);
        }
        for (int i = 0; i < 90; ++i) input.push_back(point(-10.1 - 0.7 * i / 90,4.5,0.5,0,false));
        input.push_back(point(-0.01,-0.01,-0.01,1,false)); input.push_back(point(0.01,0.01,0.01,1,false));
        input.push_back(point(0.99,10.5,5)); input.push_back(point(1.01,10.5,5.05,0.01));
        input.push_back(point(0.99,20.5,0.5)); auto rotated = point(1.01,20.5,0.5);
        rotated.normal[0] = 1; rotated.normal[2] = 0; input.push_back(rotated);
        input.push_back(point(0,30.5,0.5,1)); input.push_back(point(0.8,30.5,0.5,1)); input.push_back(point(1.2,30.5,0.5,1));
        {
            std::ofstream output(source,std::ios::binary);
            for (const auto& p : input) write_disk_record(output,p);
        }
        std::vector<Surface> baseline;
        for (auto [name,budget,workers] : {std::tuple{"serial",uint64_t(64 * 1024),1u},
            std::tuple{"parallel",uint64_t(64 * 1024),2u},std::tuple{"spilled",uint64_t(4096),2u}}) {
            const auto work = fixture.root / name; fs::create_directories(work);
            const auto output = work / "fused.bin";
            const auto stats = fuse_surfaces(source,output,work,1,budget,workers);
            auto result = read(output);
            require(stats.surfaces == 337 && result.size() == 337,"neighbor fusion duplicated or lost a surface");
            if (std::string(name) == "parallel" && spirula::hardware_threads() > 1)
                require(stats.workers == 2,"independent fusion partitions were not processed in parallel");
            if (baseline.empty()) baseline = result;
            else {
                for (size_t i = 0; i < result.size(); ++i) {
                    const auto& a = baseline[i]; const auto& b = result[i];
                    require(std::memcmp(a.point,b.point,sizeof a.point) == 0 && std::memcmp(a.color,b.color,sizeof a.color) == 0 &&
                        std::memcmp(a.normal,b.normal,sizeof a.normal) == 0 && a.radius == b.radius && a.support == b.support,
                        "fusion output changed with worker count or memory budget");
                }
            }
            uint64_t merged = 0, preserved = 0;
            for (const auto& p : result) {
                if (p.point[0] > 10 && p.point[1] == 0.5) {
                    require(p.support == 8 && std::fabs(std::fmod(p.point[0] - 11,3) - 0.0025) < 1e-10 &&
                        std::fabs(p.color[0] - 0.65f) < 1e-7,"boundary owner or support-weighted color was incorrect");
                    ++merged;
                }
                if (p.point[0] < -10) ++preserved;
            }
            require(merged == 240 && preserved == 90,"large incompatible cell or cross-partition boundaries were lost");
            for (const auto& file : fs::directory_iterator(work))
                require(file.path() == output,"successful fusion retained a sorting, partition or table file");
        }
        bool cancelled = false;
        const auto cancelled_work = fixture.root / "cancelled"; fs::create_directories(cancelled_work);
        std::atomic<unsigned> checks{0};
        try { fuse_surfaces(source,cancelled_work / "fused.bin",cancelled_work,1,4096,2,[&] {
            if (++checks > 3000) throw std::runtime_error("cancelled");
        }); } catch (const std::exception&) { cancelled = true; }
        require(cancelled,"fusion ignored cancellation");
        const auto table = fixture.root / "table.bin";
        {
            std::ofstream output(table,std::ios::binary);
            for (uint64_t i = 0; i < 251; ++i) write_disk_record(output,i);
        }
        {
            DiskTable<uint64_t> values(table,128,true);
            for (uint64_t i = 0; i < 251; ++i) { const auto index = i * 97 % 251; values.set(index,index * index); }
            for (uint64_t i = 0; i < 251; ++i) require(values.get(i) == i * i,"paged table lost a random update");
            values.flush();
        }
        {
            DiskTable<uint64_t> values(table,128);
            for (uint64_t i = 0; i < 251; ++i) require(values.get(i) == i * i,"table eviction did not persist an update");
        }
        std::printf("PASS neighbor-cell fusion, deterministic owners, parallel/spilled partitions, normal/depth boundaries, chain bounds, cancellation and paged tables\n");
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr,"FAIL: %s\n",error.what()); return 1; }
}
