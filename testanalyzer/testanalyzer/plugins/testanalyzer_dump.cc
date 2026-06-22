#include <memory>
#include <iostream>
#include <map>
#include <vector>

// Framework
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

// Data Formats
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticle.h"
#include "SimDataFormats/Associations/interface/TrackToTrackingParticleAssociator.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlock.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElement.h"
#include "DataFormats/ParticleFlowReco/interface/PFCluster.h"

#include "TTree.h"

class testanalyzer_dump : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
    explicit testanalyzer_dump(const edm::ParameterSet&);
    ~testanalyzer_dump() override = default;

private:
    void beginJob() override;
    void analyze(const edm::Event&, const edm::EventSetup&) override;

    // Tokens
    edm::EDGetTokenT<std::vector<reco::GenParticle>> gen_token_;
    edm::EDGetTokenT<std::vector<TrackingParticle>> tp_token_;
    edm::EDGetTokenT<reco::SimToRecoCollection> sim_to_reco_token_;
    edm::EDGetTokenT<reco::PFCandidateCollection> pf_cand_token_;

    TTree* tree_;

    // --- Tree Variables (Snake Case) ---
    int   gen_id_;
    float gen_pt_, gen_eta_, gen_energy_;

    int   tp_id_;
    float tp_pt_, tp_eta_, tp_energy_;

    float track_pt_, track_eta_;

    int   pf_id_;
    float pf_ecal_energy_, pf_hcal_energy_; // Final candidate calibrated energy

    // Raw Cluster info extracted directly from the Block Element
    float ecal_cluster_pt_, ecal_cluster_eta_, ecal_cluster_energy_; 
    float hcal_cluster_pt_, hcal_cluster_eta_, hcal_cluster_energy_;

    int   pf_block_id_;
    int   tracks_in_block_;
    int   gen_in_block_;
};

testanalyzer_dump::testanalyzer_dump(const edm::ParameterSet& iConfig)
    : gen_token_(consumes<std::vector<reco::GenParticle>>(edm::InputTag("genParticles"))),
      tp_token_(consumes<std::vector<TrackingParticle>>(edm::InputTag("mix", "MergedTrackTruth"))),
      sim_to_reco_token_(consumes<reco::SimToRecoCollection>(edm::InputTag("trackingParticleRecoTrackAsssociation"))),
      pf_cand_token_(consumes<reco::PFCandidateCollection>(edm::InputTag("particleFlow"))) {
    usesResource("TFileService");
}

void testanalyzer_dump::beginJob() {
    edm::Service<TFileService> fs;
    tree_ = fs->make<TTree>("tree", "physics_dump");

    // Gen Branches
    tree_->Branch("gen_id", &gen_id_, "gen_id/I");
    tree_->Branch("gen_pt", &gen_pt_, "gen_pt/F");
    tree_->Branch("gen_eta", &gen_eta_, "gen_eta/F");
    tree_->Branch("gen_energy", &gen_energy_, "gen_energy/F");

    // Sim Branches
    tree_->Branch("tp_id", &tp_id_, "tp_id/I");
    tree_->Branch("tp_pt", &tp_pt_, "tp_pt/F");
    tree_->Branch("tp_eta", &tp_eta_, "tp_eta/F");
    tree_->Branch("tp_energy", &tp_energy_, "tp_energy/F");

    // Track Branches
    tree_->Branch("track_pt", &track_pt_, "track_pt/F");
    tree_->Branch("track_eta", &track_eta_, "track_eta/F");

    // PF Candidate Branches
    tree_->Branch("pf_id", &pf_id_, "pf_id/I");
    tree_->Branch("pf_ecal_energy", &pf_ecal_energy_, "pf_ecal_energy/F");
    tree_->Branch("pf_hcal_energy", &pf_hcal_energy_, "pf_hcal_energy/F");

    // Block Cluster Branches
    tree_->Branch("ecal_cluster_pt", &ecal_cluster_pt_, "ecal_cluster_pt/F");
    tree_->Branch("ecal_cluster_eta", &ecal_cluster_eta_, "ecal_cluster_eta/F");
    tree_->Branch("ecal_cluster_energy", &ecal_cluster_energy_, "ecal_cluster_energy/F");

    tree_->Branch("hcal_cluster_pt", &hcal_cluster_pt_, "hcal_cluster_pt/F");
    tree_->Branch("hcal_cluster_eta", &hcal_cluster_eta_, "hcal_cluster_eta/F");
    tree_->Branch("hcal_cluster_energy", &hcal_cluster_energy_, "hcal_cluster_energy/F");

    // Block Meta Branches
    tree_->Branch("pf_block_id", &pf_block_id_, "pf_block_id/I");
    tree_->Branch("tracks_in_block", &tracks_in_block_, "tracks_in_block/I");
    tree_->Branch("gen_in_block", &gen_in_block_, "gen_in_block/I");
}

