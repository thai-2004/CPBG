/**
 * @file cpbg_v2.hpp
 * @brief CPBG with Progress Factor (Directional + Efficiency)
 * @version 2.0.0
 *
 * New Formula:
 * Confidence = vonMises(θ) × Sigmoid( λ × (Δh / Cost_hybrid) )
 *
 * Where:
 * - Δh = h(u,T) - h(v,T): Tiến độ thực tế về phía đích (giảm heuristic)
 * - Cost_hybrid = α·cost + (1-α)·time: Chi phí phải trả
 * - λ: Hệ số nhạy với hiệu quả (efficiency sensitivity)
 */

#ifndef CPBG_V2_HPP
#define CPBG_V2_HPP

#include <bits/stdc++.h>
#include <cmath>
#include <chrono>

namespace cpbg
{

    // ============================================================================
    // CONFIGURATION (Updated)
    // ============================================================================

    struct Config
    {
        // Directional parameters
        double sigma_theta = 0.5; // Độ lệch chuẩn cho góc (radians)

        // Progress Factor parameters (NEW)
        double lambda_efficiency = 2.0; // Hệ số nhạy của sigmoid (càng cao thì càng kén chọn đường hiệu quả)

        // Beam parameters
        uint32_t initial_beam_width = 3;
        uint32_t max_beam_width = 20;
        uint32_t stuck_threshold = 10;

        // Approximation
        double epsilon = 0.05;
        double min_confidence = 0.05; // Ngưỡng tối thiểu để giữ lại

        // Lookahead
        uint32_t lookahead_depth = 2;
        uint32_t lookahead_branch = 2;

        // Performance
        uint32_t time_limit_ms = 1000;
        bool enable_fallback = true;
    };

    struct QueryStats
    {
        uint64_t nodes_expanded = 0;
        uint64_t labels_generated = 0;
        uint64_t frontier_pruned = 0;
        uint64_t progress_penalized = 0; // NEW: Số node bị phạt do kém hiệu quả
        double query_time_ms = 0.0;
        double path_cost = 0.0;
        double path_time = 0.0;
        double efficiency_ratio = 0.0; // NEW: Hiệu suất trung bình của đường đi
        bool used_fallback = false;
        bool success = false;
    };

    // ============================================================================
    // GEOMETRY & MATH UTILS (Updated)
    // ============================================================================

    struct Vec2
    {
        double x, y;
        Vec2() : x(0), y(0) {}
        Vec2(double _x, double _y) : x(_x), y(_y) {}

        Vec2 operator-(const Vec2 &o) const { return Vec2(x - o.x, y - o.y); }
        double dot(const Vec2 &o) const { return x * o.x + y * o.y; }
        double norm() const { return std::sqrt(x * x + y * y); }

        double angleTo(const Vec2 &o) const
        {
            double n1 = norm(), n2 = o.norm();
            if (n1 < 1e-9 || n2 < 1e-9)
                return 0;
            double cos_val = dot(o) / (n1 * n2);
            cos_val = std::max(-1.0, std::min(1.0, cos_val));
            return std::acos(cos_val);
        }
    };

    // NEW: Sigmoid function for progress factor
    // Maps (-∞, +∞) -> (0, 1)
    // x > 0: Progress is good (approaching target)
    // x < 0: Progress is bad (moving away)
    inline double sigmoid(double x)
    {
        // Stable sigmoid implementation
        if (x >= 0)
        {
            return 1.0 / (1.0 + std::exp(-x));
        }
        else
        {
            double z = std::exp(x);
            return z / (1.0 + z);
        }
    }

    // ============================================================================
    // DATA STRUCTURES
    // ============================================================================

    struct Edge
    {
        uint32_t to;
        float cost;
        float time;
        Edge(uint32_t _t, float _c, float _tme) : to(_t), cost(_c), time(_tme) {}
    };

    struct Label
    {
        uint32_t node;
        double g_cost, g_time;
        double f_score;
        double confidence;
        double efficiency; // NEW: Lưu lại hiệu suất để thống kê
        uint32_t parent;
        uint16_t depth;
        bool direction;
        bool operator>(const Label &o) const { return f_score > o.f_score; }
    };

