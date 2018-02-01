#ifndef CYCLUS_RECYCLE_CORRM_H
#define CYCLUS_RECYCLE_CORRM_H

#include <string>
#include <list>
#include <vector>

#include "cyclus.h"

// forward declaration
namespace corrm {
class Corrm;
} // namespace corrm


namespace corrm {
/// @class Corrm
/// The Continuous Online Reprocessing Reactor Module (CORRM)
/// takes in initial fuel and runs the reactor by breeding fuel
/// from fertile fill commodity. It stops accepting fuel commodity
/// at the first Tock (only one DRE for accepting fuel) after the
/// core is full. Then it accepts fill commodity (Th, U238) into
/// the core. 

/// The core undergoes a depletion calculation every dt, which is
/// usually smaller than the CYCLUS time of 1 month. For every dt,
/// the agent does the following:
/// 1. Deplete the current core for dt
/// 2. Send gases, Pa and etc(??) to rep_tank buffer.
/// 3. From rep_tank buffer, send Xe, Kr and gases to waste buffer.
/// 4. From rep_tank buffer, send Pa into Pa_tank buffer.
/// 5. Fill the mass defect in core with fill_commodity.
/// 6. Check for criticality, and make up criticality by receiving U233 from Pa_tank.



class Corrm 
  : public cyclus::Facility,
    public cyclus::toolkit::CommodityProducer {
 public:  
  /// @param ctx the cyclus context for access to simulation-wide parameters
  Corrm(cyclus::Context* ctx);
  
  #pragma cyclus decl

  #pragma cyclus note {"doc": "Storage is a simple facility which accepts any number of commodities " \
                              "and holds them for a user specified amount of time. The commodities accepted "\
                              "are chosen based on the specified preferences list. Once the desired amount of material "\
                              "has entered the facility it is passed into a 'processing' buffer where it is held until "\
                              "the residence time has passed. The material is then passed into a 'ready' buffer where it is "\
                              "queued for removal. Currently, all input commodities are lumped into a single output commodity. "\
                              "Storage also has the functionality to handle materials in discrete or continuous batches. Discrete "\
                              "mode, which is the default, does not split or combine material batches. Continuous mode, however, "\
                              "divides material batches if necessary in order to push materials through the facility as quickly "\
                              "as possible."}

  /// A verbose printer for the Storage Facility
  virtual std::string str();

  // --- Facility Members ---
  
  // --- Agent Members ---
  /// Sets up the Storage Facility's trade requests
  virtual void EnterNotify();

  /// The handleTick function specific to the Storage.
  virtual void Tick();

  /// The handleTick function specific to the Storage.
  virtual void Tock();


 protected:
  ///   @brief adds a material into the incoming commodity inventory
  ///   @param mat the material to add to the incoming inventory.
  ///   @throws if there is trouble with pushing to the inventory buffer.
  void AddMat_(cyclus::Material::Ptr mat);

  /// @brief Move all unprocessed inventory to processing
  void BeginProcessing_();

  /// @brief Move as many ready resources as allowable into stocks
  /// @param cap current throughput capacity 
  void ProcessMat_();

  /// @brief depletes the core with Reduced-Order-Model of SERPENT
  void Deplete();

  /// @brief Separates elements to different streams
  void Separate();
  /// Record buff transactions
  void Record_buff(std::string sender, std::string receiver, double quantity);


  /* --- Module Members --- */

