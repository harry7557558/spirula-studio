#include "dense/PairSelection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

std::vector<spirula::dense::ImagePair> pairs(const std::vector<spirula::dense::PairImage>& images,
                                            const spirula::dense::PairOptions& options) {
    std::vector<spirula::dense::ImagePair> result;
    spirula::dense::select_pairs(images, options, [&](auto pair) { result.push_back(pair); });
    return result;
}

}  // namespace

int main() {
    try {
        using namespace spirula::dense;
        std::vector<PairImage> images(12);
        for (size_t i = 0; i < images.size(); ++i) {
            images[i].source_image = (int64_t)i;
            images[i].name = std::string(i < 6 ? "a/" : "b/") + std::to_string(i);
            images[i].center = {(double)i, 0, 0};
        }
        PairOptions options;
        options.neighbors = 2;
        auto selected = pairs(images, options);
        std::vector<int> degrees(images.size());
        std::set<ImagePair> unique;
        for (const auto& pair : selected) {
            require(pair.first < pair.second && unique.insert(pair).second, "automatic pairs not canonical/distinct");
            ++degrees[pair.first]; ++degrees[pair.second];
        }
        require(!selected.empty() && *std::max_element(degrees.begin(), degrees.end()) <= 2, "automatic neighbor cap failed");
        require(pairs(images, options) == selected, "automatic pair ordering changed");
        images[0].visible_points = {2, 4, 6}; images[11].visible_points = {2, 4, 6};
        selected = pairs(images, options);
        require(selected.front() == ImagePair(0, 11), "co-visibility did not rank ahead of nearest pose");
        require(shares_points({1, UINT64_MAX}, {2, UINT64_MAX}) && !shares_points({1, 3}, {2, 4}) &&
                !shares_points({}, {1}), "face track intersection failed for sparse identities or empty tracks");
        const auto references = select_references(images,0.8,0);
        auto scaled = images; for (auto& image : scaled) image.center = image.center * 1000 + sfm::Vec3{2e6,3e6,-1e6};
        require(references.size() == 10 && select_references(scaled,0.8,0) == references, "reference coverage depends on scene scale");
        auto directed = options; directed.directed = true; directed.references = references;
        std::vector<int> counts(images.size());
        for (const auto& pair : pairs(images,directed)) ++counts[pair.first];
        for (uint32_t reference : references) require(counts[reference] == 2, "source reference lacks its requested directed neighbors");
        auto permuted = images; std::reverse(permuted.begin(), permuted.end());
        std::vector<std::pair<int64_t, int64_t>> identities;
        for (auto pair : pairs(permuted, options)) identities.push_back(std::minmax(permuted[pair.first].source_image, permuted[pair.second].source_image));
        for (size_t i = 0; i < selected.size(); ++i)
            require(identities[i] == std::make_pair((int64_t)selected[i].first, (int64_t)selected[i].second), "input permutation changed automatic ranking");
        {
            // A ring of cameras around one cloud: wide-angle partners beat near duplicates,
            // faces of one image are never paired, and reference coverage thins a dense capture.
            std::vector<sfm::Vec3> cloud;
            for (int i = 0; i < 200; ++i) cloud.push_back({std::cos(i * 0.7) * 0.2, std::sin(i * 1.3) * 0.2, std::cos(i * 2.1) * 0.2});
            std::vector<PairImage> ring;
            for (int i = 0; i < 360; ++i) {
                const double angle = i * 3.141592653589793 / 180;
                PairImage view;
                view.source_image = i / 2; view.face = i % 2; view.name = std::to_string(i / 2);
                view.center = {4 * std::cos(angle / 2), 4 * std::sin(angle / 2), 0};
                view.forward = (sfm::Vec3{} - view.center).normalized();
                for (uint64_t p = 0; p < cloud.size(); ++p) view.visible_points.push_back(p);
                ring.push_back(view);
            }
            PairOptions automatic; automatic.neighbors = 4;
            std::vector<ImagePair> chosen;
            const auto statistics = select_pairs(ring, automatic, [&](ImagePair pair) { chosen.push_back(pair); }, {}, cloud);
            double narrow = 0;
            for (const auto& pair : chosen) {
                require(ring[pair.first].source_image != ring[pair.second].source_image, "faces of one image were paired");
                const auto a = ring[pair.first].center.normalized(), b = ring[pair.second].center.normalized();
                narrow += std::acos(std::clamp(a.dot(b), -1.0, 1.0)) < 5 * 3.141592653589793 / 180;
            }
            require(statistics.references == ring.size() && narrow < chosen.size() / 4,
                    "neighbours were not chosen for their triangulation angle");
            automatic.reference_coverage = 3;
            const auto spaced = select_pairs(ring, automatic, [](ImagePair) {}, {}, cloud);
            require(spaced.references > 0 && spaced.references < ring.size() / 2 && spaced.emitted < statistics.emitted,
                    "reference coverage did not thin a densely captured ring");
        }
        options.mode = PairMode::Sequential; options.sequence_window = 1;
        selected = pairs(images, options);
        require(selected.size() == 10, "sequence window crossed folders or skipped a neighbor");
        for (auto pair : selected) require((pair.first < 6) == (pair.second < 6), "sequence pair crossed folders");
        options.mode = PairMode::Explicit; options.explicit_pairs = {{1,0}, {0,1}, {2,2}, {3,4}};
        selected = pairs(images, options);
        require(selected == std::vector<ImagePair>{{0,1}, {3,4}}, "explicit pairs not deduplicated");
        images[1].center = images[0].center;
        require(pairs(images, options) == std::vector<ImagePair>{{3,4}}, "zero baseline survived explicit selection");
        options.mode = PairMode::Exhaustive; options.max_pairs = 3;
        require(pairs(images, options).size() == 3, "exhaustive pair cap failed");
        bool cancelled = false;
        try { select_pairs(images, options, [](ImagePair) { throw std::runtime_error("cancel"); }); }
        catch (const std::runtime_error&) { cancelled = true; }
        require(cancelled, "pair callback cancellation was swallowed");
        options.mode = PairMode::Explicit; options.explicit_pairs = {{0,99}};
        bool rejected = false;
        try { pairs(images, options); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "unknown explicit image accepted");
        options.mode = PairMode::Automatic;
        bool stopped = false;
        try { select_pairs(images, options, [](ImagePair) {}, [] { throw std::runtime_error("cancel"); }); }
        catch (const std::runtime_error&) { stopped = true; }
        require(stopped, "automatic ranking did not observe cancellation");
        std::printf("PASS deterministic pairing, co-visibility, triangulation angle, reference coverage, face separation, bounded degree, sequences, explicit/exhaustive modes, baseline and cancellation\n");
        return 0;
    } catch (const std::exception& e) { std::printf("FAIL %s\n", e.what()); return 1; }
}
