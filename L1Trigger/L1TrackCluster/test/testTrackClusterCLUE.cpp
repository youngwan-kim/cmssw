// Standalone unit tests for the CLUE track clustering. Run: testTrackClusterCLUE
// Documented in bs6pi-analysis/docs/l1trackcluster.md

#include "L1Trigger/L1TrackCluster/interface/TrackClusterCLUE.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace l1ttrackcluster;

namespace {

  int gFailures = 0;

  void check(bool condition, const std::string& what) {
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what.c_str());
    if (!condition)
      ++gFailures;
  }

  // Six tracks on a small circle in (eta, phi), all mutually within dC = 0.25,
  // at a common z. This is the minimal signal-like topology.
  std::vector<InputTrack> sixProngCluster(double eta0, double phi0, double z0, double radius = 0.08) {
    std::vector<InputTrack> tracks;
    for (int i = 0; i < 6; ++i) {
      const double angle = 2. * M_PI * i / 6.;
      InputTrack t;
      t.eta = eta0 + radius * std::cos(angle);
      t.phi = phi0 + radius * std::sin(angle);
      t.z0 = z0;
      t.pt = 3.0 + 0.5 * i;  // 3.0 .. 5.5 GeV, scalar sum 25.5 GeV
      tracks.push_back(t);
    }
    return tracks;
  }

}  // namespace

int main() {
  const Parameters params;  // working point: rhoC 3.5, dC 0.25, dM 0.30, dZiso 1.0 cm

  std::printf("Six mutually close tracks form exactly one cluster\n");
  {
    const auto tracks = sixProngCluster(1.0, 0.5, 2.0);
    const auto rho = localDensity(tracks, params.dC);
    check(std::abs(rho[0] - 3.5) < 1e-9, "rho == 3.5 for a fully connected six-track group");
    const auto clusters = clusterize(tracks, params);
    check(clusters.size() == 1, "exactly one cluster");
    check(!clusters.empty() && clusters[0].tracks.size() == 6, "cluster holds all six tracks");
  }

  std::printf("Five tracks do not seed a cluster (rho == 3.0 < rhoC)\n");
  {
    auto tracks = sixProngCluster(1.0, 0.5, 2.0);
    tracks.pop_back();
    const auto rho = localDensity(tracks, params.dC);
    check(std::abs(rho[0] - 3.0) < 1e-9, "rho == 3.0 for five tracks");
    check(clusterize(tracks, params).empty(), "no cluster produced");
  }

  std::printf("Tracks isolated along z are trimmed away\n");
  {
    auto tracks = sixProngCluster(1.0, 0.5, 2.0);
    // Two extra tracks in the same (eta, phi) box but far in z, and more than
    // deltaZIso apart from each other, so neither survives trimming.
    InputTrack far1{1.02, 0.52, 12.0, 4.0};
    InputTrack far2{0.98, 0.48, 15.0, 4.0};
    tracks.push_back(far1);
    tracks.push_back(far2);
    const auto clusters = clusterize(tracks, params);
    check(clusters.size() == 1, "still one cluster");
    check(!clusters.empty() && clusters[0].tracks.size() == 6, "the two z-isolated tracks are removed");
  }

  std::printf("Trimming can push a cluster below the multiplicity threshold\n");
  {
    // Six tracks in (eta, phi) but spread in z in three well separated pairs:
    // every track has exactly one neighbour within deltaZIso, so all are trimmed.
    auto tracks = sixProngCluster(1.0, 0.5, 0.0);
    const double z[6] = {0.0, 0.5, 10.0, 10.5, 20.0, 20.5};
    for (int i = 0; i < 6; ++i)
      tracks[i].z0 = z[i];
    check(clusterize(tracks, params).empty(), "no cluster survives");
  }

  std::printf("Two separated six-prong groups give two clusters\n");
  {
    auto tracks = sixProngCluster(1.0, 0.5, 2.0);
    const auto second = sixProngCluster(-1.0, -2.0, -3.0);
    tracks.insert(tracks.end(), second.begin(), second.end());
    const auto clusters = clusterize(tracks, params);
    check(clusters.size() == 2, "two clusters");
  }

  std::printf("Result is independent of the input track ordering\n");
  {
    auto tracks = sixProngCluster(1.0, 0.5, 2.0);
    const auto second = sixProngCluster(-1.0, -2.0, -3.0);
    tracks.insert(tracks.end(), second.begin(), second.end());
    const auto reference = clusterize(tracks, params);

    bool stable = true;
    std::mt19937 rng(1234);
    for (int trial = 0; trial < 20; ++trial) {
      auto shuffled = tracks;
      std::shuffle(shuffled.begin(), shuffled.end(), rng);
      const auto clusters = clusterize(shuffled, params);
      if (clusters.size() != reference.size()) {
        stable = false;
        break;
      }
      // Compare the pT-ordered set of cluster summaries.
      for (std::size_t i = 0; i < clusters.size(); ++i) {
        const auto a = summarize(shuffled, clusters[i]);
        bool matched = false;
        for (const auto& refCluster : reference) {
          const auto b = summarize(tracks, refCluster);
          if (a.nTracks == b.nTracks && std::abs(a.sumPt - b.sumPt) < 1e-6 && std::abs(a.mass - b.mass) < 1e-6)
            matched = true;
        }
        if (!matched) {
          stable = false;
          break;
        }
      }
    }
    check(stable, "20 shuffles give the same clusters");
  }

  std::printf("Cluster summary quantities are sane\n");
  {
    const auto tracks = sixProngCluster(1.0, 0.5, 2.0);
    const auto clusters = clusterize(tracks, params);
    if (clusters.empty()) {
      check(false, "cluster available for summary");
    } else {
      const auto s = summarize(tracks, clusters[0]);
      check(s.nTracks == 6, "nTracks == 6");
      check(std::abs(s.sumPt - 25.5) < 1e-6, "scalar sum pT == 25.5 GeV");
      check(s.vecPt < s.sumPt, "vector pT below scalar sum");
      check(s.mass > 0. && s.mass < s.sumPt, "mass positive and below the scalar sum");
      check(std::abs(s.z0 - 2.0) < 1e-9, "pT-weighted z0 == 2.0 cm");
      check(std::abs(s.eta - 1.0) < 0.05, "eta close to the generated axis");
      check(s.maxDeltaR > 0.15 && s.maxDeltaR < 0.25, "max pairwise dR within dC");
      check(std::abs(s.maxDeltaZ) < 1e-9, "max pairwise dz == 0");
      check(std::abs(s.minTrackPt - 3.0) < 1e-9 && std::abs(s.maxTrackPt - 5.5) < 1e-9, "min/max track pT");
    }
  }

  std::printf("\n%s (%d failure%s)\n", gFailures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", gFailures,
              gFailures == 1 ? "" : "s");
  return gFailures == 0 ? 0 : 1;
}
