#ifndef L1Trigger_L1TrackCluster_TrackClusterCLUE_h
#define L1Trigger_L1TrackCluster_TrackClusterCLUE_h

// CLUE-based clustering of GTT tracks for Bs -> tau(3pi) tau(3pi) at L1.
// Simplified variant of CLUE (arXiv:2001.09761); float reference implementation.
// Documented in bs6pi-analysis/docs/l1trackcluster.md

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace l1ttrackcluster {

  // PDG charged pion mass, used to build the cluster four-momentum.
  constexpr double kChargedPionMass = 0.13957039;

  // One GTT track, reduced to the quantities the clustering needs.
  struct InputTrack {
    double eta = 0.;
    double phi = 0.;
    double z0 = 0.;  // cm
    double pt = 0.;  // GeV
  };

  struct Parameters {
    // Clustering working point from the 9 June 2026 deck.
    double rhoC = 3.5;         // minimum local density to promote a track to a seed
    double dC = 0.25;          // local-density radius in (eta, phi)
    double dM = 0.30;          // cluster-growth box half-width in eta and phi
    double deltaZIso = 1.0;    // cm, z-isolation window for post-clustering trimming
    unsigned int minNeighboursInZ = 1;  // 1, not 2: settled by measurement, not by the deck
    unsigned int minNTracks = 6;        // tracks required in the cluster after trimming
    bool seedThresholdInclusive = true;  // rho >= rhoC, not rho > rhoC
  };

  struct Cluster {
    std::vector<unsigned int> tracks;  // indices into the input track vector
    unsigned int seed = 0;             // index of the seeding track
    double rhoSeed = 0.;
  };

  struct ClusterSummary {
    unsigned int nTracks = 0;
    double sumPt = 0.;  // scalar sum of track pT ("cluster total pT")
    double vecPt = 0.;  // pT of the four-momentum sum
    double eta = 0.;
    double phi = 0.;
    double z0 = 0.;    // pT-weighted mean
    double mass = 0.;  // invariant mass of the tracks, taken as charged pions
    double maxDeltaR = 0.;
    double maxDeltaZ = 0.;
    double stdDeltaZ = 0.;
    double stdPtOverSumPt = 0.;
    double minTrackPt = 0.;
    double maxTrackPt = 0.;
  };

  inline double deltaPhi(double phi1, double phi2) {
    double dphi = phi1 - phi2;
    while (dphi > M_PI)
      dphi -= 2. * M_PI;
    while (dphi <= -M_PI)
      dphi += 2. * M_PI;
    return dphi;
  }

  inline double deltaR2(const InputTrack& a, const InputTrack& b) {
    const double deta = a.eta - b.eta;
    const double dphi = deltaPhi(a.phi, b.phi);
    return deta * deta + dphi * dphi;
  }

  // rho_i = 1 + 0.5 * #{j != i : dR(i,j) <= dC}
  inline std::vector<double> localDensity(const std::vector<InputTrack>& tracks, double dC) {
    const double dC2 = dC * dC;
    std::vector<double> rho(tracks.size(), 1.0);
    for (std::size_t i = 0; i < tracks.size(); ++i) {
      for (std::size_t j = i + 1; j < tracks.size(); ++j) {
        if (deltaR2(tracks[i], tracks[j]) <= dC2) {
          rho[i] += 0.5;
          rho[j] += 0.5;
        }
      }
    }
    return rho;
  }

  // Single pass: neighbour counts are evaluated before any removal.
  inline void trimInZ(const std::vector<InputTrack>& tracks, const Parameters& params, Cluster& cluster) {
    if (params.minNeighboursInZ == 0 || params.deltaZIso <= 0.)
      return;
    std::vector<unsigned int> kept;
    kept.reserve(cluster.tracks.size());
    for (const unsigned int i : cluster.tracks) {
      unsigned int neighbours = 0;
      for (const unsigned int j : cluster.tracks) {
        if (i == j)
          continue;
        if (std::abs(tracks[i].z0 - tracks[j].z0) <= params.deltaZIso)
          ++neighbours;
      }
      if (neighbours >= params.minNeighboursInZ)
        kept.push_back(i);
    }
    cluster.tracks.swap(kept);
  }

  inline std::vector<Cluster> clusterize(const std::vector<InputTrack>& tracks, const Parameters& params) {
    std::vector<Cluster> clusters;
    if (tracks.empty())
      return clusters;

    const std::vector<double> rho = localDensity(tracks, params.dC);

    std::vector<unsigned int> seeds;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
      const bool isSeed = params.seedThresholdInclusive ? (rho[i] >= params.rhoC) : (rho[i] > params.rhoC);
      if (isSeed)
        seeds.push_back(i);
    }

    std::sort(seeds.begin(), seeds.end(), [&](unsigned int a, unsigned int b) {
      if (rho[a] != rho[b])
        return rho[a] > rho[b];
      if (tracks[a].pt != tracks[b].pt)
        return tracks[a].pt > tracks[b].pt;
      return a < b;
    });

    std::vector<bool> used(tracks.size(), false);
    for (const unsigned int seed : seeds) {
      if (used[seed])
        continue;
      Cluster cluster;
      cluster.seed = seed;
      cluster.rhoSeed = rho[seed];
      for (std::size_t j = 0; j < tracks.size(); ++j) {
        if (used[j])
          continue;
        if (std::abs(tracks[j].eta - tracks[seed].eta) > params.dM)
          continue;
        if (std::abs(deltaPhi(tracks[j].phi, tracks[seed].phi)) > params.dM)
          continue;
        cluster.tracks.push_back(j);
        used[j] = true;
      }
      trimInZ(tracks, params, cluster);
      if (cluster.tracks.size() >= params.minNTracks)
        clusters.push_back(std::move(cluster));
    }
    return clusters;
  }

  inline ClusterSummary summarize(const std::vector<InputTrack>& tracks, const Cluster& cluster) {
    ClusterSummary s;
    s.nTracks = cluster.tracks.size();
    if (cluster.tracks.empty())
      return s;

    double px = 0., py = 0., pz = 0., energy = 0., sumPtZ = 0.;
    s.minTrackPt = tracks[cluster.tracks.front()].pt;
    for (const unsigned int i : cluster.tracks) {
      const InputTrack& t = tracks[i];
      const double theta = 2. * std::atan(std::exp(-t.eta));
      const double p = t.pt / std::sin(theta);
      px += t.pt * std::cos(t.phi);
      py += t.pt * std::sin(t.phi);
      pz += p * std::cos(theta);
      energy += std::sqrt(p * p + kChargedPionMass * kChargedPionMass);
      s.sumPt += t.pt;
      sumPtZ += t.pt * t.z0;
      s.minTrackPt = std::min(s.minTrackPt, t.pt);
      s.maxTrackPt = std::max(s.maxTrackPt, t.pt);
    }

    s.vecPt = std::sqrt(px * px + py * py);
    s.z0 = sumPtZ / s.sumPt;
    s.phi = std::atan2(py, px);
    const double p = std::sqrt(px * px + py * py + pz * pz);
    // Guard the pathological case of a cluster balanced exactly along z.
    s.eta = (p > std::abs(pz)) ? 0.5 * std::log((p + pz) / (p - pz)) : 0.;
    const double m2 = energy * energy - p * p;
    s.mass = (m2 > 0.) ? std::sqrt(m2) : 0.;

    // Pairwise spreads.
    std::vector<double> pairDeltaZ;
    pairDeltaZ.reserve(cluster.tracks.size() * (cluster.tracks.size() - 1) / 2);
    for (std::size_t a = 0; a < cluster.tracks.size(); ++a) {
      for (std::size_t b = a + 1; b < cluster.tracks.size(); ++b) {
        const InputTrack& ta = tracks[cluster.tracks[a]];
        const InputTrack& tb = tracks[cluster.tracks[b]];
        s.maxDeltaR = std::max(s.maxDeltaR, std::sqrt(deltaR2(ta, tb)));
        const double dz = std::abs(ta.z0 - tb.z0);
        s.maxDeltaZ = std::max(s.maxDeltaZ, dz);
        pairDeltaZ.push_back(dz);
      }
    }

    auto stdev = [](const std::vector<double>& v) {
      if (v.size() < 2)
        return 0.;
      double mean = 0.;
      for (const double x : v)
        mean += x;
      mean /= v.size();
      double sumsq = 0.;
      for (const double x : v)
        sumsq += (x - mean) * (x - mean);
      return std::sqrt(sumsq / v.size());
    };

    s.stdDeltaZ = stdev(pairDeltaZ);

    std::vector<double> trackPt;
    trackPt.reserve(cluster.tracks.size());
    for (const unsigned int i : cluster.tracks)
      trackPt.push_back(tracks[i].pt);
    s.stdPtOverSumPt = stdev(trackPt) / s.sumPt;

    return s;
  }

}  // namespace l1ttrackcluster

#endif
