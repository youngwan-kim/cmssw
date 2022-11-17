import FWCore.ParameterSet.Config as cms
from PhysicsTools.NanoAOD.nano_eras_cff import *
from PhysicsTools.NanoAOD.common_cff import *
from PhysicsTools.NanoAOD.simpleCandidateFlatTableProducer_cfi import simpleCandidateFlatTableProducer
from math import ceil,log


photon_id_modules_WorkingPoints_nanoAOD = cms.PSet(
    modules = cms.vstring(
        'RecoEgamma.PhotonIdentification.Identification.cutBasedPhotonID_Fall17_94X_V1_TrueVtx_cff',
        'RecoEgamma.PhotonIdentification.Identification.cutBasedPhotonID_Fall17_94X_V2_cff',
        'RecoEgamma.PhotonIdentification.Identification.mvaPhotonID_Fall17_94X_V1p1_cff',
        'RecoEgamma.PhotonIdentification.Identification.mvaPhotonID_Fall17_94X_V2_cff',
        'RecoEgamma.PhotonIdentification.Identification.mvaPhotonID_Winter22_122X_V1_cff',
        'RecoEgamma.PhotonIdentification.Identification.cutBasedPhotonID_RunIIIWinter22_122X_V1_cff',
   ),
   WorkingPoints = cms.vstring(
     "egmPhotonIDs:cutBasedPhotonID-RunIIIWinter22-122X-V1-loose",
     "egmPhotonIDs:cutBasedPhotonID-RunIIIWinter22-122X-V1-medium",
     "egmPhotonIDs:cutBasedPhotonID-RunIIIWinter22-122X-V1-tight",
   )
)

photon_id_modules_WorkingPoints_nanoAOD_Fall17V2 = cms.PSet(
    modules = photon_id_modules_WorkingPoints_nanoAOD.modules,
    WorkingPoints = cms.vstring(
      "egmPhotonIDs:cutBasedPhotonID-Fall17-94X-V2-loose",
      "egmPhotonIDs:cutBasedPhotonID-Fall17-94X-V2-medium",
      "egmPhotonIDs:cutBasedPhotonID-Fall17-94X-V2-tight",
    )

)

def make_bitmapVID_docstring(id_modules_working_points_pset):
    pset = id_modules_working_points_pset

    for modname in pset.modules:
        ids = __import__(modname, globals(), locals(), ['idName','cutFlow'])
        for name in dir(ids):
            _id = getattr(ids,name)
            if hasattr(_id,'idName') and hasattr(_id,'cutFlow'):
                if (len(pset.WorkingPoints)>0 and _id.idName == pset.WorkingPoints[0].split(':')[-1]):
                    cut_names = ','.join([cut.cutName.value() for cut in _id.cutFlow])
                    n_bits_per_cut = int(ceil(log(len(pset.WorkingPoints)+1,2)))
                    return 'VID compressed bitmap (%s), %d bits per cut'%(cut_names, n_bits_per_cut)
    raise ValueError("Something is wrong in the photon ID modules parameter set!")


bitmapVIDForPho = cms.EDProducer("PhoVIDNestedWPBitmapProducer",
    src = cms.InputTag("slimmedPhotons"),
    srcForID = cms.InputTag("reducedEgamma","reducedGedPhotons"),
    WorkingPoints = photon_id_modules_WorkingPoints_nanoAOD.WorkingPoints,
)

bitmapVIDForPhoFall17V2 = cms.EDProducer("PhoVIDNestedWPBitmapProducer",
    src = cms.InputTag("slimmedPhotons"),
    srcForID = cms.InputTag("reducedEgamma","reducedGedPhotons"),
    WorkingPoints = photon_id_modules_WorkingPoints_nanoAOD_Fall17V2.WorkingPoints,
)

