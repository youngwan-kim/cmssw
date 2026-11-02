import FWCore.ParameterSet.Config as cms

############################################################
# Cluster-seeded regional tracking -- WP3a, the CPU-critical piece.
#
# HONEST SCOPE WARNING. This is a *standalone timing prototype*, not something
# that can be dropped into the full menu, and the reason is worth stating plainly
# so that nobody discovers it the hard way:
#
#   `useClusterSeededRegions` retargets the tracking regions of the shared
#   hltPhase2PixelTracksHitDoublets module. In a job that runs only this path
#   that is exactly what we want. In the full menu it is the same module instance
#   every other path's tracking depends on, so it would regionalise the entire
#   menu around our clusters. Making this menu-safe means cloning the pixel and
#   initial-step chains under hltBsTauTau* labels -- mechanical, but a real chunk
#   of configuration, and pointless before the timing number says the regional
#   approach is worth having.
#
# Second constraint: the default Phase-2 pixel tracking is the alpaka SoA chain
# (hltPhase2PixelTracksSoA), which reconstructs globally and takes no
# TrackingRegion input at all. Regions only enter through the legacy chain, so
# this requires
#
#     --procModifiers hltPhase2LegacyTracking
#
# or the equivalent in the runner cfg. The function checks for that and refuses
# rather than silently doing nothing.
#
# What it buys, and why it is worth measuring: the inclusive Phase-2 pixel regions
# (hltPhase2PixelTracksAndHighPtStepTrackingRegions) span 4 sigma_z of the beam
# spot, O(20) cm of half-length, over the full 2pi. The cluster gives eta, phi and
# a z0 good to ~1 mm, so a region of Delta eta x Delta phi = 0.8 x 0.8 and
# z half-length 1.5 cm covers the signal while spanning roughly
#   (0.8 x 0.8) / (5.0 x 6.3) x (1.5 / 20) ~ 1.5e-3
# of the inclusive seeding volume, per cluster. Whether that turns into a
# comparable CPU saving depends entirely on what fraction of the time is in
# seeding versus local reconstruction, which is why this needs measuring rather
# than arguing about.
############################################################

from HLTrigger.BsToTauTau.hltBsTauTauModules_cff import hltBsTauTauClusterTrackingRegions


def useClusterSeededRegions(process, ptMin=0.5):
    """Point the Phase-2 pixel-track seeding at the L1-cluster regions.

    Returns the process. Raises if the legacy tracking chain is not in the
    process, since without it the regions would be produced and ignored.
    """
    if not hasattr(process, "hltPhase2PixelTracksHitDoublets"):
        raise RuntimeError(
            "hltPhase2PixelTracksHitDoublets not found in the process: the alpaka pixel "
            "tracking chain reconstructs globally and cannot take tracking regions. "
            "Add the hltPhase2LegacyTracking process modifier."
        )

    process.hltBsTauTauClusterTrackingRegions = hltBsTauTauClusterTrackingRegions.clone()
    process.hltBsTauTauClusterTrackingRegions.RegionPSet.ptMin = cms.double(ptMin)

    process.hltPhase2PixelTracksHitDoublets.trackingRegions = cms.InputTag(
        "hltBsTauTauClusterTrackingRegions"
    )

    # The regions must be produced before the doublets. Inserting into the
    # existing sequence keeps the ordering explicit rather than relying on the
    # unscheduled resolution.
    for name in process.sequences_():
        seq = getattr(process, name)
        if seq.contains(process.hltPhase2PixelTracksHitDoublets):
            seq.insert(0, process.hltBsTauTauClusterTrackingRegions)
            break
    else:
        raise RuntimeError("no sequence in the process contains hltPhase2PixelTracksHitDoublets")

    return process
