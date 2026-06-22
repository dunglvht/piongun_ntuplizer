// -*- C++ -*-
//
// Package:    testanalyzer/testanalyzer
// Class:      testanalyzer
//
/**\class testanalyzer testanalyzer.cc testanalyzer/testanalyzer/plugins/testanalyzer.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  dung
//         Created:  Tue, 19 May 2026 12:36:28 GMT
//
//

// system include files
#include <memory>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
//
// class declaration
//
#include "CommonTools/UtilAlgos/interface/TFileService.h"
// TrackingParticle (Sim Truth) headers
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticle.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticleFwd.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "SimDataFormats/Associations/interface/TrackAssociation.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"
// Tfile
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "TTree.h"
#include "TFile.h"
// PFblocks
#include "DataFormats/ParticleFlowReco/interface/PFBlock.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElement.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElementTrack.h"
#include "DataFormats/ParticleFlowReco/interface/PFCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElementGsfTrack.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElementCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElementBrem.h"
#include "DataFormats/ParticleFlowReco/interface/PFBlockElementSuperCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFLayer.h"

using reco::TrackCollection;

class testanalyzer : public edm::one::EDAnalyzer<edm::one::SharedResources> {
public:
  explicit testanalyzer(const edm::ParameterSet&);
  ~testanalyzer() override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void beginJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void endJob() override;

  // Helper method to resolve element shorthand names exactly matching PFBlock.cc
  std::string get_element_short_name(const reco::PFBlockElement& element, 
                                     std::map<std::string, int>& type_counters,
                                     const reco::PFBlock& block,
                                     size_t element_index,
                                     std::vector<bool>& to_print) const;

  // ----------member data ---------------------------
  edm::EDGetTokenT<std::vector<reco::GenParticle>> gen_particle_token_;
  edm::EDGetTokenT<std::vector<TrackingParticle>> tracking_particle_token_;
  edm::EDGetTokenT<edm::View<reco::Track>> track_token_;
  edm::EDGetTokenT<reco::RecoToSimCollection> reco_to_sim_token_;
  edm::EDGetTokenT<reco::SimToRecoCollection> sim_to_reco_token_;
  edm::EDGetTokenT<reco::PFCandidateCollection> pf_candidate_token_;
  edm::EDGetTokenT<reco::PFBlockCollection> pf_block_token_;
  // TTree and branch variables (using snake_case)
  TTree* tree_;
  edm::Service<TFileService> fs;

  unsigned int event_id_;
  unsigned int run_id_;

  // GenParticle Info
  float gen_pt_;
  float gen_eta_;
  float gen_phi_;
  int gen_pdg_id_;

  // TrackingParticle Info (Sim)
  bool has_tp_;
  float tp_pt_;
  float tp_eta_;
  float tp_phi_;

  // Reco Track Info
  bool has_track_;
  float track_pt_;
  float track_eta_;
  float track_phi_;
  float track_chi2_match_;
  int track_hits_;
  float ecal_energy_;
  float hcal_energy_;
  int pf_id_;  // 1=charged hadron, 2=electron, 3=muon, 4=gamma, etc.
};

testanalyzer::testanalyzer(const edm::ParameterSet& iConfig)
    : gen_particle_token_(consumes<std::vector<reco::GenParticle>>(edm::InputTag("genParticles"))),
      tracking_particle_token_(consumes<std::vector<TrackingParticle>>(edm::InputTag("mix", "MergedTrackTruth"))),
      track_token_(consumes<edm::View<reco::Track>>(edm::InputTag("generalTracks"))),
      reco_to_sim_token_(consumes<reco::RecoToSimCollection>(edm::InputTag("trackingParticleRecoTrackAsssociation"))),
      sim_to_reco_token_(consumes<reco::SimToRecoCollection>(edm::InputTag("trackingParticleRecoTrackAsssociation"))),
      pf_candidate_token_(consumes<reco::PFCandidateCollection>(edm::InputTag("particleFlow"))),
      pf_block_token_(consumes<reco::PFBlockCollection>(edm::InputTag("particleFlowBlock"))) {
  // Setup the TTree
  tree_ = fs->make<TTree>("track_truth_tree", "Gen to Reco Tracking Truth");

  // Event Info
  tree_->Branch("event_id", &event_id_, "event_id/i");
  tree_->Branch("run_id", &run_id_, "run_id/i");

  // Gen Branches
  tree_->Branch("gen_pt", &gen_pt_, "gen_pt/F");
  tree_->Branch("gen_eta", &gen_eta_, "gen_eta/F");
  tree_->Branch("gen_phi", &gen_phi_, "gen_phi/F");
  tree_->Branch("gen_pdg_id", &gen_pdg_id_, "gen_pdg_id/I");

  // Sim (TrackingParticle) Branches
  tree_->Branch("has_tp", &has_tp_, "has_tp/O");  // 'O' is for boolean
  tree_->Branch("tp_pt", &tp_pt_, "tp_pt/F");
  tree_->Branch("tp_eta", &tp_eta_, "tp_eta/F");
  tree_->Branch("tp_phi", &tp_phi_, "tp_phi/F");

  // Reco Track Branches
  tree_->Branch("has_track", &has_track_, "has_track/O");
  tree_->Branch("track_pt", &track_pt_, "track_pt/F");
  tree_->Branch("track_eta", &track_eta_, "track_eta/F");
  tree_->Branch("track_phi", &track_phi_, "track_phi/F");
  tree_->Branch("track_chi2_match", &track_chi2_match_, "track_chi2_match/F");
  tree_->Branch("track_hits", &track_hits_, "track_hits/I");
  // Ecal + Hcal branches
  tree_->Branch("ecal_energy", &ecal_energy_, "ecal_energy/F");
  tree_->Branch("hcal_energy", &hcal_energy_, "hcal_energy/F");
  tree_->Branch("pf_id", &pf_id_, "pf_id/I");
}

// Helper method implementing exact labels and BREM filtering from CMSSW src
std::string testanalyzer::get_element_short_name(const reco::PFBlockElement& element, 
                                                 std::map<std::string, int>& type_counters,
                                                 const reco::PFBlock& block,
                                                 size_t element_index,
                                                 std::vector<bool>& to_print) const {
  std::string prefix = "??";
  bool print_this = true;

  switch (element.type()) {
    case reco::PFBlockElement::TRACK:
      type_counters["TK"]++;
      prefix = "TK" + std::to_string(type_counters["TK"]);
      break;
    case reco::PFBlockElement::GSF:
      type_counters["GSF"]++;
      prefix = "GSF" + std::to_string(type_counters["GSF"]);
      break;
    case reco::PFBlockElement::BREM: {
      std::multimap<double, unsigned> ecal_elems;
      block.associatedElements(element_index, block.linkData(), ecal_elems, reco::PFBlockElement::ECAL, reco::PFBlock::LINKTEST_ALL);
      type_counters["BREM"]++;
      if (!ecal_elems.empty()) {
        prefix = "BR" + std::to_string(type_counters["BREM"]);
      } else {
        print_this = false;
      }
      break;
    }
    case reco::PFBlockElement::SC:
      type_counters["SC"]++;
      prefix = "SC" + std::to_string(type_counters["SC"]);
      break;
    default: {
      if (element.clusterRef().isNonnull() && element.clusterRef().isAvailable()) {
        auto layer = element.clusterRef()->layer();
        switch (layer) {
          case PFLayer::PS1:
            type_counters["PV"]++;
            prefix = "PV" + std::to_string(type_counters["PV"]);
            break;
          case PFLayer::PS2:
            type_counters["PH"]++;
            prefix = "PH" + std::to_string(type_counters["PH"]);
            break;
          case PFLayer::ECAL_ENDCAP:
            type_counters["EE"]++;
            prefix = "EE" + std::to_string(type_counters["EE"]);
            break;
          case PFLayer::ECAL_BARREL:
            type_counters["EB"]++;
            prefix = "EB" + std::to_string(type_counters["EB"]);
            break;
          case PFLayer::HCAL_ENDCAP:
            type_counters["HE"]++;
            prefix = "HE" + std::to_string(type_counters["HE"]);
            break;
          case PFLayer::HCAL_BARREL1:
            type_counters["HB"]++;
            prefix = "HB" + std::to_string(type_counters["HB"]);
            break;
          case PFLayer::HCAL_BARREL2:
            type_counters["HO"]++;
            prefix = "HO" + std::to_string(type_counters["HO"]);
            break;
          case PFLayer::HF_EM:
            type_counters["FE"]++;
            prefix = "FE" + std::to_string(type_counters["FE"]);
            break;
          case PFLayer::HF_HAD:
            type_counters["FH"]++;
            prefix = "FH" + std::to_string(type_counters["FH"]);
            break;
          default:
            type_counters["??"]++;
            prefix = "??" + std::to_string(type_counters["??"]);
            break;
        }
      } else {
        type_counters["??"]++;
        prefix = "??" + std::to_string(type_counters["??"]);
      }
      break;
    }
  }
  to_print.push_back(print_this);
  return prefix;
}

//
// member functions
//

// ------------ method called for each event  ------------
void testanalyzer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  // Populate the branch metadata variables
  run_id_ = iEvent.id().run();
  event_id_ = iEvent.id().event();

  // Print a prominent event-by-event separation block
  std::cout << "\n=========================================================================\n";
  std::cout << "=== RUN: " << run_id_ << " | EVENT: " << event_id_ << " =========================================\n";
  std::cout << "=========================================================================\n\n";

  edm::Handle<std::vector<reco::GenParticle>> gen_particle_handle;
  iEvent.getByToken(gen_particle_token_, gen_particle_handle);
  edm::Handle<std::vector<TrackingParticle>> tracking_particle_handle;
  iEvent.getByToken(tracking_particle_token_, tracking_particle_handle);
  edm::Handle<reco::SimToRecoCollection> sim_to_reco_handle;
  iEvent.getByToken(sim_to_reco_token_, sim_to_reco_handle);
  edm::Handle<reco::PFCandidateCollection> pf_candidate_handle;
  iEvent.getByToken(pf_candidate_token_, pf_candidate_handle);

  // [Build maps]
  std::map<reco::GenParticleRef, std::vector<TrackingParticleRef>> gen_to_tp_map;
  for (size_t i = 0; i < tracking_particle_handle->size(); ++i) {
    TrackingParticleRef tp_ref(tracking_particle_handle, i);
    for (const auto& gen_ref : tp_ref->genParticles()) {
      gen_to_tp_map[gen_ref].push_back(tp_ref);
    }
  }

  std::map<unsigned int, reco::PFCandidateRef> track_to_pf_map;
  for (size_t i = 0; i < pf_candidate_handle->size(); ++i) {
    reco::PFCandidateRef pf_ref(pf_candidate_handle, i);
    if (pf_ref->trackRef().isNonnull()) {
      track_to_pf_map[pf_ref->trackRef().key()] = pf_ref;
    }
  }

  // --- MAIN GEN LOOP ---
  for (size_t i = 0; i < gen_particle_handle->size(); ++i) {
    reco::GenParticleRef gen_ref(gen_particle_handle, i);
    if (gen_ref->status() != 1 || gen_ref->charge() == 0 || gen_ref->pt() < 0.5)
      continue;

    std::cout << "[GEN] ID: " << gen_ref->pdgId() << " | pT: " << gen_ref->pt() << " | eta: " << gen_ref->eta() << "\n";

    if (gen_to_tp_map.find(gen_ref) != gen_to_tp_map.end()) {
      TrackingParticleRef tp_ref = gen_to_tp_map[gen_ref].front();

      if (sim_to_reco_handle->find(tp_ref) != sim_to_reco_handle->end()) {
        auto matched_tracks = (*sim_to_reco_handle)[tp_ref];
        if (!matched_tracks.empty()) {
          edm::RefToBase<reco::Track> track_ref = matched_tracks.begin()->first;

          std::cout << "  └─> [TRACK] pT: " << track_ref->pt() << " | eta: " << track_ref->eta() << "\n";

          // --- PARTICLE FLOW IDENTIFICATION ---
          if (track_to_pf_map.find(track_ref.key()) != track_to_pf_map.end()) {
            reco::PFCandidateRef pf_candidate = track_to_pf_map[track_ref.key()];

            std::cout << "        └─> [PF CANDIDATE] ID: " << pf_candidate->particleId()
                      << " | ECAL: " << pf_candidate->ecalEnergy() << " | HCAL: " << pf_candidate->hcalEnergy() << "\n"
                      << "         | raw ECAL: " << pf_candidate->rawEcalEnergy() << " | raw HCAL: " << pf_candidate->rawHcalEnergy() << "\n";

            // --- RAW BLOCK DUMP ---
            const auto& elements_in_block = pf_candidate->elementsInBlocks();

            if (!elements_in_block.empty()) {
              reco::PFBlockRef block_ref = elements_in_block[0].first;
              const auto& block_elements = block_ref->elements();
              const auto& link_data = block_ref->linkData(); 

              std::cout << "----------------------------------\n";
              std::cout << "--- PFBlock ---\n";
              std::cout << "number of elements: " << block_elements.size() << "\n";

              std::vector<std::string> element_names;
              std::vector<bool> to_print;
              std::map<std::string, int> type_counters;

              // First pass: Resolve shortnames and filter unprinted BREMs
              for (size_t element_index = 0; element_index < block_elements.size(); ++element_index) {
                const auto& element = block_elements[element_index];
                std::string short_name = get_element_short_name(element, type_counters, *block_ref, element_index, to_print);
                element_names.push_back(short_name);

                if (to_print[element_index]) {
                  try {
                    std::cout << short_name << " " << element << "\n";
                  } catch (const cms::Exception& ex) {
                    std::cout << short_name << " element " << element_index << "- type " << element.type() 
                              << " [Ref Product unavailable in input file]\n";
                  }
                }
              }

              // Second pass: Format the link matrix header
              std::cout << "\nlink data:\n";
              std::cout << "         ";
              for (size_t i = 0; i < block_elements.size(); ++i) {
                if (to_print[i]) {
                  std::cout << std::setw(6) << std::left << element_names[i];
                }
              }
              std::cout << "\n";

              // Third pass: Populate the link data matrix rows using correct CMSSW distance scaling
              for (size_t i = 0; i < block_elements.size(); ++i) {
                if (!to_print[i]) continue;
                std::cout << "   " << std::setw(5) << std::left << element_names[i];
                for (size_t j = 0; j < block_elements.size(); ++j) {
                  if (!to_print[j]) continue;
                  if (i == j) {
                    std::cout << std::setw(6) << " ";
                  } else {
                    double dist = block_ref->dist(i, j, link_data);
                    if (dist > -0.5) {
                      // CMSSW scales distances by 1000 for representation
                      std::cout << std::setw(6) << std::right << std::fixed << std::setprecision(1) << dist * 1000.0;
                    } else {
                      std::cout << std::setw(6) << " ";
                    }
                  }
                }
                std::cout << "\n";
              }
              std::cout << "-----------------------------------------\n";
            }
          }
        }
        std::cout << "-------------------------------------------------------------------------\n";
      }
    }
  }
}

testanalyzer::~testanalyzer() {}

// ------------ method called once each job just before starting event loop  ------------
void testanalyzer::beginJob() {
  // please remove this method if not needed
}

// ------------ method called once each job just after ending the event loop  ------------
void testanalyzer::endJob() {
  // please remove this method if not needed
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void testanalyzer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.setUnknown();
  descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(testanalyzer);