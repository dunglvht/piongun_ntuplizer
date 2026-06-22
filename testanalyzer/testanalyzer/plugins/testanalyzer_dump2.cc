// === STEP 1: Includes and Framework Setup ===
#include <memory>
#include <iostream>
#include <map>
#include <vector>
#include <cmath>

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
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/Math/interface/deltaPhi.h"

// Propagation (for Swimming)
#include "CommonTools/BaseParticlePropagator/interface/BaseParticlePropagator.h"
#include "CommonTools/BaseParticlePropagator/interface/RawParticle.h"

#include "TTree.h"


// === STEP 2: Class Definition and Structs ===
class testanalyzer_dump2 : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
    explicit testanalyzer_dump2(const edm::ParameterSet&);
    ~testanalyzer_dump2() override = default;

private:
    void beginJob() override;
    void analyze(const edm::Event&, const edm::EventSetup&) override;

    // Internal structs to neatly group data for the TTrees
    struct gen_data_struct   { int id; float pt, eta, energy; };
    struct tp_data_struct    { int id; float pt, eta, energy; };
    struct track_data_struct { float pt, eta; };
    struct match_data_struct { 
        int id; 
        float raw_ecal, calib_ecal, raw_hcal, calib_hcal;
        float ecal_cluster_energy, ecal_cluster_eta; 
        float hcal_cluster_energy, hcal_cluster_eta; 
        float dr;
        int block_id, tracks_in_block, gen_in_block;
    };

    // Helper function to extract PF info
    void fill_match_info(reco::PFCandidateRef pf_ref, match_data_struct& data, const std::map<int, int>& gen_map, float dr_val);

    // Tokens
    edm::EDGetTokenT<std::vector<reco::GenParticle>> gen_token_;
    edm::EDGetTokenT<std::vector<TrackingParticle>> tp_token_;
    edm::EDGetTokenT<reco::SimToRecoCollection> sim_to_reco_token_;
    edm::EDGetTokenT<reco::PFCandidateCollection> pf_candidate_token_;
    edm::EDGetTokenT<reco::PFBlockCollection> pf_block_token_;

    // TTree Pointers
    TTree *gen_tree_, *tp_tree_, *track_tree_, *pf_link_tree_, *asym_tree_, *swim_tree_,*block_search_tree_;
    
    // Variables holding the current event data
    gen_data_struct gen_ref_data_;
    tp_data_struct  tp_ref_data_;
    track_data_struct track_ref_data_;
    match_data_struct pf_link_ref_data_, asym_ref_data_, swim_ref_data_, block_search_ref_data_;
};


// === STEP 3: Constructor and TTree Setup ===
testanalyzer_dump2::testanalyzer_dump2(const edm::ParameterSet& iConfig)
    : gen_token_(consumes<std::vector<reco::GenParticle>>(edm::InputTag("genParticles"))),
      tp_token_(consumes<std::vector<TrackingParticle>>(edm::InputTag("mix", "MergedTrackTruth"))),
      sim_to_reco_token_(consumes<reco::SimToRecoCollection>(edm::InputTag("trackingParticleRecoTrackAsssociation"))),
      pf_candidate_token_(consumes<reco::PFCandidateCollection>(edm::InputTag("particleFlow"))),
      pf_block_token_(consumes<reco::PFBlockCollection>(edm::InputTag("particleFlowBlock"))){
    
    usesResource("TFileService");
}

