// === Includes and Framework Setup ===
#include <memory>
#include <iostream>
#include <map>
#include <vector>
#include <cmath>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticle.h"
#include "SimDataFormats/Associations/interface/TrackToTrackingParticleAssociator.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlock.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElement.h"
#include "DataFormats/ParticleFlowReco/interface/PFCluster.h" 
#include "DataFormats/Math/interface/deltaR.h"

#include "TTree.h"

// === Class Definition and Data Structures ===
class testanalyzerdump_4 : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
    explicit testanalyzerdump_4(const edm::ParameterSet&);
    ~testanalyzerdump_4() override = default;

private:
    void beginJob() override;
    void analyze(const edm::Event&, const edm::EventSetup&) override;

    struct match_data { 
        int id; 
        float pf_ecal_energy, pf_hcal_energy;
        float pf_raw_ecal_energy, pf_raw_hcal_energy;
        float ecal_sum_energy, ecal_max_energy, ecal_closest_energy, ecal_closest_dr;
        float hcal_sum_energy, hcal_max_energy, hcal_closest_energy, hcal_closest_dr;
        int block_id, tracks_in_block, gen_in_block;

        void reset() {
            id = -1;
            pf_ecal_energy = -1.0;
            pf_hcal_energy = -1.0;
            pf_raw_ecal_energy = -1.0;
            pf_raw_hcal_energy = -1.0;
            ecal_sum_energy = -1.0;
            ecal_max_energy = -1.0;
            ecal_closest_energy = -1.0;
            ecal_closest_dr = 999.0;
            hcal_sum_energy = -1.0;
            hcal_max_energy = -1.0;
            hcal_closest_energy = -1.0;
            hcal_closest_dr = 999.0;
            block_id = -1;
            tracks_in_block = 0;
            gen_in_block = 0;
        }
    };

    void fill_aggregated_block_info(reco::PFBlockRef block_ref, edm::RefToBase<reco::Track> track_ref, match_data& data, const std::map<int, int>& gen_in_block_map);

    // Hardcoded Tokens
    edm::EDGetTokenT<std::vector<reco::GenParticle>> gen_particle_token_;
    edm::EDGetTokenT<std::vector<TrackingParticle>> tracking_particle_token_;
    edm::EDGetTokenT<reco::SimToRecoCollection> sim_to_reco_token_;
    edm::EDGetTokenT<reco::PFCandidateCollection> pf_candidate_token_;
    edm::EDGetTokenT<reco::PFBlockCollection> pf_block_token_;

    TTree *gen_tree_, *track_tree_, *pf_link_tree_, *block_search_tree_;

    // Tree Output Variables
    int   gen_id_;
    float gen_pt_, gen_eta_, gen_energy_;
    float track_pt_, track_eta_;
    match_data pf_link_data_, block_search_data_;
};

// === Constructor and TTree Setup ===
testanalyzerdump_4::testanalyzerdump_4(const edm::ParameterSet& config)
    : gen_particle_token_(consumes<std::vector<reco::GenParticle>>(edm::InputTag("genParticles"))),
      tracking_particle_token_(consumes<std::vector<TrackingParticle>>(edm::InputTag("mix", "MergedTrackTruth"))),
      sim_to_reco_token_(consumes<reco::SimToRecoCollection>(edm::InputTag("trackingParticleRecoTrackAsssociation"))),
      pf_candidate_token_(consumes<reco::PFCandidateCollection>(edm::InputTag("particleFlow"))),
      pf_block_token_(consumes<reco::PFBlockCollection>(edm::InputTag("particleFlowBlock"))) {
    usesResource("TFileService");
}

