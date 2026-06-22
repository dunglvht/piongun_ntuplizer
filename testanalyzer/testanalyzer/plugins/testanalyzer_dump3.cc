// === STEP 1: Includes and Framework Setup ===
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

// === STEP 2: Class Definition and Data Structures ===
class testanalyzer_dump3 : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
    explicit testanalyzer_dump3(const edm::ParameterSet&);
    ~testanalyzer_dump3() override = default;

private:
    void beginJob() override;
    void analyze(const edm::Event&, const edm::EventSetup&) override;

    struct match_data_struct { 
        int id; 
        float pf_ecal_energy, pf_hcal_energy;
        float pf_raw_ecal_energy, pf_raw_hcal_energy;
        float ecal_sum_energy, ecal_max_energy, ecal_closest_energy, ecal_closest_dr;
        float hcal_sum_energy, hcal_max_energy, hcal_closest_energy, hcal_closest_dr;
        int block_id, tracks_in_block, gen_in_block;
    };

    void fill_aggregated_block_info(reco::PFBlockRef block_ref, edm::RefToBase<reco::Track> track_ref, match_data_struct& d, const std::map<int, int>& gen_map);

    edm::EDGetTokenT<std::vector<reco::GenParticle>> gen_token_;
    edm::EDGetTokenT<std::vector<TrackingParticle>> tp_token_;
    edm::EDGetTokenT<reco::SimToRecoCollection> sim_to_reco_token_;
    edm::EDGetTokenT<reco::PFCandidateCollection> pf_candidate_token_;
    edm::EDGetTokenT<reco::PFBlockCollection> pf_block_token_;

    TTree *gen_tree_, *track_tree_, *pf_link_tree_, *block_search_tree_;

    int   gen_id_;
    float gen_pt_, gen_eta_, gen_energy_;
    float track_pt_, track_eta_; // <--- TRACK INFO
    match_data_struct pf_link_ref_data_, block_search_ref_data_;
};

// === STEP 3: Constructor and TTree Setup ===
testanalyzer_dump3::testanalyzer_dump3(const edm::ParameterSet& iConfig)
    : gen_token_(consumes<std::vector<reco::GenParticle>>(edm::InputTag("genParticles"))),
      tp_token_(consumes<std::vector<TrackingParticle>>(edm::InputTag("mix", "MergedTrackTruth"))),
      sim_to_reco_token_(consumes<reco::SimToRecoCollection>(edm::InputTag("trackingParticleRecoTrackAsssociation"))),
      pf_candidate_token_(consumes<reco::PFCandidateCollection>(edm::InputTag("particleFlow"))),
      pf_block_token_(consumes<reco::PFBlockCollection>(edm::InputTag("particleFlowBlock"))) {
    usesResource("TFileService");
}