isoForPho = cms.EDProducer("PhoIsoValueMapProducer",
    src = cms.InputTag("slimmedPhotons"),
    relative = cms.bool(False),
    doQuadratic = cms.bool(True),
    rho_PFIso = cms.InputTag("fixedGridRhoFastjetAll"),
    QuadraticEAFile_PFIso_Chg  = cms.FileInPath("RecoEgamma/PhotonIdentification/data/RunIII_Winter22/effectiveArea_ChgHadronIso_95percentBased.txt"),
    QuadraticEAFile_PFIso_ECal = cms.FileInPath("RecoEgamma/PhotonIdentification/data/RunIII_Winter22/effectiveArea_ECalClusterIso_95percentBased.txt"),
    QuadraticEAFile_PFIso_HCal = cms.FileInPath("RecoEgamma/PhotonIdentification/data/RunIII_Winter22/effectiveArea_HCalClusterIso_95percentBased.txt"),
)

hOverEForPho = cms.EDProducer("PhoHoverEValueMapProducer",
    src = cms.InputTag("slimmedPhotons"),
    relative = cms.bool(False),
    rho = cms.InputTag("fixedGridRhoFastjetAll"),
    QuadraticEAFile_HoverE = cms.FileInPath("RecoEgamma/PhotonIdentification/data/RunIII_Winter22/effectiveArea_coneBasedHoverE_95percentBased.txt"),
)

isoForPhoFall17V2 = cms.EDProducer("PhoIsoValueMapProducer",
    src = cms.InputTag("slimmedPhotons"),
    relative = cms.bool(False),
    doQuadratic = cms.bool(False),
    rho_PFIso = cms.InputTag("fixedGridRhoFastjetAll"),
    EAFile_PFIso_Chg = cms.FileInPath("RecoEgamma/PhotonIdentification/data/Fall17/effAreaPhotons_cone03_pfChargedHadrons_90percentBased_V2.txt"),
    EAFile_PFIso_Neu = cms.FileInPath("RecoEgamma/PhotonIdentification/data/Fall17/effAreaPhotons_cone03_pfNeutralHadrons_90percentBased_V2.txt"),
    EAFile_PFIso_Pho = cms.FileInPath("RecoEgamma/PhotonIdentification/data/Fall17/effAreaPhotons_cone03_pfPhotons_90percentBased_V2.txt"),
)


seedGainPho = cms.EDProducer("PhotonSeedGainProducer", src = cms.InputTag("slimmedPhotons"))

import RecoEgamma.EgammaTools.calibratedEgammas_cff

calibratedPatPhotonsNano = RecoEgamma.EgammaTools.calibratedEgammas_cff.calibratedPatPhotons.clone(
    produceCalibratedObjs = False,
    correctionFile = cms.string("EgammaAnalysis/ElectronTools/data/ScalesSmearings/Run2016_UltraLegacy_preVFP_RunFineEtaR9Gain"),
)

(run2_egamma_2016 & tracker_apv_vfp30_2016).toModify(
    calibratedPatPhotonsNano,
    correctionFile = cms.string("EgammaAnalysis/ElectronTools/data/ScalesSmearings/Run2016_UltraLegacy_preVFP_RunFineEtaR9Gain")
)

(run2_egamma_2016 & ~tracker_apv_vfp30_2016).toModify(
    calibratedPatPhotonsNano,
    correctionFile = cms.string("EgammaAnalysis/ElectronTools/data/ScalesSmearings/Run2016_UltraLegacy_postVFP_RunFineEtaR9Gain"),
)

run2_egamma_2017.toModify(
    calibratedPatPhotonsNano,
    correctionFile = cms.string("EgammaAnalysis/ElectronTools/data/ScalesSmearings/Run2017_24Feb2020_runEtaR9Gain_v2")
)

run2_egamma_2018.toModify(
    calibratedPatPhotonsNano,
    correctionFile = cms.string("EgammaAnalysis/ElectronTools/data/ScalesSmearings/Run2018_29Sep2020_RunFineEtaR9Gain")
)