void testanalyzerdump_4::beginJob() {
    edm::Service<TFileService> file_service;
    TFileDirectory tree_dir = file_service->mkdir("tree");

    gen_tree_          = tree_dir.make<TTree>("gen", "Generator Baseline");
    track_tree_        = tree_dir.make<TTree>("track", "Inner Tracker Guess");
    pf_link_tree_      = tree_dir.make<TTree>("match_pf_link", "Software Pointer Logic");
    block_search_tree_ = tree_dir.make<TTree>("match_block_search", "Block Element Lookup Logic");

    auto setup_match_branches = [](TTree* tree, match_data& data) {
        tree->Branch("id", &data.id, "id/I");
        tree->Branch("pf_ecal_energy", &data.pf_ecal_energy, "pf_ecal_energy/F");
        tree->Branch("pf_hcal_energy", &data.pf_hcal_energy, "pf_hcal_energy/F");
        tree->Branch("pf_raw_ecal_energy", &data.pf_raw_ecal_energy, "pf_raw_ecal_energy/F");
        tree->Branch("pf_raw_hcal_energy", &data.pf_raw_hcal_energy, "pf_raw_hcal_energy/F");
        tree->Branch("ecal_sum_energy", &data.ecal_sum_energy, "ecal_sum_energy/F");
        tree->Branch("ecal_max_energy", &data.ecal_max_energy, "ecal_max_energy/F");
        tree->Branch("ecal_closest_energy", &data.ecal_closest_energy, "ecal_closest_energy/F");
        tree->Branch("ecal_closest_dr", &data.ecal_closest_dr, "ecal_closest_dr/F");
        tree->Branch("hcal_sum_energy", &data.hcal_sum_energy, "hcal_sum_energy/F");
        tree->Branch("hcal_max_energy", &data.hcal_max_energy, "hcal_max_energy/F");
        tree->Branch("hcal_closest_energy", &data.hcal_closest_energy, "hcal_closest_energy/F");
        tree->Branch("hcal_closest_dr", &data.hcal_closest_dr, "hcal_closest_dr/F");
        tree->Branch("block_id", &data.block_id, "block_id/I");
        tree->Branch("tracks_in_block", &data.tracks_in_block, "tracks_in_block/I");
        tree->Branch("gen_in_block", &data.gen_in_block, "gen_in_block/I");
    };

    gen_tree_->Branch("id", &gen_id_, "id/I");
    gen_tree_->Branch("pt", &gen_pt_, "pt/F");
    gen_tree_->Branch("eta", &gen_eta_, "eta/F");
    gen_tree_->Branch("energy", &gen_energy_, "energy/F");

    track_tree_->Branch("pt", &track_pt_, "pt/F"); 
    track_tree_->Branch("eta", &track_eta_, "eta/F");

    setup_match_branches(pf_link_tree_, pf_link_data_);
    setup_match_branches(block_search_tree_, block_search_data_);
}

// === Helper Function Implementation ===
void testanalyzerdump_4::fill_aggregated_block_info(reco::PFBlockRef block_ref, edm::RefToBase<reco::Track> track_ref, match_data& data, const std::map<int, int>& gen_in_block_map) {
    data.block_id = block_ref.key();
    data.gen_in_block = gen_in_block_map.count(data.block_id) ? gen_in_block_map.at(data.block_id) : 0;
    
    float min_ecal_dr = 999.0;
    float min_hcal_dr = 999.0;

    for (const auto& element : block_ref->elements()) {
        if (element.type() == reco::PFBlockElement::TRACK) {
            data.tracks_in_block++;
        }

        if (element.clusterRef().isNonnull()) {
            float energy = element.clusterRef()->energy();
            float delta_r = reco::deltaR(*track_ref, *(element.clusterRef()));

            if (element.type() == reco::PFBlockElement::ECAL) {
                if (data.ecal_sum_energy == -1.0) data.ecal_sum_energy = 0; 
                data.ecal_sum_energy += energy;
                
                if (energy > data.ecal_max_energy) data.ecal_max_energy = energy;
                
                if (delta_r < min_ecal_dr) { 
                    min_ecal_dr = delta_r; 
                    data.ecal_closest_dr = delta_r; 
                    data.ecal_closest_energy = energy; 
                }
            }
            if (element.type() == reco::PFBlockElement::HCAL) {
                if (data.hcal_sum_energy == -1.0) data.hcal_sum_energy = 0; 
                data.hcal_sum_energy += energy;
                
                if (energy > data.hcal_max_energy) data.hcal_max_energy = energy;
                
                if (delta_r < min_hcal_dr) { 
                    min_hcal_dr = delta_r; 
                    data.hcal_closest_dr = delta_r; 
                    data.hcal_closest_energy = energy; 
                }
            }
        }
    }
}