void testanalyzer_dump3::beginJob() {
    edm::Service<TFileService> fs;
    TFileDirectory tree_dir = fs->mkdir("tree");

    gen_tree_          = tree_dir.make<TTree>("gen", "gen");
    track_tree_        = tree_dir.make<TTree>("track", "track"); // <--- INITIALIZE
    pf_link_tree_      = tree_dir.make<TTree>("match_pf_link", "match_pf_link");
    block_search_tree_ = tree_dir.make<TTree>("match_block_search", "match_block_search");

    auto setup_match_branches = [](TTree* t, match_data_struct& d) {
        t->Branch("id", &d.id, "id/I");
        t->Branch("pf_ecal_energy", &d.pf_ecal_energy, "pf_ecal_energy/F");
        t->Branch("pf_hcal_energy", &d.pf_hcal_energy, "pf_hcal_energy/F");
        t->Branch("pf_raw_ecal_energy", &d.pf_raw_ecal_energy, "pf_raw_ecal_energy/F");
        t->Branch("pf_raw_hcal_energy", &d.pf_raw_hcal_energy, "pf_raw_hcal_energy/F");
        t->Branch("ecal_sum_energy", &d.ecal_sum_energy, "ecal_sum_energy/F");
        t->Branch("ecal_max_energy", &d.ecal_max_energy, "ecal_max_energy/F");
        t->Branch("ecal_closest_energy", &d.ecal_closest_energy, "ecal_closest_energy/F");
        t->Branch("ecal_closest_dr", &d.ecal_closest_dr, "ecal_closest_dr/F");
        t->Branch("hcal_sum_energy", &d.hcal_sum_energy, "hcal_sum_energy/F");
        t->Branch("hcal_max_energy", &d.hcal_max_energy, "hcal_max_energy/F");
        t->Branch("hcal_closest_energy", &d.hcal_closest_energy, "hcal_closest_energy/F");
        t->Branch("hcal_closest_dr", &d.hcal_closest_dr, "hcal_closest_dr/F");
        t->Branch("block_id", &d.block_id, "block_id/I");
        t->Branch("tracks_in_block", &d.tracks_in_block, "tracks_in_block/I");
        t->Branch("gen_in_block", &d.gen_in_block, "gen_in_block/I");
    };

    gen_tree_->Branch("id", &gen_id_, "id/I");
    gen_tree_->Branch("pt", &gen_pt_, "pt/F");
    gen_tree_->Branch("eta", &gen_eta_, "eta/F");
    gen_tree_->Branch("energy", &gen_energy_, "energy/F");

    track_tree_->Branch("pt", &track_pt_, "pt/F");   // <--- BRANCH
    track_tree_->Branch("eta", &track_eta_, "eta/F");

    setup_match_branches(pf_link_tree_, pf_link_ref_data_);
    setup_match_branches(block_search_tree_, block_search_ref_data_);
}

// === STEP 4: Helper Function Implementation ===
void testanalyzer_dump3::fill_aggregated_block_info(reco::PFBlockRef block_ref, edm::RefToBase<reco::Track> track_ref, match_data_struct& d, const std::map<int, int>& gen_map) {
    d.block_id = block_ref.key();
    d.gen_in_block = gen_map.count(d.block_id) ? gen_map.at(d.block_id) : 0;
    
    float min_ecal_dr = 999.0;
    float min_hcal_dr = 999.0;

    for (const auto& element : block_ref->elements()) {
        if (element.type() == reco::PFBlockElement::TRACK) d.tracks_in_block++;

        if (element.clusterRef().isNonnull()) {
            float energy = element.clusterRef()->energy();
            float dr = reco::deltaR(*track_ref, *(element.clusterRef()));

            if (element.type() == reco::PFBlockElement::ECAL) {
                if (d.ecal_sum_energy == -1.0) d.ecal_sum_energy = 0; 
                d.ecal_sum_energy += energy;
                if (energy > d.ecal_max_energy) d.ecal_max_energy = energy;
                if (dr < min_ecal_dr) { 
                    min_ecal_dr = dr; d.ecal_closest_dr = dr; d.ecal_closest_energy = energy; 
                }
            }
            if (element.type() == reco::PFBlockElement::HCAL) {
                if (d.hcal_sum_energy == -1.0) d.hcal_sum_energy = 0; 
                d.hcal_sum_energy += energy;
                if (energy > d.hcal_max_energy) d.hcal_max_energy = energy;
                if (dr < min_hcal_dr) { 
                    min_hcal_dr = dr; d.hcal_closest_dr = dr; d.hcal_closest_energy = energy; 
                }
            }
        }
    }
}

