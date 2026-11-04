#include "benchmark.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "bruteforce.h"
#include "datagen.h"
#include "facility.h"
#include "facility_index.h"
#include "kdtree.h"

namespace {
using clk = std::chrono::steady_clock;

std::string cpuModel() {
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.find("model name") == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string v = line.substr(pos + 1);
                // trim
                size_t b = v.find_first_not_of(" \t");
                if (b != std::string::npos) v = v.substr(b);
                size_t e = v.find_last_not_of(" \t\r\n");
                if (e != std::string::npos) v = v.substr(0, e + 1);
                return v;
            }
        }
    }
    return "unknown CPU";
}

double median3(double a, double b, double c) {
    std::vector<double> v = {a, b, c};
    std::sort(v.begin(), v.end());
    return v[1];
}

volatile double sink = 0.0;
}

void runBenchmark(const BenchOptions& opts) {
    namespace fs = std::filesystem;
    fs::create_directories("results");

    std::string envStr = "CPU: " + cpuModel() + "\nCompiler: " + __VERSION__ + "\n";
    {
        std::ofstream f("results/benchmark_environment.txt");
        f << envStr;
    }
    std::cout << envStr << "\n";

    std::vector<std::string> texRows;
    std::ofstream csv("results/benchmark_results.csv");
    csv << "experiment,distribution,n,k_or_radius,kd_us,brute_us,speedup,avg_nodes_visited,mismatches,avg_result_count\n";

    std::vector<double> kdTimesForTex, bruteTimesForTex;
    std::vector<std::size_t> sizesForTex;

    // Maximum queries for brute depends on n
    auto bruteQ = [&](std::size_t n) {
        if (n >= 100000) return std::min(opts.queries, (std::size_t)200);
        return opts.queries;
    };

    for (auto dist : {"uniform", "clustered"}) {
        // clustered only extra run at 100000 for nearest; but spec: clustered distribution at 100000 only. We'll do all sizes uniform, and one clustered 100000.
        if (std::string(dist) == "clustered") {
            // only n=100000
            std::vector<std::size_t> just = {100000};
            for (auto n : just) {
                // nearest only
                auto pts = datagen::generate(n, "clustered", opts.seed, "synthetic");
                KDTree kd;
                kd.build(pts);
                // queries
                std::mt19937 rq(opts.seed + 99);
                std::uniform_real_distribution<double> la(10.95, 11.10), lo(76.90, 77.05);
                std::vector<std::pair<double,double>> qs;
                for (std::size_t i = 0; i < opts.queries; ++i) qs.push_back({la(rq), lo(rq)});
                // KD timing
                double kdTotalUs = 0, bruteTotalUs = 0;
                std::size_t totalNodes = 0;
                std::size_t mism = 0;
                int bq = std::min((int)opts.queries, 200);
                for (int rep = 0; rep < 3; ++rep) {
                    auto s = clk::now();
                    for (auto& q : qs) {
                        std::size_t nv = 0;
                        auto r = kd.nearest(q.first, q.second, &nv);
                        if (r) sink += r->second;
                        totalNodes += nv;
                    }
                    auto e = clk::now();
                    kdTotalUs += std::chrono::duration<double, std::micro>(e - s).count();
                }
                // brute timing limited
                auto s = clk::now();
                for (int i = 0; i < bq; ++i) {
                    auto r = brute::bruteNearest(pts, qs[i].first, qs[i].second);
                    if (r) sink += r->second;
                }
                auto e = clk::now();
                bruteTotalUs = std::chrono::duration<double, std::micro>(e - s).count() * (double)opts.queries / bq;

                // correctness check
                for (std::size_t i = 0; i < qs.size(); ++i) {
                    auto a = kd.nearest(qs[i].first, qs[i].second);
                    auto b = brute::bruteNearest(pts, qs[i].first, qs[i].second);
                    double da = a ? a->second : 0, db = b ? b->second : 0;
                    if (std::fabs(da - db) > 1e-9) ++mism;
                }

                double kdUs = (kdTotalUs / 3.0) / opts.queries;
                double bruteUs = bruteTotalUs / opts.queries; // already normalized-ish (3 reps not for brute? spec one? we did one pass scaled)
                // Use median? We only have one brute measurement; okay.
                double speedup = bruteUs / kdUs;
                std::cout << "[clustered] n=" << n << " kd=" << kdUs << " us brute=" << bruteUs << " us speedup=" << speedup << " nodes=" << (double)totalNodes/ (3.0*opts.queries) << " mism=" << mism << "\n";
                csv << "nearest,clustered," << n << ",1," << kdUs << "," << bruteUs << "," << speedup << "," << (double)totalNodes/(3.0*opts.queries) << "," << mism << ",1\n";
            }
            continue;
        }

        // uniform all sizes
        for (auto n : opts.sizes) {
            auto pts = datagen::generate(n, "uniform", opts.seed, "synthetic");
            KDTree kd;
            auto t0 = clk::now();
            kd.build(pts);
            auto t1 = clk::now();
            double buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

            // warm-up not needed separately
            std::mt19937 rq(opts.seed + 99);
            std::uniform_real_distribution<double> la(10.95, 11.10), lo(76.90, 77.05);
            std::vector<std::pair<double,double>> qs;
            for (std::size_t i = 0; i < opts.queries; ++i) qs.push_back({la(rq), lo(rq)});

            // mac Q count for brute
            int bq = (int)bruteQ(n);

            // Helper to measure kd nearest total us over 3 reps
            auto measureNearest = [&]() {
                double reps[3];
                std::size_t nodes = 0;
                for (int r = 0; r < 3; ++r) {
                    auto s = clk::now();
                    for (auto& q : qs) {
                        std::size_t nv = 0;
                        auto res = kd.nearest(q.first, q.second, &nv);
                        if (res) sink += res->second;
                        nodes += nv;
                    }
                    auto e = clk::now();
                    reps[r] = std::chrono::duration<double, std::micro>(e - s).count();
                }
                return std::make_tuple(median3(reps[0], reps[1], reps[2]) / (double)opts.queries, (double)nodes / (3.0*(double)opts.queries));
            };

            std::cout << "n=" << n << " build=" << buildMs << " ms\n";

            // Nearest
            {
                auto [kdUs, nodes] = measureNearest();
                // brute
                auto s = clk::now();
                for (int i = 0; i < bq; ++i) {
                    auto r = brute::bruteNearest(pts, qs[i].first, qs[i].second);
                    if (r) sink += r->second;
                }
                auto e = clk::now();
                double bruteTotal = std::chrono::duration<double, std::micro>(e - s).count();
                double bruteUs = (bruteTotal / (double)bq); // per query average (single pass) normalized
                // scale to all queries? bruteUs is per query already
                // correctness mismatches
                std::size_t mism = 0;
                for (auto& q : qs) {
                    auto a = kd.nearest(q.first, q.second);
                    auto b = brute::bruteNearest(pts, q.first, q.second);
                    double da = a ? a->second : 0, db = b ? b->second : 0;
                    if (std::fabs(da - db) > 1e-9) ++mism;
                }
                double speedup = bruteUs / kdUs;
                std::cout << "  nearest: kd=" << kdUs << " us  brute=" << bruteUs << " us  speedup=" << speedup
                          << "  nodes=" << nodes << " (" << 100.0*nodes/n << "% of n)  mism=" << mism << "\n";
                csv << "nearest,uniform," << n << ",1," << kdUs << "," << bruteUs << "," << speedup << "," << nodes << "," << mism << ",1\n";
                char buf[256]; std::snprintf(buf,sizeof(buf),"%zu & %.2f & %.2f & %.2f & %.0f \\", n, kdUs, bruteUs, speedup, nodes); texRows.push_back(buf);
                if (n == 100000) { kdTimesForTex.push_back(kdUs); bruteTimesForTex.push_back(bruteUs); sizesForTex.push_back(n); }
            }

            // k-nearest k=5
            {
                double reps[3]; std::size_t nodes=0;
                for (int r=0;r<3;++r){
                    auto s=clk::now();
                    for (auto& q: qs){ std::size_t nv=0; auto res=kd.kNearest(q.first,q.second,5,&nv); for(auto&x:res) sink+=x.second; nodes+=nv; }
                    auto e=clk::now(); reps[r]=std::chrono::duration<double,std::micro>(e-s).count();
                }
                double kdUs = median3(reps[0],reps[1],reps[2])/(double)opts.queries;
                // brute
                auto s=clk::now();
                for (int i=0;i<bq;++i){ auto res=brute::bruteKNearest(pts,qs[i].first,qs[i].second,5); for(auto&x:res) sink+=x.second; }
                auto e=clk::now();
                double bruteUs = std::chrono::duration<double,std::micro>(e-s).count()/(double)bq;
                // mismatches
                std::size_t mism=0;
                for (auto& q: qs) {
                    auto a=kd.kNearest(q.first,q.second,5);
                    auto b=brute::bruteKNearest(pts,q.first,q.second,5);
                    bool bad=false;
                    if (a.size()!=b.size()) bad=true;
                    else for (size_t i=0;i<a.size();++i) if (std::fabs(a[i].second-b[i].second)>1e-9) bad=true;
                    if (bad) ++mism;
                }
                double speedup=bruteUs/kdUs;
                std::cout << "  kNN5: kd=" << kdUs << " us  brute=" << bruteUs << " us  speedup=" << speedup
                          << "  nodes=" << (double)nodes/(3.0*(double)opts.queries) << "  mism=" << mism << "\n";
                csv << "knn,uniform," << n << ",5," << kdUs << "," << bruteUs << "," << speedup << "," << (double)nodes/(3.0*(double)opts.queries) << "," << mism << ",5\n";
            }

            // radius 2km
            {
                double reps[3]; std::size_t nodes=0;
                for (int r=0;r<3;++r){
                    auto s=clk::now();
                    for (auto& q: qs){ std::size_t nv=0; auto res=kd.radiusSearch(q.first,q.second,2.0,&nv); for(auto&x:res) sink+=x.second; nodes+=nv; }
                    auto e=clk::now(); reps[r]=std::chrono::duration<double,std::micro>(e-s).count();
                }
                double kdUs = median3(reps[0],reps[1],reps[2])/(double)opts.queries;
                auto s=clk::now();
                for (int i=0;i<bq;++i){ auto res=brute::bruteRadius(pts,qs[i].first,qs[i].second,2.0); for(auto&x:res) sink+=x.second; }
                auto e=clk::now();
                double bruteUs = std::chrono::duration<double,std::micro>(e-s).count()/(double)bq;
                std::size_t mism=0; double avgRes=0;
                for (auto& q: qs) {
                    auto a=kd.radiusSearch(q.first,q.second,2.0);
                    auto b=brute::bruteRadius(pts,q.first,q.second,2.0);
                    if (a.size()!=b.size()) { ++mism; }
                    else for (size_t i=0;i<a.size();++i) if (std::fabs(a[i].second-b[i].second)>1e-9) ++mism;
                    avgRes += (double)b.size();
                }
                avgRes /= (double)qs.size();
                double speedup=bruteUs/kdUs;
                std::cout << "  radius2km: kd=" << kdUs << " us  brute=" << bruteUs << " us  speedup=" << speedup
                          << "  nodes=" << (double)nodes/(3.0*(double)opts.queries) << "  mism=" << mism << "  avgResults=" << avgRes << "\n";
                csv << "radius,uniform," << n << ",2," << kdUs << "," << bruteUs << "," << speedup << "," << (double)nodes/(3.0*(double)opts.queries) << "," << mism << "," << avgRes << "\n";
            }
        }
    }

    // Tree health experiment n=20000
    {
        std::size_t n = 20000;
        auto pts = datagen::generate(n, "uniform", opts.seed, "synthetic");
        std::cout << "\nTree health (n=" << n << ")\n";
        std::cout << "log2(n) ~ " << std::log2((double)n) << "\n";
        // state built
        KDTree built; built.build(pts);
        // random insert
        std::vector<Facility> shuffled = pts;
        std::mt19937 rr(opts.seed + 7);
        std::shuffle(shuffled.begin(), shuffled.end(), rr);
        KDTree insRandom;
        for (auto& f : shuffled) insRandom.insert(f);
        // sorted by lat
        std::vector<Facility> sorted = pts;
        std::sort(sorted.begin(), sorted.end(), [](const Facility&a,const Facility&b){return a.lat<b.lat;});
        KDTree insSorted;
        for (auto& f : sorted) insSorted.insert(f);
        insSorted.rebuild();
        // average nearest query time for each state using same queries
        std::mt19937 rq(opts.seed+99);
        std::uniform_real_distribution<double> la(10.95,11.10), lo(76.90,77.05);
        std::vector<std::pair<double,double>> qs(200);
        for (auto& q: qs) { q.first=la(rq); q.second=lo(rq); }
        auto timeTree = [&](KDTree& t)->double {
            auto s=clk::now();
            for (auto& q: qs){ auto r=t.nearest(q.first,q.second); if(r) sink+=r->second; }
            auto e=clk::now();
            return std::chrono::duration<double,std::micro>(e-s).count()/qs.size();
        };
        std::cout << "  build() height=" << built.height() << "  queryUs=" << timeTree(built) << "\n";
        std::cout << "  insert random height=" << insRandom.height() << "  queryUs=" << timeTree(insRandom) << "\n";
        // sorted before rebuild captured earlier? Need measure before rebuild. Rebuild separately:
        KDTree insSorted2;
        for (auto& f: sorted) insSorted2.insert(f);
        std::cout << "  insert sorted height=" << insSorted2.height() << "  queryUs=" << timeTree(insSorted2) << "\n";
        insSorted2.rebuild();
        std::cout << "  after rebuild height=" << insSorted2.height() << "  queryUs=" << timeTree(insSorted2) << "\n";

        csv << "treehealth,uniform," << n << ",1," << timeTree(built) << ",," << "," << built.height() << ",0,\n";
    }

    // LaTeX fragment
    std::ofstream tex("results/benchmark_table.tex");
    tex << "\\begin{tabular}{lrrrrr}\n";
    tex << "\\toprule\n";
    tex << "n & KD (us) & Brute (us) & Speedup & Nodes \\\\ \n";
    tex << "\\midrule\n";
    for (auto& r : texRows) tex << r << "\n";
    tex << "\\bottomrule\n";
    tex << "\\end{tabular}\n";

    std::cout << "\nBenchmark complete.\n";
}