// === Main Analysis Logic ===
void testanalyzerdump_4::analyze(const edm::Event& event, const edm::EventSetup& setup) {
    auto gen_particle_handle = event.getHandle(gen_particle_token_);
    auto tracking_particle_handle = event.getHandle(tracking_particle_token_);
    auto sim_to_reco_handle = event.getHandle(sim_to_reco_token_);
    auto pf_candidate_handle = event.getHandle(pf_candidate_token_);
    auto pf_block_handle = event.getHandle(pf_block_token_);

    std::map<reco::GenParticleRef, TrackingParticleRef> gen_to_tracking_particle_map;
    std::map<unsigned int, reco::PFCandidateRef> track_to_pf_candidate_map;
    
    // Map Reco Tracks to PF Candidates
    for (size_t i = 0; i < pf_candidate_handle->size(); ++i) {
        reco::PFCandidateRef pf_candidate_ref(pf_candidate_handle, i);
        if (pf_candidate_ref->trackRef().isNonnull()) {
            track_to_pf_candidate_map[pf_candidate_ref->trackRef().key()] = pf_candidate_ref;
        }
    }

    std::map<int, int> block_gen_count_map;
    // Map TrackingParticles to GenParticles and count Gens in PF Blocks
    for (size_t i = 0; i < tracking_particle_handle->size(); ++i) {
        TrackingParticleRef tracking_particle_ref(tracking_particle_handle, i);
        for (const auto& gen_ref : tracking_particle_ref->genParticles()) {
            gen_to_tracking_particle_map[gen_ref] = tracking_particle_ref;
            
            if (sim_to_reco_handle->find(tracking_particle_ref) != sim_to_reco_handle->end()) {
                auto tracking_match = (*sim_to_reco_handle)[tracking_particle_ref];
                if (!tracking_match.empty() && track_to_pf_candidate_map.count(tracking_match.begin()->first.key())) {
                    auto pf_candidate_ref = track_to_pf_candidate_map[tracking_match.begin()->first.key()];
                    if (!pf_candidate_ref->elementsInBlocks().empty()) {
                        block_gen_count_map[pf_candidate_ref->elementsInBlocks()[0].first.key()]++;
                    }
                }
            }
        }
    }

    // Process Gen Particles
    for (size_t i = 0; i < gen_particle_handle->size(); ++i) {
        reco::GenParticleRef gen_ref(gen_particle_handle, i);
        if (gen_ref->status() != 1 || gen_ref->charge() == 0) continue;

        // Reset Tree Variables
        gen_id_ = gen_ref->pdgId(); 
        gen_pt_ = gen_ref->pt(); 
        gen_eta_ = gen_ref->eta(); 
        gen_energy_ = gen_ref->energy();
        
        track_pt_ = -1.0; 
        track_eta_ = -1.0; 
        
        pf_link_data_.reset();
        block_search_data_.reset();

        if (gen_to_tracking_particle_map.count(gen_ref)) {
            auto tracking_particle_ref = gen_to_tracking_particle_map[gen_ref];
            if (sim_to_reco_handle->find(tracking_particle_ref) != sim_to_reco_handle->end()) {
                auto matches = (*sim_to_reco_handle)[tracking_particle_ref];
                if (!matches.empty()) {
                    edm::RefToBase<reco::Track> track_ref = matches.begin()->first;
                    
                    track_pt_ = track_ref->pt();
                    track_eta_ = track_ref->eta();

                    // Case A: match_pf_link (Using direct CMSSW pointers)
                    if (track_to_pf_candidate_map.count(track_ref.key())) {
                        auto pf_candidate_ref = track_to_pf_candidate_map[track_ref.key()];
                        
                        pf_link_data_.id = pf_candidate_ref->particleId();
                        pf_link_data_.pf_ecal_energy = pf_candidate_ref->ecalEnergy();
                        pf_link_data_.pf_hcal_energy = pf_candidate_ref->hcalEnergy();
                        pf_link_data_.pf_raw_ecal_energy = pf_candidate_ref->rawEcalEnergy();
                        pf_link_data_.pf_raw_hcal_energy = pf_candidate_ref->rawHcalEnergy();
                        
                        if (!pf_candidate_ref->elementsInBlocks().empty()) {
                            fill_aggregated_block_info(pf_candidate_ref->elementsInBlocks()[0].first, track_ref, pf_link_data_, block_gen_count_map);
                        }
                    }

                    // Case B: match_block_search (Brute-force searching all blocks for track keys)
                    for (size_t block_index = 0; block_index < pf_block_handle->size(); ++block_index) {
                        reco::PFBlockRef block_ref(pf_block_handle, block_index);
                        bool block_match_found = false;
                        
                        for (const auto& element : block_ref->elements()) {
                            if (element.type() == reco::PFBlockElement::TRACK && element.trackRef().isNonnull() && element.trackRef().key() == track_ref.key()) {
                                fill_aggregated_block_info(block_ref, track_ref, block_search_data_, block_gen_count_map);
                                block_match_found = true;
                                break;
                            }
                        }
                        if (block_match_found) break;
                    }
                }
            }
        }
        
        // Push synchronously to all TTrees
        gen_tree_->Fill(); 
        track_tree_->Fill();
        pf_link_tree_->Fill(); 
        block_search_tree_->Fill();
    }
}

DEFINE_FWK_MODULE(testanalyzerdump_4);