void testanalyzer_dump::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
    // --- STEP 1: Fetch all data handles from the event ---
    auto gen_handle = iEvent.getHandle(gen_token_);
    auto tp_handle = iEvent.getHandle(tp_token_);
    auto sim_to_reco_handle = iEvent.getHandle(sim_to_reco_token_);
    auto pf_cand_handle = iEvent.getHandle(pf_cand_token_);

    // --- STEP 2: Pre-build Lookup Maps to avoid O(N^2) loops later ---
    
    // Map A: Link GenParticle to TrackingParticle
    std::map<reco::GenParticleRef, TrackingParticleRef> gen_to_tp;
    for (size_t i = 0; i < tp_handle->size(); ++i) {
        TrackingParticleRef tp_ref(tp_handle, i);
        for (const auto& gen_ref : tp_ref->genParticles()) {
            gen_to_tp[gen_ref] = tp_ref;
        }
    }

    // Map B: Link Reco Track to PF Candidate
    std::map<unsigned int, reco::PFCandidateRef> track_to_pf;
    for (size_t i = 0; i < pf_cand_handle->size(); ++i) {
        reco::PFCandidateRef pf_ref(pf_cand_handle, i);
        if (pf_ref->trackRef().isNonnull()) {
            track_to_pf[pf_ref->trackRef().key()] = pf_ref;
        }
    }

    // Map C: Count how many unique GenParticles feed into each PFBlock
    std::map<int, int> block_gen_count;
    for (auto const& [gen_ref, tp_ref] : gen_to_tp) {
        if (sim_to_reco_handle->find(tp_ref) != sim_to_reco_handle->end()) {
            auto matches = (*sim_to_reco_handle)[tp_ref];
            if (!matches.empty()) {
                auto trk_key = matches.begin()->first.key();
                if (track_to_pf.count(trk_key)) {
                    auto pf_ref = track_to_pf[trk_key];
                    if (!pf_ref->elementsInBlocks().empty()) {
                        // Using elementsInBlocks() to get the block reference
                        block_gen_count[pf_ref->elementsInBlocks()[0].first.key()]++;
                    }
                }
            }
        }
    }

    // --- STEP 3: Main Physics Loop (Iterate over Truth) ---
    for (size_t i = 0; i < gen_handle->size(); ++i) {
        reco::GenParticleRef gen_ref(gen_handle, i);
        
        // Filter out unstable or neutral particles that don't leave tracks
        if (gen_ref->status() != 1 || gen_ref->charge() == 0) continue;

        // --- STEP 4: Reset all variables to dummy values (-999.0) ---
        // This fully replaces the need for "has_track", "has_tp", etc.
        gen_id_ = gen_ref->pdgId(); 
        gen_pt_ = gen_ref->pt(); gen_eta_ = gen_ref->eta(); gen_energy_ = gen_ref->energy();
        
        tp_id_ = 0; tp_pt_ = -999.0; tp_eta_ = -999.0; tp_energy_ = -999.0;
        track_pt_ = -999.0; track_eta_ = -999.0;
        
        pf_id_ = 0; pf_ecal_energy_ = -999.0; pf_hcal_energy_ = -999.0;
        
        ecal_cluster_pt_ = -999.0; ecal_cluster_eta_ = -999.0; ecal_cluster_energy_ = -999.0;
        hcal_cluster_pt_ = -999.0; hcal_cluster_eta_ = -999.0; hcal_cluster_energy_ = -999.0;
        
        pf_block_id_ = -1; tracks_in_block_ = 0; gen_in_block_ = 0;

        // --- STEP 5: Trace Truth through the Detector ---
        
        // Did it hit the silicon? (TrackingParticle)
        if (gen_to_tp.count(gen_ref)) {
            auto tp_ref = gen_to_tp[gen_ref];
            tp_id_ = tp_ref->pdgId(); 
            tp_pt_ = tp_ref->pt(); 
            tp_eta_ = tp_ref->eta(); 
            tp_energy_ = tp_ref->energy();

            // Did the software find the track? (Reco Track)
            if (sim_to_reco_handle->find(tp_ref) != sim_to_reco_handle->end()) {
                auto matches = (*sim_to_reco_handle)[tp_ref];
                if (!matches.empty()) {
                    auto trk_ref = matches.begin()->first;
                    track_pt_ = trk_ref->pt();
                    track_eta_ = trk_ref->eta();

                    // Did Particle Flow link it to Calorimeters? (PFCandidate)
                    if (track_to_pf.count(trk_ref.key())) {
                        auto pf_ref = track_to_pf[trk_ref.key()];
                        pf_id_ = pf_ref->particleId();
                        pf_ecal_energy_ = pf_ref->ecalEnergy(); 
                        pf_hcal_energy_ = pf_ref->hcalEnergy();

                        auto const& active_elements = pf_ref->elementsInBlocks();
                        if (!active_elements.empty()) {
                            
                            // Get Block Metadata
                            auto block_ref = active_elements[0].first;
                            pf_block_id_ = block_ref.key();
                            gen_in_block_ = block_gen_count[pf_block_id_];
                            
                            // Count total tracks inside this entire physical block
                            for (auto const& element : block_ref->elements()) {
                                if (element.type() == reco::PFBlockElement::TRACK) tracks_in_block_++;
                            }

                            // --- STEP 6: Extract Raw Cluster Info from the Block Elements ---
                            // We loop specifically through the elements *linked to this candidate*
                            for (auto const& idx_pair : active_elements) {
                                // Follow the Ref to the Block, then index into its element array
                                auto const& block_element = idx_pair.first->elements()[idx_pair.second];
                                
                                if (block_element.type() == reco::PFBlockElement::ECAL && block_element.clusterRef().isNonnull()) {
                                    ecal_cluster_pt_ = block_element.clusterRef()->pt();
                                    ecal_cluster_eta_ = block_element.clusterRef()->eta();
                                    // Extract the corrected energy directly from the raw cluster
                                    ecal_cluster_energy_ = block_element.clusterRef()->correctedEnergy(); 
                                }
                                
                                if (block_element.type() == reco::PFBlockElement::HCAL && block_element.clusterRef().isNonnull()) {
                                    hcal_cluster_pt_ = block_element.clusterRef()->pt();
                                    hcal_cluster_eta_ = block_element.clusterRef()->eta();
                                    // Extract the corrected energy directly from the raw cluster
                                    hcal_cluster_energy_ = block_element.clusterRef()->correctedEnergy();
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // --- STEP 7: Save to TTree ---
        tree_->Fill();
    }
}

DEFINE_FWK_MODULE(testanalyzer_dump);