// === STEP 5: Main Analysis Logic ===
void testanalyzer_dump3::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
    auto gen_h = iEvent.getHandle(gen_token_);
    auto tp_h = iEvent.getHandle(tp_token_);
    auto sim_to_reco_h = iEvent.getHandle(sim_to_reco_token_);
    auto pf_h = iEvent.getHandle(pf_candidate_token_);
    auto block_h = iEvent.getHandle(pf_block_token_);

    std::map<reco::GenParticleRef, TrackingParticleRef> gen_to_tp_map;
    std::map<unsigned int, reco::PFCandidateRef> track_to_pf_map;
    for (size_t i = 0; i < pf_h->size(); ++i) {
        reco::PFCandidateRef r(pf_h, i);
        if (r->trackRef().isNonnull()) track_to_pf_map[r->trackRef().key()] = r;
    }

    std::map<int, int> block_gen_count;
    for (size_t i = 0; i < tp_h->size(); ++i) {
        TrackingParticleRef tp_ref(tp_h, i);
        for (const auto& gr : tp_ref->genParticles()) {
            gen_to_tp_map[gr] = tp_ref;
            if (sim_to_reco_h->find(tp_ref) != sim_to_reco_h->end()) {
                auto m = (*sim_to_reco_h)[tp_ref];
                if (!m.empty() && track_to_pf_map.count(m.begin()->first.key())) {
                    auto pf = track_to_pf_map[m.begin()->first.key()];
                    if (!pf->elementsInBlocks().empty()) block_gen_count[pf->elementsInBlocks()[0].first.key()]++;
                }
            }
        }
    }

    for (size_t i = 0; i < gen_h->size(); ++i) {
        reco::GenParticleRef gen_ref(gen_h, i);
        if (gen_ref->status() != 1 || gen_ref->charge() == 0) continue;

        // Reset
        gen_id_ = gen_ref->pdgId(); gen_pt_ = gen_ref->pt(); gen_eta_ = gen_ref->eta(); gen_energy_ = gen_ref->energy();
        track_pt_ = -999.0; track_eta_ = -999.0; // <--- RESET
        match_data_struct d_reset = {-1, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, 999.0, -1.0, -1.0, -1.0, 999.0, -1, 0, 0};
        pf_link_ref_data_ = block_search_ref_data_ = d_reset;

        if (gen_to_tp_map.count(gen_ref)) {
            auto tp = gen_to_tp_map[gen_ref];
            if (sim_to_reco_h->find(tp) != sim_to_reco_h->end()) {
                auto matches = (*sim_to_reco_h)[tp];
                if (!matches.empty()) {
                    edm::RefToBase<reco::Track> trk_ref = matches.begin()->first;
                    
                    track_pt_ = trk_ref->pt(); // <--- ASSIGN
                    track_eta_ = trk_ref->eta();

                    // A: match_pf_link
                    if (track_to_pf_map.count(trk_ref.key())) {
                        auto pf = track_to_pf_map[trk_ref.key()];
                        pf_link_ref_data_.id = pf->particleId();
                        pf_link_ref_data_.pf_ecal_energy = pf->ecalEnergy();
                        pf_link_ref_data_.pf_hcal_energy = pf->hcalEnergy();
                        pf_link_ref_data_.pf_raw_ecal_energy = pf->rawEcalEnergy();
                        pf_link_ref_data_.pf_raw_hcal_energy = pf->rawHcalEnergy();
                        if (!pf->elementsInBlocks().empty()) fill_aggregated_block_info(pf->elementsInBlocks()[0].first, trk_ref, pf_link_ref_data_, block_gen_count);
                    }

                    // B: match_block_search
                    for (size_t b_idx = 0; b_idx < block_h->size(); ++b_idx) {
                        reco::PFBlockRef b_ref(block_h, b_idx);
                        bool block_match_found = false;
                        for (const auto& el : b_ref->elements()) {
                            if (el.type() == reco::PFBlockElement::TRACK && el.trackRef().isNonnull() && el.trackRef().key() == trk_ref.key()) {
                                fill_aggregated_block_info(b_ref, trk_ref, block_search_ref_data_, block_gen_count);
                                block_match_found = true;
                                break;
                            }
                        }
                        if (block_match_found) break;
                    }
                }
            }
        }
        gen_tree_->Fill(); 
        track_tree_->Fill(); // <--- FILL
        pf_link_tree_->Fill(); 
        block_search_tree_->Fill();
    }
}

DEFINE_FWK_MODULE(testanalyzer_dump3);