void testanalyzer_dump2::beginJob() {
    edm::Service<TFileService> fs;
    
    // Create a top-level directory in the ROOT file
    TFileDirectory tree_dir = fs->mkdir("tree");

    // Initialize the 6 synchronous TTrees
    gen_tree_     = tree_dir.make<TTree>("gen", "gen");
    tp_tree_      = tree_dir.make<TTree>("tp", "tp");
    track_tree_   = tree_dir.make<TTree>("track", "track");
    pf_link_tree_ = tree_dir.make<TTree>("match_pf_link", "match_pf_link");
    asym_tree_    = tree_dir.make<TTree>("match_asym", "match_asym");
    swim_tree_    = tree_dir.make<TTree>("match_swim", "match_swim");
    block_search_tree_ = tree_dir.make<TTree>("match_block_search", "match_block_search");
    // Lambda helper to set up the 3 identical match trees
    auto setup_match_tree = [](TTree* t, match_data_struct& d) {
        t->Branch("id", &d.id, "id/I");
        t->Branch("raw_ecal", &d.raw_ecal, "raw_ecal/F");
        t->Branch("calib_ecal", &d.calib_ecal, "calib_ecal/F");
        t->Branch("raw_hcal", &d.raw_hcal, "raw_hcal/F");
        t->Branch("calib_hcal", &d.calib_hcal, "calib_hcal/F");
        t->Branch("ecal_cluster_energy", &d.ecal_cluster_energy, "ecal_cluster_energy/F");
        t->Branch("ecal_cluster_eta", &d.ecal_cluster_eta, "ecal_cluster_eta/F");
        t->Branch("hcal_cluster_energy", &d.hcal_cluster_energy, "hcal_cluster_energy/F");
        t->Branch("hcal_cluster_eta", &d.hcal_cluster_eta, "hcal_cluster_eta/F");
        t->Branch("dr", &d.dr, "dr/F");
        t->Branch("block_id", &d.block_id, "block_id/I");
        t->Branch("tracks_in_block", &d.tracks_in_block, "tracks_in_block/I");
        t->Branch("gen_in_block", &d.gen_in_block, "gen_in_block/I");
    };

    // Setup Gen Tree Branches
    gen_tree_->Branch("id", &gen_ref_data_.id, "id/I");
    gen_tree_->Branch("pt", &gen_ref_data_.pt, "pt/F");
    gen_tree_->Branch("eta", &gen_ref_data_.eta, "eta/F");
    gen_tree_->Branch("energy", &gen_ref_data_.energy, "energy/F");

    // Setup Sim/TP Tree Branches
    tp_tree_->Branch("id", &tp_ref_data_.id, "id/I");
    tp_tree_->Branch("pt", &tp_ref_data_.pt, "pt/F");
    tp_tree_->Branch("eta", &tp_ref_data_.eta, "eta/F");
    tp_tree_->Branch("energy", &tp_ref_data_.energy, "energy/F");

    // Setup Track Tree Branches
    track_tree_->Branch("pt", &track_ref_data_.pt, "pt/F");
    track_tree_->Branch("eta", &track_ref_data_.eta, "eta/F");

    // Setup the 3 Match Trees
    setup_match_tree(pf_link_tree_, pf_link_ref_data_);
    setup_match_tree(asym_tree_, asym_ref_data_);
    setup_match_tree(swim_tree_, swim_ref_data_);
    setup_match_tree(block_search_tree_, block_search_ref_data_);

}


// === STEP 4: Helper Function to Fill PF Data ===
void testanalyzer_dump2::fill_match_info(reco::PFCandidateRef pf_ref, match_data_struct& d, const std::map<int, int>& gen_map, float dr_val) {
    d.id = pf_ref->particleId(); 
    d.dr = dr_val;
    d.raw_ecal = (float)pf_ref->rawEcalEnergy(); 
    d.calib_ecal = (float)pf_ref->ecalEnergy();
    d.raw_hcal = (float)pf_ref->rawHcalEnergy(); 
    d.calib_hcal = (float)pf_ref->hcalEnergy();
    
    // Look inside the PFBlock
    if (!pf_ref->elementsInBlocks().empty()) {
        auto b_ref = pf_ref->elementsInBlocks()[0].first;
        d.block_id = b_ref.key();
        d.gen_in_block = gen_map.count(d.block_id) ? gen_map.at(d.block_id) : 0;
        d.tracks_in_block = 0;
        
        // Count tracks and extract raw cluster kinematics
            for (auto const& el : b_ref->elements()) {
                    if (el.type() == reco::PFBlockElement::TRACK) d.tracks_in_block++;
                    
                    if (el.type() == reco::PFBlockElement::ECAL && el.clusterRef().isNonnull()) {
                        d.ecal_cluster_energy = (float)el.clusterRef()->energy();
                        d.ecal_cluster_eta = (float)el.clusterRef()->eta();
                    }
                    if (el.type() == reco::PFBlockElement::HCAL && el.clusterRef().isNonnull()) {
                        d.hcal_cluster_energy = (float)el.clusterRef()->energy();
                        d.hcal_cluster_eta = (float)el.clusterRef()->eta();
                    }
    }
    }
}


