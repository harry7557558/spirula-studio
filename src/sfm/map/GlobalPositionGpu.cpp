#include "sfm/map/GlobalPositionGpu.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

#include "sfm/vk/EmbeddedSpirv.h"
#include "sfm/vk/VkContext.h"

namespace sfm {
namespace {

struct UInt2 { uint32_t x, y; };
struct Float4 { float x, y, z, w; };
struct DispatchCommand { uint32_t x, y, z, pad; };

class PositionGpu {
public:
    PositionGpu(const GlobalPositionProblem& p, int device, const std::string& selector)
        : nc_((uint32_t)p.cameras.size()), np_((uint32_t)p.points.size()),
          no_((uint32_t)p.observations.size()), nodes_(nc_ + np_),
          npr_((uint32_t)p.position_priors.size()), dim_(3 * nodes_), var_dim_(dim_ + no_),
          metric_scale_(p.metric_scale), free_camera_gauge_(!p.position_priors.empty()) {
        ctx_.init(deviceOnlyOpt(device, selector));
        std::vector<UInt2> obs(no_);
        std::vector<Float4> rays((size_t)no_ * 2);
        std::vector<uint32_t> ranges((size_t)nodes_ + 1, 0);
        std::vector<uint32_t> prior_cameras(std::max(npr_, 1u));
        std::vector<Float4> priors((size_t)std::max(npr_, 1u) * 4);
        std::vector<uint32_t> prior_ranges((size_t)nodes_ + 1, 0);
        std::vector<float> vars(var_dim_);
        for (uint32_t i = 0; i < nc_; i++) put(vars, 3 * i, p.cameras[i]);
        for (uint32_t i = 0; i < np_; i++) put(vars, 3 * (nc_ + i), p.points[i]);
        for (uint32_t i = 0; i < no_; i++) {
            const auto& o = p.observations[i];
            obs[i] = {o.camera, o.point};
            rays[2 * (size_t)i] = {(float)o.ray.x, (float)o.ray.y, (float)o.ray.z, (float)o.weight};
            rays[2 * (size_t)i + 1] = {(float)o.camera_offset.x, (float)o.camera_offset.y,
                                       (float)o.camera_offset.z, 0};
            if (!(rays[2 * (size_t)i].w > 0) || !std::isfinite(rays[2 * (size_t)i].w))
                throw std::invalid_argument("global positioning: observation weight exceeds GPU precision");
            const Float4 offset = rays[2 * (size_t)i + 1];
            if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z))
                throw std::invalid_argument("global positioning: rig offset exceeds GPU precision");
            vars[3 * nodes_ + i] = (float)p.log_scales[i];
            ranges[o.camera + 1]++;
            ranges[nc_ + o.point + 1]++;
        }
        for (uint32_t i = 0; i < npr_; i++) {
            const GlobalPositionPrior& q = p.position_priors[i];
            const Mat3 J = globalPositionPriorJacobian(q);
            const Vec3 offset = mul(J, q.camera_offset);
            const Vec3 target = {q.sigma.x > 0 ? q.b.x / q.sigma.x - offset.x : 0,
                                 q.sigma.y > 0 ? q.b.y / q.sigma.y - offset.y : 0,
                                 q.sigma.z > 0 ? q.b.z / q.sigma.z - offset.z : 0};
            prior_cameras[i] = q.camera;
            for (uint32_t row = 0; row < 3; row++) {
                const double target_row = row == 0 ? target.x : row == 1 ? target.y : target.z;
                priors[4 * (size_t)i + row] = {(float)J[3 * row], (float)J[3 * row + 1],
                                                (float)J[3 * row + 2], (float)target_row};
            }
            priors[4 * (size_t)i + 3] = {(float)q.cauchy, 0, 0, 0};
            for (uint32_t row = 0; row < 4; row++) {
                const Float4 v = priors[4 * (size_t)i + row];
                if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
                    !std::isfinite(v.w))
                    throw std::invalid_argument("global positioning: position prior exceeds GPU precision");
            }
            prior_ranges[q.camera + 1]++;
        }
        for (float value : vars)
            if (!std::isfinite(value))
                throw std::invalid_argument("global positioning: parameter exceeds GPU precision");
        std::partial_sum(ranges.begin(), ranges.end(), ranges.begin());
        std::vector<uint32_t> cursor = ranges, adjacency((size_t)no_ * 2);
        for (uint32_t i = 0; i < no_; i++) {
            adjacency[cursor[obs[i].x]++] = i;
            adjacency[cursor[nc_ + obs[i].y]++] = i;
        }
        std::partial_sum(prior_ranges.begin(), prior_ranges.end(), prior_ranges.begin());
        std::vector<uint32_t> prior_cursor = prior_ranges,
                              prior_adjacency(std::max(npr_, 1u));
        for (uint32_t i = 0; i < npr_; i++) prior_adjacency[prior_cursor[prior_cameras[i]]++] = i;
        const uint32_t reductions = groups(std::max({dim_, no_, npr_}));
        std::vector<VkDeviceSize> bytes = {
            (VkDeviceSize)no_ * sizeof(UInt2), (VkDeviceSize)no_ * 2 * sizeof(Float4),
            (VkDeviceSize)var_dim_ * 4, (VkDeviceSize)no_ * sizeof(Float4),
            (VkDeviceSize)no_ * sizeof(Float4), (VkDeviceSize)(nodes_ + 1) * 4,
            (VkDeviceSize)no_ * 8, (VkDeviceSize)dim_ * 8 * 4,
            (VkDeviceSize)reductions * sizeof(Float4), 2 * sizeof(Float4),
            6 * sizeof(DispatchCommand), (VkDeviceSize)std::max(npr_, 1u) * 4,
            (VkDeviceSize)std::max(npr_, 1u) * 4 * sizeof(Float4),
            (VkDeviceSize)(nodes_ + 1) * 4, (VkDeviceSize)std::max(npr_, 1u) * 4};
        std::vector<VkBuffer> handles;
        for (size_t i = 0; i < bytes.size(); i++) {
            const VkDeviceSize size = bytes[i];
            if (size > ctx_.maxBufferBytes())
                throw std::length_error("global positioning: GPU buffer exceeds device limit");
            const VkBufferUsageFlags usage = i + 1 == bytes.size() ? VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT : 0;
            buffers_.push_back(ctx_.createBuffer(size, usage));
            handles.push_back(buffers_.back().buf);
        }
        ctx_.createDescriptors(handles);
        size_t words = 0;
        const uint32_t* shader = findSpirv("global_position", &words);
        if (!shader) throw std::runtime_error("global positioning: shader is unavailable");
        ctx_.loadPipelines(shader, words * 4,
            {"gp_linearize", "gp_nodes", "gp_points", "gp_scale_update", "gp_vector", "gp_reduce",
             "gp_cg_scalar"});
        std::vector<VkContext::UploadItem> uploads = {
            {&buffers_[0], obs.data(), bytes[0]}, {&buffers_[1], rays.data(), bytes[1]},
            {&buffers_[2], vars.data(), bytes[2]}, {&buffers_[5], ranges.data(), bytes[5]},
            {&buffers_[6], adjacency.data(), bytes[6]}, {&buffers_[13], prior_ranges.data(), bytes[13]}};
        if (npr_) {
            uploads.push_back({&buffers_[11], prior_cameras.data(), bytes[11]});
            uploads.push_back({&buffers_[12], priors.data(), bytes[12]});
            uploads.push_back({&buffers_[14], prior_adjacency.data(), bytes[14]});
        }
        ctx_.uploadMany(uploads.data(), uploads.size());
    }

    GlobalPositionResult solve(GlobalPositionProblem& p, const GlobalPositionOptions& opt) {
        GlobalPositionResult result;
        double cost = reduce(2, std::max(no_, npr_), (float)opt.huber).x;
        if (!std::isfinite(cost)) throw std::runtime_error("global positioning: non-finite GPU cost");
        result.initial_cost = cost;
        double damping = opt.initial_damping;
        int small_steps = 0;
        for (int iteration = 0; iteration < opt.max_iterations; iteration++) {
            cancel::check();
            VkCommandBuffer cb = ctx_.begin();
            Push push = base(); push.f0 = (float)opt.huber;
            dispatch(cb, "gp_linearize", groups(no_), push);
            ctx_.barrier(cb);
            push.u3 = 0; push.f0 = (float)damping;
            dispatch(cb, "gp_nodes", nc_, push);
            dispatch(cb, "gp_points", groups(np_), push);
            ctx_.barrier(cb);
            push.f0 = (float)damping;
            dispatch(cb, "gp_vector", groups(dim_), push);
            ctx_.barrier(cb);
            push = base(); push.u3 = 0;
            dispatch(cb, "gp_reduce", groups(dim_), push);
            ctx_.barrier(cb);
            push.f0 = (float)opt.cg_tolerance;
            dispatch(cb, "gp_cg_scalar", 1, push);
            ctx_.barrier(cb);
            ctx_.indirectBarrier(cb);
            for (int cg = 0; cg < opt.cg_iterations; cg++) {
                push = base(); push.u3 = 1; push.f0 = (float)damping;
                dispatchIndirect(cb, "gp_nodes", 0, push);
                dispatchIndirect(cb, "gp_points", 1, push);
                ctx_.barrier(cb);
                push = base(); push.u3 = 1; push.u5 = 1;
                dispatchIndirect(cb, "gp_reduce", 2, push);
                ctx_.barrier(cb);
                dispatch(cb, "gp_cg_scalar", 1, push);
                ctx_.barrier(cb);
                ctx_.indirectBarrier(cb);
                push = base(); push.u3 = 1; push.f0 = (float)damping;
                dispatchIndirect(cb, "gp_vector", 3, push);
                ctx_.barrier(cb);
                push = base(); push.u3 = 0; push.u5 = 1;
                dispatchIndirect(cb, "gp_reduce", 4, push);
                ctx_.barrier(cb);
                push.u3 = 2;
                dispatch(cb, "gp_cg_scalar", 1, push);
                ctx_.barrier(cb);
                ctx_.indirectBarrier(cb);
                dispatchIndirect(cb, "gp_vector", 5, push);
                ctx_.barrier(cb);
            }
            push = base(); push.u3 = 3;
            dispatch(cb, "gp_vector", groups(dim_), push);
            push.u3 = 0; push.f0 = (float)damping;
            dispatch(cb, "gp_scale_update", groups(no_), push);
            ctx_.barrier(cb);
            const Float4 candidate = reduce(cb, 3, std::max(no_, npr_), (float)opt.huber);
            const double next_cost = candidate.x;
            if (candidate.y != 0) {
                result.converged = candidate.y == 1;
                break;
            }
            result.iterations = iteration + 1;
            if (std::isfinite(next_cost) && next_cost < cost) {
                const double change = (cost - next_cost) / std::max(cost, 1e-30);
                cb = ctx_.begin();
                push = base(); push.u3 = 4;
                dispatch(cb, "gp_vector", groups(dim_), push);
                push.u3 = 1;
                dispatch(cb, "gp_scale_update", groups(no_), push);
                ctx_.barrier(cb);
                ctx_.submit(cb);
                cost = next_cost;
                result.accepted++;
                damping = std::max(1e-12, damping * 0.5);
                small_steps = change < std::max(opt.relative_tolerance, 1e-6)
                                  ? small_steps + 1 : 0;
                if (small_steps >= 3) {
                    result.converged = true;
                    break;
                }
            } else {
                small_steps = 0;
                damping = std::min(1e12, damping * 4.0);
            }
        }
        std::vector<float> vars(var_dim_);
        ctx_.download(buffers_[2], vars.data(), (VkDeviceSize)var_dim_ * 4);
        for (uint32_t i = 0; i < nc_; i++) p.cameras[i] = get(vars, 3 * i);
        for (uint32_t i = 0; i < np_; i++) p.points[i] = get(vars, 3 * (nc_ + i));
        for (uint32_t i = 0; i < no_; i++) p.log_scales[i] = vars[3 * nodes_ + i];
        result.final_cost = globalPositionCost(p, opt.huber);
        return result;
    }