    struct FrontierEntry
    {
        double cost, time;
        uint32_t label_idx;
        bool dominates(const FrontierEntry &o, double eps) const
        {
            return (cost <= o.cost * (1 + eps) && time <= o.time * (1 + eps)) &&
                   (cost < o.cost || time < o.time);
        }
    };

    // ============================================================================
    // MAIN ROUTER CLASS (Updated)
    // ============================================================================

    class Router
    {
    private:
        std::vector<std::vector<Edge>> adj_;
        std::vector<Vec2> coords_;
        size_t n_nodes_;
        Config cfg_;
        double alpha_ = 0.5; // NEW: Store alpha for current query

        struct Context
        {
            std::vector<std::vector<FrontierEntry>> fwd_frontier, bwd_frontier;
            std::vector<Label> pool_fwd, pool_bwd;
            uint8_t ts = 1;
        } ctx_;

        QueryStats last_stats_;

    public:
        explicit Router(size_t n) : n_nodes_(n)
        {
            adj_.resize(n);
            coords_.resize(n);
            ctx_.fwd_frontier.resize(n);
            ctx_.bwd_frontier.resize(n);
        }

        void setCoord(uint32_t node, double x, double y) { coords_[node] = Vec2(x, y); }
        void addEdge(uint32_t u, uint32_t v, double c, double t) { adj_[u].emplace_back(v, c, t); }
        void setConfig(const Config &cfg) { cfg_ = cfg; }

        // ------------------------------------------------------------------------
        // HYBRID HEURISTIC & COST HELPERS (NEW)
        // ------------------------------------------------------------------------

        // Heuristic ước tính khoảng cách còn lại (hybrid)
        inline double hybridHeuristic(uint32_t u, uint32_t v) const
        {
            if (coords_[u].x == 0 && coords_[u].y == 0)
                return 0;
            double d = (coords_[u] - coords_[v]).norm();
            // Giả sử: heuristic = alpha * (distance * cost_rate) + (1-alpha) * (distance / speed)
            // Đơn giản hóa: trả về khoảng cách Euclidean, sẽ được nhân hệ số bên ngoài
            return d;
        }

        // Chi phí thực tế của cạnh theo alpha
        inline double hybridEdgeCost(const Edge &e) const
        {
            return alpha_ * e.cost + (1.0 - alpha_) * e.time;
        }

        // ------------------------------------------------------------------------
        // MAIN QUERY
        // ------------------------------------------------------------------------

        std::vector<uint32_t> query(uint32_t start, uint32_t goal,
                                    double alpha = 0.5,
                                    QueryStats *stats_out = nullptr)
        {
            auto t_start = std::chrono::high_resolution_clock::now();
            last_stats_ = QueryStats{};
            alpha_ = alpha; // Store for use in expansion

            // Reset
            ctx_.pool_fwd.clear();
            ctx_.pool_bwd.clear();
            for (auto &f : ctx_.fwd_frontier)
                f.clear();
            for (auto &f : ctx_.bwd_frontier)
                f.clear();

            // Initialize
            initialize(start, goal);

            std::vector<Label *> open_fwd, open_bwd;
            if (!ctx_.pool_fwd.empty())
                open_fwd.push_back(&ctx_.pool_fwd[0]);
            if (!ctx_.pool_bwd.empty())
                open_bwd.push_back(&ctx_.pool_bwd[0]);

            double best_cost = 1e18;
            std::vector<uint32_t> best_path;
            uint32_t beam = cfg_.initial_beam_width;
            int stuck = 0, iter = 0;

            while (!open_fwd.empty() && !open_bwd.empty() && iter < 100000)
            {
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration<double, std::milli>(now - t_start).count() > cfg_.time_limit_ms)
                    break;

                if (++stuck > cfg_.stuck_threshold)
                {
                    beam = std::min(beam * 2, cfg_.max_beam_width);
                    stuck = 0;
                }

                // Chọn chiều
                bool fwd = true;
                if (open_fwd.empty())
                    fwd = false;
                else if (!open_bwd.empty())
                    fwd = (open_fwd.front()->f_score <= open_bwd.front()->f_score);

                if (fwd)
                    expand(open_fwd, ctx_.pool_fwd, ctx_.fwd_frontier, ctx_.bwd_frontier,
                           start, goal, beam, true, best_cost, best_path);
                else
                    expand(open_bwd, ctx_.pool_bwd, ctx_.bwd_frontier, ctx_.fwd_frontier,
                           goal, start, beam, false, best_cost, best_path);

                // Termination
                double min_f = (open_fwd.empty() ? 1e18 : open_fwd.front()->f_score) +
                               (open_bwd.empty() ? 1e18 : open_bwd.front()->f_score);
                if (min_f >= best_cost * (1 + cfg_.epsilon))
                    break;
                iter++;
            }