slimmedPhotonsWithUserData = cms.EDProducer("PATPhotonUserDataEmbedder",
    src = cms.InputTag("slimmedPhotons"),
    parentSrcs = cms.VInputTag("reducedEgamma:reducedGedPhotons"),
    userFloats = cms.PSet(
        mvaID = cms.InputTag("photonMVAValueMapProducer:PhotonMVAEstimatorRunIIFall17v2Values"),
        mvaID_RunIIIWinter22V1 = cms.InputTag("photonMVAValueMapProducer:PhotonMVAEstimatorRunIIIWinter22v1Values"),
        PFIsoChgFall17V2 = cms.InputTag("isoForPhoFall17V2:PFIsoChg"),
        PFIsoAllFall17V2 = cms.InputTag("isoForPhoFall17V2:PFIsoAll"),
        PFIsoChgQuadratic = cms.InputTag("isoForPho:PFIsoChgQuadratic"),
        PFIsoAllQuadratic = cms.InputTag("isoForPho:PFIsoAllQuadratic"),
        HoverEQuadratic = cms.InputTag("hOverEForPho:HoEForPhoEACorr"),
    ),
    userIntFromBools = cms.PSet(
        cutBasedID_RunIIIWinter22V1_loose  = cms.InputTag("egmPhotonIDs:cutBasedPhotonID-RunIIIWinter22-122X-V1-loose"),
        cutBasedID_RunIIIWinter22V1_medium = cms.InputTag("egmPhotonIDs:cutBasedPhotonID-RunIIIWinter22-122X-V1-medium"),
        cutBasedID_RunIIIWinter22V1_tight  = cms.InputTag("egmPhotonIDs:cutBasedPhotonID-RunIIIWinter22-122X-V1-tight"),
        cutbasedID_Fall17V2_loose  = cms.InputTag("egmPhotonIDs:cutBasedPhotonID-Fall17-94X-V2-loose"),
        cutbasedID_Fall17V2_medium = cms.InputTag("egmPhotonIDs:cutBasedPhotonID-Fall17-94X-V2-medium"),
        cutbasedID_Fall17V2_tight  = cms.InputTag("egmPhotonIDs:cutBasedPhotonID-Fall17-94X-V2-tight"),
        mvaID_WP90 = cms.InputTag("egmPhotonIDs:mvaPhoID-RunIIFall17-v2-wp90"),
        mvaID_WP80 = cms.InputTag("egmPhotonIDs:mvaPhoID-RunIIFall17-v2-wp80"),
        mvaID_RunIIIWinter22V1_WP90 = cms.InputTag("egmPhotonIDs:mvaPhoID-RunIIIWinter22-v1-wp90"),
        mvaID_RunIIIWinter22V1_WP80 = cms.InputTag("egmPhotonIDs:mvaPhoID-RunIIIWinter22-v1-wp80"),
    ),
    userInts = cms.PSet(
        VIDNestedWPBitmap = cms.InputTag("bitmapVIDForPho"),
        VIDNestedWPBitmapFall17V2 = cms.InputTag("bitmapVIDForPhoFall17V2"),
        seedGain = cms.InputTag("seedGainPho"),
    )
)


(run2_egamma_2016 | run2_egamma_2017 | run2_egamma_2018).toModify(
    slimmedPhotonsWithUserData.userFloats,
    ecalEnergyErrPostCorrNew = cms.InputTag("calibratedPatPhotonsNano","ecalEnergyErrPostCorr"),
    ecalEnergyPreCorrNew     = cms.InputTag("calibratedPatPhotonsNano","ecalEnergyPreCorr"),
    ecalEnergyPostCorrNew    = cms.InputTag("calibratedPatPhotonsNano","ecalEnergyPostCorr"),
    energyScaleUpNew            = cms.InputTag("calibratedPatPhotonsNano","energyScaleUp"),
    energyScaleDownNew          = cms.InputTag("calibratedPatPhotonsNano","energyScaleDown"),
    energySigmaUpNew            = cms.InputTag("calibratedPatPhotonsNano","energySigmaUp"),
    energySigmaDownNew          = cms.InputTag("calibratedPatPhotonsNano","energySigmaDown"),
)


