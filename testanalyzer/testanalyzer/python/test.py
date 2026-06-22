import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing
import os
dir_path = os.path.dirname(os.path.realpath(__file__))

# [STEP 1] Setup Options and Process
options = VarParsing.VarParsing('analysis')
options.outputFile = 'test_0p2to5000_v3.root'
options.maxEvents  = 10
options.parseArguments()

process = cms.Process("testanalyzer")

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 1000
process.maxEvents = cms.untracked.PSet(input = cms.untracked.int32(options.maxEvents))

# [STEP 2] Detector Geometry and Magnetic Field
# Required for Track propagation and Chi2 calculations
process.load('Configuration.StandardSequences.Services_cff')
process.load('Configuration.StandardSequences.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')

from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, 'auto:phase1_2024_realistic', '') 

# [STEP 3] Input Source
# MINIMAL CHANGE: Use command line files if provided, otherwise use hardcoded list
if len(options.inputFiles) > 0:
    file_list = options.inputFiles
else:
    file_list = []
    file_names = [dir_path + '/../file_list_0p2to10.txt',
                    dir_path + '/../file_list_0p2to200.txt',
                  dir_path + '/../file_list_200to500.txt',
                  dir_path + '/../file_list_500to5000.txt']
    for file_name in file_names:
        if os.path.exists(file_name):
            with open(file_name) as f:
                lines = f.read().splitlines()
                file_list += lines[:5]

process.source = cms.Source("PoolSource",
    fileNames = cms.untracked.vstring(
        # [
        # 'file:///afs/cern.ch/work/v/vle/CMSSW_15_0_19/src/testanalyzer/testanalyzer/python/GEN-SIM-RECO_sample_0p2to200.root',
        # 'file:///afs/cern.ch/work/v/vle/CMSSW_15_0_19/src/testanalyzer/testanalyzer/python/GEN-SIM-RECO_sample_200to500.root',
        # 'file:///afs/cern.ch/work/v/vle/CMSSW_15_0_19/src/testanalyzer/testanalyzer/python/GEN-SIM-RECO_sample_500to5000.root',
        # ]
        file_list
    )
)

# [STEP 4] TrackingParticle Reconstitution
# Re-creates simulation truth "TrackingParticles" from digitizer information
process.load("SimGeneral.MixingModule.mixNoPU_cfi")
process.load("SimGeneral.MixingModule.trackingTruthProducerSelection_cfi")
process.trackingParticles.simHitCollections = cms.PSet( )
process.mix.playback = cms.untracked.bool(False) 
process.mix.digitizers = cms.PSet(
     mergedtruth = cms.PSet(process.trackingParticles)
)
for a in process.aliases: delattr(process, a)

# [STEP 5] Track Associator Configuration
# Defines the logic for matching Reco-to-Sim using Chi2 parameters
process.load("SimTracker.TrackAssociation.trackingParticleRecoTrackAsssociation_cfi")
process.load('SimTracker.TrackAssociatorProducers.trackAssociatorByChi2_cfi')
# Point the association producer to use the Chi2 method instead of the default Hit method
process.trackingParticleRecoTrackAsssociation.associator = cms.InputTag("trackAssociatorByChi2")


# [STEP 6] User Analyzer
process.test = cms.EDAnalyzer("testanalyzer")

# [STEP 7] Execution Path
# Ordered: Truth Reconstitution -> Math Associator -> Association Map -> User Code
process.p = cms.Path(
    process.mix *     
    process.trackAssociatorByChi2 *     
    process.trackingParticleRecoTrackAsssociation *   
    process.test                                    
)

# [STEP 8] Output Service
process.TFileService = cms.Service("TFileService",
    fileName = cms.string(options.outputFile)
)