            if (best_path.empty() && cfg_.enable_fallback)
            {
                best_path = fallbackAStar(start, goal);
                last_stats_.used_fallback = true;
            }

            auto t_end = std::chrono::high_resolution_clock::now();
            last_stats_.query_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
            last_stats_.success = !best_path.empty();

            if (stats_out)
                *stats_out = last_stats_;
            return best_path;
        }

    private:
        // ------------------------------------------------------------------------
        // EXPANSION WITH PROGRESS FACTOR (CORE UPDATE)
        // ------------------------------------------------------------------------

        void expand(std::vector<Label *> &open, std::vector<Label> &pool,
                    std::vector<std::vector<FrontierEntry>> &my_f,
                    std::vector<std::vector<FrontierEntry>> &other_f,
                    uint32_t source, uint32_t target, uint32_t beam,
                    bool is_fwd, double &best_cost, std::vector<uint32_t> &best_path)
        {

            if (open.empty())
                return;
            std::pop_heap(open.begin(), open.end(), [](Label *a, Label *b)
                          { return *a > *b; });
            Label *cur = open.back();
            open.pop_back();
            uint32_t cur_idx = cur - &pool[0];

            // Check meeting
            for (const auto &o : other_f[cur->node])
            {
                double total = alpha_ * (cur->g_cost + o.cost) + (1 - alpha_) * (cur->g_time + o.time);
                if (total < best_cost)
                {
                    best_cost = total;
                    best_path = reconstruct(cur_idx, o.label_idx, is_fwd, pool);
                }
            }

            if (isDominated(my_f[cur->node], cur->g_cost, cur->g_time))
            {
                last_stats_.frontier_pruned++;
                return;
            }
            last_stats_.nodes_expanded++;

            // Calculate heuristic values for current node (for progress calculation)
            double h_current = hybridHeuristic(cur->node, target);

            Vec2 pos_u = coords_[cur->node];
            Vec2 pos_t = coords_[target];
            Vec2 to_target = pos_t - pos_u;

            std::vector<Label> candidates;

            for (const auto &edge : adj_[cur->node])
            {
                Vec2 pos_v = coords_[edge.to];

                // ========== 1. DIRECTIONAL BIAS (von Mises) ==========
                Vec2 to_v = pos_v - pos_u;
                double angle = (to_target.norm() < 1e-9 || to_v.norm() < 1e-9) ? 0 : to_target.angleTo(to_v);
                double dir_conf = std::exp(-(angle * angle) / (2 * cfg_.sigma_theta * cfg_.sigma_theta));

                // ========== 2. PROGRESS FACTOR (NEW) ==========
                double h_next = hybridHeuristic(edge.to, target);
                double delta_h = h_current - h_next; // Tiến độ (mét)

                double hybrid_cost = hybridEdgeCost(edge);
                if (hybrid_cost < 1e-9)
                    hybrid_cost = 1e-9;

                // Efficiency: meters gained per unit cost
                double efficiency = delta_h / hybrid_cost;

                // Sigmoid: Reward if efficiency > 0 (progressing), Penalize if < 0
                // lambda controls how fast we penalize inefficient moves
                double prog_conf = sigmoid(cfg_.lambda_efficiency * efficiency);

                // ========== 3. COMBINED CONFIDENCE ==========
                double confidence = dir_conf * prog_conf;

                // Penalty for detours: if moving away (delta_h < 0), prog_conf < 0.5
                if (delta_h < 0)
                    last_stats_.progress_penalized++;

                if (confidence < cfg_.min_confidence)
                    continue;

                Label next;
                next.node = edge.to;
                next.g_cost = cur->g_cost + edge.cost;
                next.g_time = cur->g_time + edge.time;
                next.efficiency = efficiency; // Store for stats
                next.parent = cur_idx;
                next.depth = cur->depth + 1;
                next.direction = is_fwd;
                next.confidence = confidence;

                double h_hat = hybridHeuristic(edge.to, target);
                next.f_score = alpha_ * (next.g_cost + h_hat) + (1 - alpha_) * (next.g_time + h_hat / 500.0 * 60.0);

                candidates.push_back(next);
            }

            // Beam limiting by confidence
            if (candidates.size() > beam)
            {
                std::nth_element(candidates.begin(), candidates.begin() + beam, candidates.end(),
                                 [](const Label &a, const Label &b)
                                 { return a.confidence > b.confidence; });
                candidates.resize(beam);
            }

            // Add to frontier
            for (auto &c : candidates)
            {
                if (isDominated(my_f[c.node], c.g_cost, c.g_time))
                {
                    last_stats_.frontier_pruned++;
                    continue;
                }

                auto &f = my_f[c.node];
                f.erase(std::remove_if(f.begin(), f.end(),
                                       [&c, this](const FrontierEntry &e)
                                       {
                                           return c.g_cost <= e.cost * (1 + cfg_.epsilon) && c.g_time <= e.time * (1 + cfg_.epsilon);
                                       }),
                        f.end());

                uint32_t new_idx = pool.size();
                f.push_back({c.g_cost, c.g_time, new_idx});
                pool.push_back(c);
                open.push_back(&pool.back());
                std::push_heap(open.begin(), open.end(), [](Label *a, Label *b)
                               { return *a > *b; });
                last_stats_.labels_generated++;
            }
        }