finalPhotons = cms.EDFilter("PATPhotonRefSelector",
    src = cms.InputTag("slimmedPhotonsWithUserData"),
    cut = cms.string("pt > 5 ")
)

photonTable = simpleCandidateFlatTableProducer.clone(
    src = cms.InputTag("linkedObjects","photons"),
    name= cms.string("Photon"),
    doc = cms.string("slimmedPhotons after basic selection (" + finalPhotons.cut.value()+")"),
    variables = cms.PSet(P3Vars,
        jetIdx = Var("?hasUserCand('jet')?userCand('jet').key():-1", int, doc="index of the associated jet (-1 if none)"),
        electronIdx = Var("?hasUserCand('electron')?userCand('electron').key():-1", int, doc="index of the associated electron (-1 if none)"),
        energyErr = Var("getCorrectedEnergyError('regression2')",float,doc="energy error of the cluster from regression",precision=6),
        energyRaw = Var("superCluster().rawEnergy()",float,doc="raw energy of photon supercluster", precision=10),
        r9 = Var("full5x5_r9()",float,doc="R9 of the supercluster, calculated with full 5x5 region",precision=8),
        sieie = Var("full5x5_sigmaIetaIeta()",float,doc="sigma_IetaIeta of the supercluster, calculated with full 5x5 region",precision=8),
        sipip = Var("showerShapeVariables().sigmaIphiIphi", float, doc="sigmaIphiIphi of the supercluster", precision=8),
        sieip = Var("full5x5_showerShapeVariables().sigmaIetaIphi",float,doc="sigma_IetaIphi of the supercluster, calculated with full 5x5 region",precision=8),
        s4 = Var("full5x5_showerShapeVariables().e2x2/full5x5_showerShapeVariables().e5x5",float,doc="e2x2/e5x5 of the supercluster, calculated with full 5x5 region",precision=8),
        etaWidth = Var("superCluster().etaWidth()",float,doc="Width of the photon supercluster in eta", precision=8),
        phiWidth = Var("superCluster().phiWidth()",float,doc="Width of the photon supercluster in phi", precision=8),
        cutBased = Var(
            "userInt('cutBasedID_RunIIIWinter22V1_loose')+userInt('cutBasedID_RunIIIWinter22V1_medium')+userInt('cutBasedID_RunIIIWinter22V1_tight')",
            int,
            doc="cut-based ID bitmap, RunIIIWinter22V1, (0:fail, 1:loose, 2:medium, 3:tight)",
        ),
        cutBasedFall17V2 = Var(
            "userInt('cutbasedID_Fall17V2_loose')+userInt('cutbasedID_Fall17V2_medium')+userInt('cutbasedID_Fall17V2_tight')",
            int,
            doc="cut-based ID bitmap, Fall17V2, (0:fail, 1:loose, 2:medium, 3:tight)",
        ),
        vidNestedWPBitmap = Var(
            "userInt('VIDNestedWPBitmap')",
            int,
            doc="RunIIIWinter22V1 " + make_bitmapVID_docstring(photon_id_modules_WorkingPoints_nanoAOD),
        ),
        vidNestedWPBitmapFall17V2 = Var(
            "userInt('VIDNestedWPBitmapFall17V2')",
            int,
            doc="Fall17V2 " + make_bitmapVID_docstring(photon_id_modules_WorkingPoints_nanoAOD_Fall17V2),
        ),
        electronVeto = Var("passElectronVeto()",bool,doc="pass electron veto"),
        pixelSeed = Var("hasPixelSeed()",bool,doc="has pixel seed"),
        mvaID = Var("userFloat('mvaID')",float,doc="MVA ID score, Fall17V2",precision=10),
        mvaID_RunIIIWinter22V1 = Var("userFloat('mvaID_RunIIIWinter22V1')",float,doc="MVA ID score, RunIIIWinter22V1",precision=10),          
        mvaID_WP90 = Var("userInt('mvaID_WP90')",bool,doc="MVA ID WP90, Fall17V2"),
        mvaID_WP80 = Var("userInt('mvaID_WP80')",bool,doc="MVA ID WP80, Fall17V2"),
        mvaID_RunIIIWinter22V1_WP90 = Var("userInt('mvaID_RunIIIWinter22V1_WP90')",bool,doc="MVA ID WP90, RunIIIWinter22V1"),
        mvaID_RunIIIWinter22V1_WP80 = Var("userInt('mvaID_RunIIIWinter22V1_WP80')",bool,doc="MVA ID WP80, RunIIIWinter22V1"),                 
        pfPhoIso03 = Var("photonIso()",float,doc="PF absolute isolation dR=0.3, photon component (uncorrected)"),
        pfChargedIsoPFPV = Var("chargedHadronPFPVIso()",float,doc="PF absolute isolation dR=0.3, charged component (PF PV only)"),
        pfChargedIsoWorstVtx = Var("chargedHadronWorstVtxIso()",float,doc="PF absolute isolation dR=0.3, charged component (Vertex with largest isolation)"),
        pfRelIso03_chgFall17V2 = Var("userFloat('PFIsoChgFall17V2')/pt",float,doc="PF relative isolation dR=0.3, charged component (with rho*EA PU corrections)"),
        pfRelIso03_allFall17V2 = Var("userFloat('PFIsoAllFall17V2')/pt",float,doc="PF relative isolation dR=0.3, total (with rho*EA PU corrections)"),
        pfRelIso03_chg_quadratic = Var("userFloat('PFIsoChgQuadratic')/pt",float,doc="PF relative isolation dR=0.3, charged hadron component (with quadraticEA*rho*rho + linearEA*rho corrections)"),
        pfRelIso03_all_quadratic = Var("userFloat('PFIsoAllQuadratic')/pt",float,doc="PF relative isolation dR=0.3, total (with quadraticEA*rho*rho + linearEA*rho corrections)"),
        hoe = Var("hadronicOverEm()",float,doc="H over E",precision=8),
        hoe_PUcorr = Var("userFloat('HoverEQuadratic')",float,doc="PU corrected H/E (cone-based with quadraticEA*rho*rho + linearEA*rho corrections)",precision=8),
        isScEtaEB = Var("abs(superCluster().eta()) < 1.4442",bool,doc="is supercluster eta within barrel acceptance"),
        isScEtaEE = Var("abs(superCluster().eta()) > 1.566 && abs(superCluster().eta()) < 2.5",bool,doc="is supercluster eta within endcap acceptance"),
        seedGain = Var("userInt('seedGain')","uint8",doc="Gain of the seed crystal"),
        # position of photon is best approximated by position of seed cluster, not the SC centroid
        x_calo = Var("superCluster().seed().position().x()",float,doc="photon supercluster position on calorimeter, x coordinate (cm)",precision=10),
        y_calo = Var("superCluster().seed().position().y()",float,doc="photon supercluster position on calorimeter, y coordinate (cm)",precision=10),
        z_calo = Var("superCluster().seed().position().z()",float,doc="photon supercluster position on calorimeter, z coordinate (cm)",precision=10),
        # ES variables
        esEffSigmaRR = Var("full5x5_showerShapeVariables().effSigmaRR()", float, doc="preshower sigmaRR"),
        esEnergyOverRawE = Var("superCluster().preshowerEnergy()/superCluster().rawEnergy()", float, doc="ratio of preshower energy to raw supercluster energy"),
        haloTaggerMVAVal = Var("haloTaggerMVAVal()",float,doc="Value of MVA based BDT based  beam halo tagger in the Ecal endcap (valid for pT > 200 GeV)",precision=8),
    )
)