// === STEP 5: Analyze Method - Event Data Extraction ===
void testanalyzer_dump2::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
    auto gen_handle = iEvent.getHandle(gen_token_);
    auto tp_handle = iEvent.getHandle(tp_token_);
    auto sim_to_reco_handle = iEvent.getHandle(sim_to_reco_token_);
    auto pf_handle = iEvent.getHandle(pf_candidate_token_);
    auto block_handle = iEvent.getHandle(pf_block_token_);

    // === STEP 6: Pre-computation & Mapping (Speeds up loops) ===
    std::map<reco::GenParticleRef, TrackingParticleRef> gen_to_tp_map;
    std::map<unsigned int, reco::PFCandidateRef> track_to_pf_map;
    std::map<int, int> block_gen_count;

    // Link Tracks to PF Candidates
    for (size_t i = 0; i < pf_handle->size(); ++i) {
        reco::PFCandidateRef r(pf_handle, i);
        if (r->trackRef().isNonnull()) track_to_pf_map[r->trackRef().key()] = r;
    }
    
    // Link Gen to TP, and count Gen particles inside specific PFBlocks
    for (size_t i = 0; i < tp_handle->size(); ++i) {
        TrackingParticleRef tp_ref(tp_handle, i);
        for (const auto& gr : tp_ref->genParticles()) {
            gen_to_tp_map[gr] = tp_ref;
            if (sim_to_reco_handle->find(tp_ref) != sim_to_reco_handle->end()) {
                auto m = (*sim_to_reco_handle)[tp_ref];
                if (!m.empty() && track_to_pf_map.count(m.begin()->first.key())) {
                    auto pf = track_to_pf_map[m.begin()->first.key()];
                    if (!pf->elementsInBlocks().empty()) {
                        block_gen_count[pf->elementsInBlocks()[0].first.key()]++;
                    }
                }
            }
        }
    }


    // === STEP 7: Main GenParticle Loop & Reset ===
    for (size_t i = 0; i < gen_handle->size(); ++i) {
        reco::GenParticleRef gen_ref(gen_handle, i);
        
        // Filter out unstable or neutral particles
        if (gen_ref->status() != 1 || gen_ref->charge() == 0) continue;

        // Reset all variables to safely handle missing data (-1 or -999.0)
        gen_ref_data_   = {gen_ref->pdgId(), (float)gen_ref->pt(), (float)gen_ref->eta(), (float)gen_ref->energy()};
        tp_ref_data_    = {-1, -1.0, -1.0, -1.0};
        track_ref_data_ = {-1.0, -1.0};
        match_data_struct empty_m = {-1, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1, 0, 0};
        pf_link_ref_data_ = asym_ref_data_ = swim_ref_data_ = block_search_ref_data_ = empty_m;


        // === STEP 8: Software Matching (Gen -> Sim -> Track -> PF) ===
        if (gen_to_tp_map.count(gen_ref)) {
            auto tp = gen_to_tp_map[gen_ref];
            tp_ref_data_ = {tp->pdgId(), (float)tp->pt(), (float)tp->eta(), (float)tp->energy()};
            
            if (sim_to_reco_handle->find(tp) != sim_to_reco_handle->end()) {
                auto matches = (*sim_to_reco_handle)[tp];
                if (!matches.empty()) {
                    auto trk = matches.begin()->first;
                    track_ref_data_ = {(float)trk->pt(), (float)trk->eta()};
                    
                    // If track is found, get the associated PF candidate
                    if (track_to_pf_map.count(trk.key())) {
                        fill_match_info(track_to_pf_map[trk.key()], pf_link_ref_data_, block_gen_count, -1.0);
                    }
                }
            }
        }
        

        // === STEP 9: Fallback Match 1 (Asymmetric Box Window) ===
        // Finds candidates using a tight Eta and loose Phi window to account for curve
        float best_asym_dr = 999.0;
        for (size_t j = 0; j < pf_handle->size(); ++j) {
            reco::PFCandidateRef pf(pf_handle, j);
            float d_eta = std::abs((float)gen_ref->eta() - (float)pf->eta());
            float d_phi = std::abs((float)reco::deltaPhi(gen_ref->phi(), pf->phi()));
            
            if (d_eta < 0.05 && d_phi < 0.3) {
                float dr = std::sqrt(d_eta*d_eta + d_phi*d_phi);
                if (dr < best_asym_dr) { 
                    best_asym_dr = dr; 
                    fill_match_info(pf, asym_ref_data_, block_gen_count, dr); 
                }
            }
        }


        // === STEP 10: Fallback Match 2 (Magnetic Field Swim) ===
        // Propagates the particle mathematically to the HCAL entrance
        math::XYZTLorentzVector v4(gen_ref->vx(), gen_ref->vy(), gen_ref->vz(), 0.0);
        RawParticle rp(gen_ref->p4(), v4, (double)gen_ref->charge());
        BaseParticlePropagator propagator(rp, 0., 0., 3.8); // 3.8T Magnetic Field
        propagator.propagateToHcalEntrance();
        
        if (propagator.getSuccess() != 0) {
            float swim_eta = (float)propagator.particle().eta(); 
            float swim_phi = (float)propagator.particle().phi();
            float best_swim_dr = 999.0;
            
            for (size_t j = 0; j < pf_handle->size(); ++j) {
                reco::PFCandidateRef pf(pf_handle, j);
                float dr = (float)reco::deltaR(swim_eta, swim_phi, pf->eta(), pf->phi());
                
                if (dr < 0.1 && dr < best_swim_dr) { 
                    best_swim_dr = dr; 
                    fill_match_info(pf, swim_ref_data_, block_gen_count, dr); 
                }
            }
        }

// === STEP 10.5: Fallback Match 3 (Direct PFBlock Search) ===
        // If the PF Candidate was rejected, the track might still exist inside a raw PFBlock.
        auto block_handle = iEvent.getHandle(pf_block_token_);
        
        // Logic: If we have a TrackingParticle match for this GenParticle, look for the block
        if (gen_to_tp_map.count(gen_ref)) {
            auto tp_ref = gen_to_tp_map[gen_ref];

            // Check if this TP has an associated reco track
            if (sim_to_reco_handle->find(tp_ref) != sim_to_reco_handle->end()) {
                auto track_matches = (*sim_to_reco_handle)[tp_ref];
                
                if (!track_matches.empty()) {
                    auto target_track_ref = track_matches.begin()->first;
                    bool block_match_found = false;

                    // Loop over all raw PFBlocks in the event
                    for (size_t b_idx = 0; b_idx < block_handle->size(); ++b_idx) {
                        reco::PFBlockRef block_ref(block_handle, b_idx);
                        
                        // Loop over all elements inside this specific block
                        for (const auto& element : block_ref->elements()) {
                            
                            // Match by comparing the unique key of the track
                            if (element.type() == reco::PFBlockElement::TRACK && 
                                element.trackRef().isNonnull() && 
                                element.trackRef().key() == target_track_ref.key()) {
                                
                                block_match_found = true;
                                block_search_ref_data_.block_id = block_ref.key();
                                
                                // Get the Gen count from our pre-computed map
                                if (block_gen_count.count(block_search_ref_data_.block_id)) {
                                    block_search_ref_data_.gen_in_block = block_gen_count[block_search_ref_data_.block_id];
                                } else {
                                    block_search_ref_data_.gen_in_block = 0;
                                }

                                // Iterate through elements to count total tracks in this block
                                block_search_ref_data_.tracks_in_block = 0;
                                for (const auto& internal_element : block_ref->elements()) {
                                        if (internal_element.type() == reco::PFBlockElement::TRACK) {
                                            block_search_ref_data_.tracks_in_block++;
                                        }
                                        
                                        if (internal_element.type() == reco::PFBlockElement::ECAL && internal_element.clusterRef().isNonnull()) {
                                            block_search_ref_data_.ecal_cluster_energy = (float)internal_element.clusterRef()->energy();
                                            block_search_ref_data_.ecal_cluster_eta = (float)internal_element.clusterRef()->eta();
                                        }
                                        
                                        if (internal_element.type() == reco::PFBlockElement::HCAL && internal_element.clusterRef().isNonnull()) {
                                            block_search_ref_data_.hcal_cluster_energy = (float)internal_element.clusterRef()->energy();
                                            block_search_ref_data_.hcal_cluster_eta = (float)internal_element.clusterRef()->eta();
                                        }
                                    }
                                break; // Found our track, stop looking in this block
                            }
                        }
                        if (block_match_found) break; // Found our block, stop looking at other blocks
                    }
                }
            }
        }


        // === STEP 11: Fill TTrees Synchronously ===
        // Ensures entry index 'N' aligns perfectly across all 6 trees
        gen_tree_->Fill(); 
        tp_tree_->Fill(); 
        track_tree_->Fill();
        pf_link_tree_->Fill(); 
        asym_tree_->Fill(); 
        swim_tree_->Fill();
        block_search_tree_->Fill();
    }
}

// Define this as a standard CMSSW plug-in
DEFINE_FWK_MODULE(testanalyzer_dump2);