        // Helper methods (initialize, isDominated, reconstruct, fallback...)
        // --- kept similar to previous version ---
        void initialize(uint32_t s, uint32_t t)
        {
            double h = hybridHeuristic(s, t);
            Label ls{s, 0, 0, h, 1.0, 0, static_cast<uint32_t>(-1), 0, true, 0};
            Label lt{t, 0, 0, h, 1.0, 0, static_cast<uint32_t>(-1), 0, false, 0};
            ctx_.pool_fwd.push_back(ls);
            ctx_.pool_bwd.push_back(lt);
            ctx_.fwd_frontier[s].push_back({0, 0, 0});
            ctx_.bwd_frontier[t].push_back({0, 0, 0});
        }

        bool isDominated(const std::vector<FrontierEntry> &f, double c, double t)
        {
            for (const auto &e : f)
                if (e.dominates(FrontierEntry{c, t, 0}, cfg_.epsilon))
                    return true;
            return false;
        }

        std::vector<uint32_t> reconstruct(uint32_t fidx, uint32_t bidx, bool is_fwd,
                                          const std::vector<Label> &pool)
        {
            std::vector<uint32_t> path;
            // ... (implementation similar to previous) ...
            uint32_t idx = fidx;
            while (idx != static_cast<uint32_t>(-1))
            {
                path.push_back(is_fwd ? ctx_.pool_fwd[idx].node : ctx_.pool_bwd[idx].node);
                idx = (is_fwd ? ctx_.pool_fwd[idx].parent : ctx_.pool_bwd[idx].parent);
            }
            std::reverse(path.begin(), path.end());
            // ... add backward part ...
            return path;
        }

        std::vector<uint32_t> fallbackAStar(uint32_t s, uint32_t g)
        {
            // ... standard A* ...
            return {};
        }
    };

} // namespace cpbg

#endif