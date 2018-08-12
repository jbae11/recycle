#ifndef CYCLUS_RECYCLE_AMSRA_H
#define CYCLUS_RECYCLE_AMSRA_H

#include <string>
#include <list>
#include <vector>

#include "cyclus.h"
#include "recycle_version.h"

namespace amsra {
/// @class amsra
/// The Average Molten Salt Reactor Archetype (AMSRA)
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



cyclus::Material::Ptr SepMaterial(std::map<int, double> effs,
                                  cyclus::Material::Ptr mat);


class Amsra 
  : public cyclus::Facility,
    public cyclus::toolkit::CommodityProducer {
 public:  
  /// @param ctx the cyclus context for access to simulation-wide parameters
  Amsra(cyclus::Context* ctx);
  
  #pragma cyclus decl

  #pragma cyclus note {"doc": "Amsra models MSR behavior using average rates of "\
                              "material flow"}

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


  /* --- Module Members --- */
  #pragma cyclus var {"uitype": ["oneormore", "incommodity"], \
                      "tooltip": "infuel commodity",\
                      "doc":"commodities accepted by this facility",\
                      "uilabel":"Infuel Commodity"}
  std::vector<std::string> init_fuel_commod;

  #pragma cyclus var { \
    "default": [], \
    "uilabel": "Fresh Fuel Preference List", \
    "doc": "The preference for each type of fresh fuel requested corresponding"\
           " to each input commodity (same order).  If no preferences are " \
           "specified, 1.0 is used for all fuel " \
           "requests (default).", \
  }
  std::vector<double> fuel_prefs;

  #pragma cyclus var {"default": 1e299,\
                      "tooltip":"Core Size (kg)",\
                      "doc":"the maximum amount of material that can be in core",\
                      "uilabel":"Maximum Core Size",\
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "units":"kg"}
  double core_size;

  #pragma cyclus var {"uitype": "inrecipe", \
                      "uilabel": "Init fuel recipe name", \
                      "doc": "Init fuel commodity recipe name"}
  std::string init_fuel_recipe;

  #pragma cyclus var {"tooltip": "fill commodity",\
                      "doc":"fill commodities accepted by this facility",\
                      "uilabel":"Fill Commodity"}
  std::string fill_commod;


  #pragma cyclus var {"uitype": "inrecipe", \
                      "uilabel": "Fill recipe name", \
                      "doc": "Fill commodity recipe name"}
  std::string fill_recipe;
  
  #pragma cyclus var {"tooltip":"Fill per timestep (kg)",\
                      "doc":"the amount of material that the reactor needs per timestep",\
                      "uilabel":"Fill in per timestep",\
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "units":"kg"}
  double fill_per_timestep;


  #pragma cyclus var {"tooltip": "discharge fuel commodity",\
                      "doc":"discharge fuel commodity accepted by this facility",\
                      "uilabel":"Discharge Fuel Commodity"}
  std::string discharge_fuel_commod;

  #pragma cyclus var {"tooltip": "discharge fuel recipe",\
                      "doc":"discharge fuel recipe offered by this facility",\
                      "uilabel":"Discharge Fuel Recipe"}
  std::string discharge_fuel_recipe;






  #pragma cyclus var { \
    "default": 0, \
    "doc": "Amount of electrical power the facility produces when operating " \
           "normally.", \
    "uilabel": "Nominal Reactor Power", \
    "uitype": "range", \
    "range": [0.0, 5000.00],  \
    "units": "MWe", \
  }
  double power_cap;


  #pragma cyclus var {"tooltip": "fissile output commodity",\
                      "doc":"fissile commodities this facility outputs",\
                      "uilabel":"fissile output"}
  std::string fissile_out_commod;

  #pragma cyclus var {"tooltip": "fissile output recipe",\
                      "doc":"fissile recipe this facility outputs",\
                      "uilabel":"fissile recipe"}
  std::string fissile_out_recipe;
  #pragma cyclus var {"tooltip":"Fissile out per timestep (kg)",\
                      "doc":"the amount of fissile material reactor outputs per timestep",\
                      "uilabel":"Fissile out per timestep",\
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "units":"kg"}
  double fissile_output_per_timestep;

  #pragma cyclus var {"tooltip": "waste output commodity",\
                      "doc":"waste commodities this facility outputs",\
                      "uilabel":"waste output"}
  std::string waste_commod;

  #pragma cyclus var {"tooltip": "waste output recipe",\
                      "doc":"waste recipe this facility outputs",\
                      "uilabel":"waste recipe"}
  std::string waste_recipe;
  #pragma cyclus var {"tooltip":"Waste out per timestep (kg)",\
                      "doc":"the amount of waste material reactor outputs per timestep",\
                      "uilabel":"Waste out per timestep",\
                      "uitype": "range", \
                      "range": [0.0, 1e299], \
                      "units":"kg"}
  double waste_output_per_timestep;

  bool fresh;

  ///  ResBuf for various stages
  #pragma cyclus var {"tooltip":"Incoming material buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> fertile_tank;

  #pragma cyclus var {"tooltip":"Initial fuel buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> init_fuel_tank;


  #pragma cyclus var {"tooltip":"Waste buffer"}
  cyclus::toolkit::ResBuf<cyclus::Material> waste_tank;

  #pragma cyclus var {"tooltip":"Fissile tank"}
  cyclus::toolkit::ResBuf<cyclus::Material> fissile_tank;

  #pragma cyclus var {"tooltip":"Discharge tank"}
  cyclus::toolkit::ResBuf<cyclus::Material> discharge_tank;


  //// A policy for requesting material
  cyclus::toolkit::MatlBuyPolicy buy_policy;
  cyclus::toolkit::MatlBuyPolicy buy_policy_fill;
  //// A policy for sending material
  cyclus::toolkit::MatlSellPolicy sell_policy_waste;
  cyclus::toolkit::MatlSellPolicy sell_policy_fissile;
  cyclus::toolkit::MatlSellPolicy sell_policy_end;


  friend class AmsraTest;
};

}  // namespace amsra

#endif  // CYCLUS_RECYCLE_AMSRA_H