#these eras need to make the energy correction, hence the "New"
(run2_egamma_2016 | run2_egamma_2017 | run2_egamma_2018).toModify(
    photonTable.variables,
    pt = Var("pt*userFloat('ecalEnergyPostCorrNew')/userFloat('ecalEnergyPreCorrNew')", float, precision=-1, doc="p_{T}"),
    energyErr = Var("userFloat('ecalEnergyErrPostCorrNew')",float,doc="energy error of the cluster from regression",precision=6),
    eCorr = Var("userFloat('ecalEnergyPostCorrNew')/userFloat('ecalEnergyPreCorrNew')",float,doc="ratio of the calibrated energy/miniaod energy"),
    hoe = Var("hadTowOverEm()",float,doc="H over E (Run2)",precision=8),
)

photonsMCMatchForTable = cms.EDProducer("MCMatcher",  # cut on deltaR, deltaPt/Pt; pick best by deltaR
    src         = photonTable.src,                 # final reco collection
    matched     = cms.InputTag("finalGenParticles"), # final mc-truth particle collection
    mcPdgId     = cms.vint32(11,22),                 # one or more PDG ID (11 = el, 22 = pho); absolute values (see below)
    checkCharge = cms.bool(False),              # True = require RECO and MC objects to have the same charge
    mcStatus    = cms.vint32(1),                # PYTHIA status code (1 = stable, 2 = shower, 3 = hard scattering)
    maxDeltaR   = cms.double(0.3),              # Minimum deltaR for the match
    maxDPtRel   = cms.double(0.5),              # Minimum deltaPt/Pt for the match
    resolveAmbiguities    = cms.bool(True),     # Forbid two RECO objects to match to the same GEN object
    resolveByMatchQuality = cms.bool(True),    # False = just match input in order; True = pick lowest deltaR pair first
)