  #pragma cyclus var {"tooltip":"input commodity",\
                      "doc":"commodities accepted by this facility",\
                      "uilabel":"Input Commodities",\
                      "uitype":["oneormore","incommodity"]}
  std::vector<std::string> in_commods;

  #pragma cyclus var {"tooltip":"fill commodity",\
                      "doc":"fill commodities accepted by this facility",\
                      "uilabel":"Fill Commodities",\
                      "uitype":["oneormore","incommodity"]}
  std::vector<std::string> fill_commods;

  #pragma cyclus var {"default": [],\
                      "doc":"preferences for each of the given commodities, in the same order."\
                      "Defauts to 1 if unspecified",\
                      "uilabel":"In Commody Preferences", \
                      "range": [None, [1e-299, 1e299]], \
                      "uitype":["oneormore", "range"]}
  std::vector<double> in_commod_prefs;

  #pragma cyclus var {"default": [],\
                      "doc":"preferences for each of the given fill commodities, in the same order."\
                      "Defauts to 1 if unspecified",\
                      "uilabel":"Fill Commodity Preferences", \
                      "range": [None, [1e-299, 1e299]], \
                      "uitype":["oneormore", "range"]}
  std::vector<double> fill_commod_prefs;

  #pragma cyclus var {"tooltip":"output commodity",\
                      "doc":"commodity produced by this facility. Multiple commodity tracking is"\
                      " currently not supported, one output commodity catches all input commodities.",\
                      "uilabel":"Output Commodities",\
                      "uitype":["oneormore","outcommodity"]}
  std::vector<std::string> out_commods;

  #pragma cyclus var {"default":"",\
                      "tooltip":"input recipe",\
                      "doc":"recipe accepted by this facility, if unspecified a dummy recipe is used",\
                      "uilabel":"Input Recipe",\
                      "uitype":"inrecipe"}
  std::string in_recipe;

  #pragma cyclus var {"default":"",\
                      "tooltip":"fill recipe",\
                      "doc":"fill recipe accepted by this facility, if unspecified a dummy recipe is used",\
                      "uilabel":"Fill Recipe",\
                      "uitype":"inrecipe"}
  std::string fill_recipe;

  #pragma cyclus var {"default": 0,\
                      "tooltip":"residence time (timesteps)",\
                      "doc":"the minimum holding time for a received commodity (timesteps).",\
                      "units":"time steps",\
                      "uilabel":"Residence Time", \
                      "uitype": "range", \
                      "range": [0, 12000]}
  int residence_time;

  #pragma cyclus var {"default": 3,\
                      "tooltip":"depletion and reprocessing time (days)",\
                      "doc":"time period for reactor depletion and reprocessing.",\
                      "units":"time steps",\
                      "uilabel":"Deplete Time", \
                      "uitype": "range", \
                      "range": [0, 120000]}
  int dt;

  #pragma cyclus var {"default": 1e299,\
                     "tooltip":"throughput per timestep (kg)",\
                     "doc":"the max amount that can be moved through the facility per timestep (kg)",\
                     "uilabel":"Throughput",\
                     "uitype": "range", \
                     "range": [0.0, 1e299], \
                     "units":"kg"}
  double throughput;

  #pragma cyclus var {"default": 1e299,\
                      "tooltip":"maximum inventory size (kg)",\
                      "doc":"the maximum amount of material that can be in all storage buffer stages",\
                      "uilabel":"Maximum Inventory Size",\
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "units":"kg"}
  double max_inv_size; 


  #pragma cyclus var {"default": 1e299,\
                      "tooltip":"Core Size (kg)",\
                      "doc":"the maximum amount of material that can be in core",\
                      "uilabel":"Maximum Core Size",\
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "units":"kg"}
  double core_size;

  #pragma cyclus var {"default": 1e299,\
                      "tooltip":"Fill Buffer Size (kg)",\
                      "doc":"the maximum amount of fill materials that can be stored",\
                      "uilabel":"Maximum Fill Size",\
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "units":"kg"}
  double fill_size;

  #pragma cyclus var {"default": 1, "doc": "Always starts fresh, flag for fuel and fill"}
  bool fresh;

  ///  ResBuf for various stages
  #pragma cyclus var {"tooltip":"Incoming material buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> core;

  #pragma cyclus var {"tooltip":"Output material buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> waste;

  #pragma cyclus var {"tooltip":"Protactinium decay tank"}
  cyclus::toolkit::ResBuf<cyclus::Material> pa_tank;

  #pragma cyclus var {"tooltip":"Fill material stockpile buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> fill_tank;

  #pragma cyclus var {"tooltip":"Reprocessing buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> rep_tank;
  //// list of input times for materials entering the processing buffer
  #pragma cyclus var{"default": [],\
                      "internal": True}
  std::list<int> entry_times;


  //// A policy for requesting material
  cyclus::toolkit::MatlBuyPolicy buy_policy;

  //// A policy for sending material
  cyclus::toolkit::MatlSellPolicy sell_policy;


  friend class CorrmTest;
};

}  // namespace corrm

#endif  // CYCLUS_RECYCLE_CORRM_H