private:
    static uint32_t groups(uint32_t n) { return (uint32_t)(((uint64_t)n + 63) / 64); }
    static void put(std::vector<float>& v, uint32_t i, const Vec3& x) {
        v[i] = (float)x.x; v[i + 1] = (float)x.y; v[i + 2] = (float)x.z;
    }
    static Vec3 get(const std::vector<float>& v, uint32_t i) {
        return {v[i], v[i + 1], v[i + 2]};
    }
    Push base() const {
        Push p;
        p.u0 = nc_; p.u1 = np_; p.u2 = no_; p.u4 = metric_scale_ ? 1 : 0;
        p.u6 = free_camera_gauge_ ? 1 : 0; p.u7 = npr_;
        return p;
    }
    void dispatch(VkCommandBuffer cb, const char* entry, uint32_t count, const Push& p) {
        const uint32_t x = std::min(count, 65535u);
        ctx_.dispatch(cb, entry, x, p, (count + 65534u) / 65535u);
    }
    void dispatchIndirect(VkCommandBuffer cb, const char* entry, uint32_t slot, const Push& p) {
        ctx_.dispatchIndirect(cb, entry, buffers_[10], slot * sizeof(DispatchCommand), p);
    }
    Float4 reduce(uint32_t mode, uint32_t count, float parameter) {
        return reduce(ctx_.begin(), mode, count, parameter);
    }
    Float4 reduce(VkCommandBuffer cb, uint32_t mode, uint32_t count, float parameter) {
        Push p = base(); p.u3 = mode; p.f0 = parameter;
        const uint32_t n = groups(count);
        dispatch(cb, "gp_reduce", n, p);
        ctx_.barrier(cb);
        const VkDeviceSize bytes = (VkDeviceSize)n * sizeof(Float4);
        std::vector<Float4> parts(n);
        if (bytes <= VkContext::stagingCapacity()) {
            ctx_.recordDownload(cb, buffers_[8], bytes);
            ctx_.submit(cb);
            const auto* src = static_cast<const Float4*>(ctx_.stagingDownloadPtr());
            std::copy(src, src + n, parts.begin());
        } else {
            ctx_.submit(cb);
            ctx_.download(buffers_[8], parts.data(), bytes);
        }
        double x = 0, y = 0, z = 0, w = 0;
        for (const auto& part : parts) { x += part.x; y += part.y; z += part.z; w += part.w; }
        return {(float)x, (float)y, (float)z, (float)w};
    }
    VkContext ctx_;
    std::vector<GpuBuffer> buffers_;
    uint32_t nc_, np_, no_, nodes_, npr_, dim_, var_dim_;
    bool metric_scale_, free_camera_gauge_;
};

}  // namespace

GlobalPositionResult solveGlobalPositionGpu(GlobalPositionProblem& p,
                                            const GlobalPositionOptions& options,
                                            int device, const std::string& selector) {
    validateGlobalPosition(p, options);
    const float huber = (float)options.huber, damping = (float)options.initial_damping;
    const float cg_tolerance = (float)options.cg_tolerance;
    if (!(huber > 0) || !std::isfinite(huber) || !(damping > 0) || !std::isfinite(damping) ||
        !(cg_tolerance > 0) || !std::isfinite(cg_tolerance))
        throw std::invalid_argument("global positioning: options exceed GPU precision");
    const uint64_t nodes = p.cameras.size() + p.points.size();
    const uint64_t dim = 3 * nodes;
    if (p.observations.size() > UINT32_MAX / 2 || nodes > UINT32_MAX ||
        dim > UINT32_MAX / 8 || dim + p.observations.size() > UINT32_MAX)
        throw std::length_error("global positioning: GPU problem exceeds indexing limit");
    PositionGpu solver(p, device, selector);
    return solver.solve(p, options);
}

}  // namespace sfm