photonMCTable = cms.EDProducer("CandMCMatchTableProducer",
    src     = photonTable.src,
    mcMap   = cms.InputTag("photonsMCMatchForTable"),
    objName = photonTable.name,
    objType = photonTable.name, #cms.string("Photon"),
    branchName = cms.string("genPart"),
    docString = cms.string("MC matching to status==1 photons or electrons"),
)

from RecoEgamma.EgammaTools.egammaObjectModificationsInMiniAOD_cff import egamma8XObjectUpdateModifier,egamma9X105XUpdateModifier,prependEgamma8XObjectUpdateModifier
#we have dataformat changes to 106X so to read older releases we use egamma updators
slimmedPhotonsTo106X = cms.EDProducer("ModifiedPhotonProducer",
    src = cms.InputTag("slimmedPhotons"),
    modifierConfig = cms.PSet( modifications = cms.VPSet(egamma9X105XUpdateModifier) )
)


#adding 4 most imp scale & smearing variables to table
(run2_egamma_2016 | run2_egamma_2017 | run2_egamma_2018).toModify(
    photonTable.variables,
    dEscaleUp=Var("userFloat('ecalEnergyPostCorrNew') - userFloat('energyScaleUpNew')", float, doc="ecal energy scale shifted 1 sigma up (adding gain/stat/syst in quadrature)", precision=8),
    dEscaleDown=Var("userFloat('ecalEnergyPostCorrNew') - userFloat('energyScaleDownNew')", float, doc="ecal energy scale shifted 1 sigma down (adding gain/stat/syst in quadrature)", precision=8),
    dEsigmaUp=Var("userFloat('ecalEnergyPostCorrNew') - userFloat('energySigmaUpNew')", float, doc="ecal energy smearing value shifted 1 sigma up", precision=8),
    dEsigmaDown=Var("userFloat('ecalEnergyPostCorrNew') - userFloat('energySigmaDownNew')", float, doc="ecal energy smearing value shifted 1 sigma up", precision=8),
)

photonTask = cms.Task(bitmapVIDForPho, bitmapVIDForPhoFall17V2, isoForPho, hOverEForPho, isoForPhoFall17V2, seedGainPho, calibratedPatPhotonsNano, slimmedPhotonsWithUserData, finalPhotons)
photonTablesTask = cms.Task(photonTable)
photonMCTask = cms.Task(photonsMCMatchForTable, photonMCTable)


