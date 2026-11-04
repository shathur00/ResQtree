#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "benchmark.h"
#include "bruteforce.h"
#include "csv_loader.h"
#include "datagen.h"
#include "facility.h"
#include "facility_index.h"
#include "haversine.h"
#include "kdtree.h"

namespace fs = std::filesystem;

namespace {
const char* const kDefaultCsvPaths[] = {"data/facilities.csv", "../data/facilities.csv"};
std::string findDefaultCsv() {
    for (const char* path : kDefaultCsvPaths) if (fs::exists(path)) return path;
    return kDefaultCsvPaths[0];
}
bool readDouble(const std::string& prompt, double& out) {
    std::string line; std::cout << prompt;
    if (!std::getline(std::cin, line)) return false;
    const std::string text = trim(line);
    if (text.empty()) return false;
    try {
        std::size_t consumed=0;
        double v = std::stod(text,&consumed);
        if (consumed != text.size()) return false;
        out=v; return true;
    } catch(...) { return false; }
}
bool readInt(const std::string& prompt, int& out) {
    std::string line; std::cout << prompt;
    if (!std::getline(std::cin, line)) return false;
    const std::string text = trim(line);
    if (text.empty()) return false;
    try {
        std::size_t consumed=0;
        long v = std::stol(text,&consumed);
        if (consumed != text.size()) return false;
        out=(int)v; return true;
    } catch(...) { return false; }
}
bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    if (!std::getline(std::cin, out)) return false;
    out = trim(out);
    return true;
}
std::vector<std::string> splitTypes(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = normalizeType(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}
void printTable(const std::vector<std::pair<Facility,double>>& res) {
    std::cout << "rank  name                          type              id      distance(km)\n";
    int rank=1;
    for (auto& r : res) {
        std::cout << rank++ << "     " << r.first.name;
        // naive pad
        std::cout << "  " << r.first.type << "  " << r.first.id << "  ";
        std::cout.precision(2); std::cout << std::fixed << r.second << "\n";
    }
}
void printTypes(const FacilityIndex& index) {
    auto stats = index.typeStats();
    if (stats.empty()) { std::cout << "  No types.\n"; return;}
    for (auto& s : stats) std::cout << "  " << s.name << " : " << s.count << " facilities, height " << s.height << "\n";
}
bool validateLat(double lat){ return lat>=-90 && lat<=90; }
bool validateLon(double lon){ return lon>=-180 && lon<=180; }

void runSelfCheck(const FacilityIndex& index, const std::vector<Facility>& allFac) {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> latD(-95.0,95.0), lonD(-185.0,185.0); // expanded
    int pass=0, fail=0;
    auto check = [&](bool ok){ if(ok) ++pass; else ++fail; };
    // nearest
    for (int i=0;i<2000;++i) {
        double lat = latD(rng);
        double lon = lonD(rng);
        // keep
        auto kd = index.nearestOverall(lat,lon);
        auto br = brute::bruteNearest(allFac,lat,lon);
        double dkd = kd?kd->second:0, dbr=br?br->second:0;
        check(!kd.has_value()==!br.has_value() && std::fabs(dkd-dbr)<1e-9);
    }
    for (int k : {1,3,5,10}) {
        for (int i=0;i<2000;++i) {
            double lat=latD(rng), lon=lonD(rng);
            auto kd=index.kNearestOverall(lat,lon,k);
            auto br=brute::bruteKNearest(allFac,lat,lon,k);
            bool ok = (kd.size()==br.size());
            if (ok) for (size_t j=0;j<kd.size();++j) if (std::fabs(kd[j].second-br[j].second)>1e-9) ok=false;
            check(ok);
        }
    }
    for (double r : {1.0,2.0,5.0}) {
        for (int i=0;i<2000;++i) {
            double lat=latD(rng), lon=lonD(rng);
            auto kd=index.radiusOverall(lat,lon,r);
            auto br=brute::bruteRadius(allFac,lat,lon,r);
            bool ok = (kd.size()==br.size());
            if (ok) for (size_t j=0;j<kd.size();++j) if (std::fabs(kd[j].second-br[j].second)>1e-9) ok=false;
            check(ok);
        }
    }
    std::cout << "Self-check: " << pass << " PASS, " << fail << " FAIL\n";
}
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--benchmark") {
        BenchOptions opts;
        for (int i=2;i<argc;++i) {
            std::string a=argv[i];
            if (a=="--sizes" && i+1<argc) { opts.sizes.clear(); std::stringstream ss(argv[++i]); std::string t; while(std::getline(ss,t,',')) opts.sizes.push_back(std::stoull(t)); }
            else if (a=="--queries" && i+1<argc) { opts.queries=std::stoull(argv[++i]); }
            else if (a=="--seed" && i+1<argc) { opts.seed=std::stoul(argv[++i]); }
        }
        runBenchmark(opts);
        return 0;
    }

    std::cout << "===== ResQTree =====\n";
    const std::string csvPath = (argc > 1) ? std::string(argv[1]) : findDefaultCsv();
    std::cout << "Loading data from: " << csvPath << "\n";
    const csv_loader::LoadResult loaded = csv_loader::load(csvPath);
    if (!loaded.ok) { std::cerr << "Error: " << loaded.error << "\n"; return 1; }
    if (loaded.facilities.empty()) { std::cerr << "Error: no usable facilities\n"; return 1; }
    FacilityIndex index; index.build(loaded.facilities);
    std::cout << "Loaded " << index.size() << " facilities across " << index.types().size() << " types.\n";
    if (loaded.skipped > 0) std::cout << "Skipped " << loaded.skipped << " row(s), first at " << loaded.firstBadLine << ".\n";

    std::vector<Facility> allFac = loaded.facilities;

    bool running=true;
    while (running) {
        std::cout << "\n===== Main Menu =====\n";
        std::cout << "1. Find nearest facility\n";
        std::cout << "2. Find k nearest facilities\n";
        std::cout << "3. Find facilities within a radius\n";
        std::cout << "4. Insert a facility\n";
        std::cout << "5. Delete a facility (by id)\n";
        std::cout << "6. Look up a facility by id\n";
        std::cout << "7. List facility types and tree stats\n";
        std::cout << "8. Rebalance all trees\n";
        std::cout << "9. Run correctness self-check\n";
        std::cout << "10. Run benchmark\n";
        std::cout << "11. Exit\n";
        std::string choice;
        if (!readLine("Your choice: ", choice)) break;
        if (choice=="11"||choice=="0") { running=false; continue;}
        if (choice=="1") {
            std::cout << "Available types:\n"; printTypes(index);
            std::string typeSpec;
            if (!readLine("Type(s) (e.g. hospital or hospital,medical_shop or all): ", typeSpec)) { std::cout << "\n"; continue; }
            // parse
            std::vector<std::string> types;
            if (normalizeType(typeSpec)=="all") types = index.types();
            else types = splitTypes(typeSpec);
            // unknown check
            std::vector<std::string> unknown;
            std::vector<std::string> valid;
            for (auto& t : types) {
                if (index.countOfType(t)==0) unknown.push_back(t);
                else valid.push_back(t);
            }
            if (!unknown.empty()) {
                std::cout << "Unknown type(s): ";
                for (auto& u: unknown) std::cout << u << " ";
                std::cout << "\nAvailable: ";
                for (auto& t: index.types()) std::cout << t << " ";
                std::cout << "\n";
            }
            if (valid.empty()) { std::cout << "No valid types.\n"; continue;}
            double lat,lon;
            if (!readDouble("Latitude: ", lat) || !validateLat(lat)) { std::cout << "  Invalid latitude.\n"; continue;}
            if (!readDouble("Longitude: ", lon) || !validateLon(lon)) { std::cout << "  Invalid longitude.\n"; continue;}
            std::size_t nv=0;
            auto res = index.nearestInTypes(valid, lat, lon, &nv);
            if (!res.has_value()) std::cout << "No facility found.\n";
            else {
                std::cout << "rank  name  type  id  distance(km)\n";
                std::cout << "1     " << res->first.name << "  " << res->first.type << "  " << res->first.id << "  ";
                std::cout.precision(2); std::cout << std::fixed << res->second << "\n";
            }
        } else if (choice=="2") {
            std::cout << "Available types:\n"; printTypes(index);
            std::string typeSpec;
            if (!readLine("Type(s): ", typeSpec)) { std::cout<<"\n"; continue;}
            std::vector<std::string> types;
            if (normalizeType(typeSpec)=="all") types=index.types();
            else types=splitTypes(typeSpec);
            std::vector<std::string> unknown,valid;
            for (auto&t:types) if (index.countOfType(t)==0) unknown.push_back(t); else valid.push_back(t);
            if(!unknown.empty()){ std::cout<<"Unknown: "; for(auto&u:unknown) std::cout<<u<<" "; std::cout<<"\nAvailable: "; for(auto&t:index.types()) std::cout<<t<<" "; std::cout<<"\n";}
            if (valid.empty()){ std::cout<<"No valid.\n"; continue;}
            double lat,lon; int k;
            if (!readDouble("Latitude: ", lat) || !validateLat(lat)) { std::cout<<"Invalid.\n"; continue;}
            if (!readDouble("Longitude: ", lon) || !validateLon(lon)) { std::cout<<"Invalid.\n"; continue;}
            if (!readInt("k: ", k) || k<1) { std::cout<<"Invalid k.\n"; continue;}
            auto res = index.kNearestInTypes(valid,lat,lon,k);
            printTable(res);
        } else if (choice=="3") {
            std::cout << "Available types:\n"; printTypes(index);
            std::string typeSpec;
            if (!readLine("Type(s): ", typeSpec)) { std::cout<<"\n"; continue;}
            std::vector<std::string> types;
            if (normalizeType(typeSpec)=="all") types=index.types();
            else types=splitTypes(typeSpec);
            std::vector<std::string> unknown,valid;
            for (auto&t:types) if (index.countOfType(t)==0) unknown.push_back(t); else valid.push_back(t);
            if(!unknown.empty()){ std::cout<<"Unknown: "; for(auto&u:unknown) std::cout<<u<<" "; std::cout<<"\nAvailable: "; for(auto&t:index.types()) std::cout<<t<<" "; std::cout<<"\n";}
            if (valid.empty()){ std::cout<<"No valid.\n"; continue;}
            double lat,lon,r;
            if (!readDouble("Latitude: ", lat) || !validateLat(lat)) { std::cout<<"Invalid.\n"; continue;}
            if (!readDouble("Longitude: ", lon) || !validateLon(lon)) { std::cout<<"Invalid.\n"; continue;}
            if (!readDouble("Radius km: ", r) || r<=0) { std::cout<<"Invalid radius.\n"; continue;}
            auto res = index.radiusInTypes(valid,lat,lon,r);
            printTable(res);
        } else if (choice=="4") {
            int id; std::string name,type;
            // read id
            if (!readInt("Id: ", id)) { std::cout<<"Invalid.\n"; continue;}
            // check duplicate
            if (index.findById(id).has_value()) { std::cout << "Duplicate id; rejected.\n"; continue; }
            if (!readLine("Name: ", name)) { std::cout<<"\n"; continue;}
            if (!readLine("Type: ", type)) { std::cout<<"\n"; continue;}
            double lat,lon;
            if (!readDouble("Latitude: ", lat) || !validateLat(lat)) { std::cout<<"Invalid.\n"; continue;}
            if (!readDouble("Longitude: ", lon) || !validateLon(lon)) { std::cout<<"Invalid.\n"; continue;}
            Facility f{id,name,type,lat,lon};
            if (!index.add(f)) { std::cout << "Insert failed (duplicate id).\n"; continue;}
            allFac.push_back(f);
            std::cout << "Inserted. Total count: " << index.size() << "\n";
        } else if (choice=="5") {
            int id;
            if (!readInt("Id to delete: ", id)) { std::cout<<"Invalid.\n"; continue;}
            if (index.remove(id)) std::cout << "Deleted. Total count: " << index.size() << "\n";
            else std::cout << "Not found.\n";
        } else if (choice=="6") {
            int id;
            if (!readInt("Id: ", id)) { std::cout<<"Invalid.\n"; continue;}
            auto f=index.findById(id);
            if (!f.has_value()) std::cout << "Not found.\n";
            else std::cout << f->name << " " << f->type << " " << f->lat << " " << f->lon << "\n";
        } else if (choice=="7") {
            printTypes(index);
        } else if (choice=="8") {
            index.rebuildAll();
            std::cout << "Rebalanced. Total count: " << index.size() << "\n";
        } else if (choice=="9") {
            runSelfCheck(index, allFac);
        } else if (choice=="10") {
            BenchOptions opts;
            runBenchmark(opts);
        } else {
            std::cout << "Invalid choice.\n";
        }
    }
    std::cout << "\nGoodbye.\n";
    return 0;
}