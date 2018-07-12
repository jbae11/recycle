// corrm.cc
// Implements the Corrm class
#include "corrm.h"

namespace corrm {

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Corrm::Corrm(cyclus::Context* ctx) : cyclus::Facility(ctx) {
  cyclus::Warn<cyclus::EXPERIMENTAL_WARNING>(
      "The Corrm Facility is experimental.");
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// pragmas

#pragma cyclus def schema corrm::Corrm

#pragma cyclus def annotations corrm::Corrm

#pragma cyclus def initinv corrm::Corrm

#pragma cyclus def snapshotinv corrm::Corrm

#pragma cyclus def infiletodb corrm::Corrm

#pragma cyclus def snapshot corrm::Corrm

#pragma cyclus def clone corrm::Corrm

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Corrm::InitFrom(Corrm* m) {
#pragma cyclus impl initfromcopy corrm::Corrm
  cyclus::toolkit::CommodityProducer::Copy(m);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Corrm::InitFrom(cyclus::QueryableBackend* b) {
#pragma cyclus impl initfromdb corrm::Corrm
}



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Upon entering 
void Corrm::EnterNotify() {
  cyclus::Facility::EnterNotify();

  init_fuel_tank.capacity(core_size);
  fertile_tank.capacity(fill_per_timestep);

  // set buy_policy to send things to inventory buffers
  buy_policy.Init(this, &init_fuel_tank, std::string("init_fuel"));
  
  // Flag for first entering (used for receiving fuel or fill)
  fresh = true;

  // dummy comp, use in_recipe if provided
  cyclus::CompMap v;
  cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
  comp = context() -> GetRecipe(init_fuel_recipe);

  // set buy_policy for incommod with preference
  buy_policy.Set(init_fuel_commod, comp);
  buy_policy.Start();


  sell_policy_fissile.Init(this, &fissile_tank, std::string("fissile"));
  sell_policy_fissile.Set(fissile_out_commod);
  sell_policy_fissile.Start();

  sell_policy_waste.Init(this, &waste_tank, std::string("Waste"));
  sell_policy_waste.Set(waste_commod);
  sell_policy_waste.Start();

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string Corrm::str() {

}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/// Before DRE, display current capacity of archetype
void Corrm::Tick() {
  using cyclus::Material;
  // if core is full,
  // transmute, send to rep_tank 

  // whewn it's its exittime,
  if (context()->time() == exit_time()){
  	// add end material to buffer
  	cyclus::CompMap discharge_v;
  	cyclus::Composition::Ptr discharge_comp = cyclus::Composition::CreateFromAtom(discharge_v);
  	discharge_comp = context() -> GetRecipe(discharge_fuel_recipe);
  	Material::Ptr m = Material::Create(this, core_size, discharge_comp);
  	discharge_tank.Push(m);

  	// initiate sell policy to start selling
  	sell_policy_end.Init(this, &discharge_tank, std::string("Discharge"));
  	sell_policy_end.Set(discharge_fuel_commod);
  	sell_policy_end.Start();
  }

  // empty out fill buffer to receive next
  if (!fertile_tank.empty()){
  	fertile_tank.Pop();
  }

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void Corrm::Tock() {
  using cyclus::Material;

  // if the core is full for the first time, changes buy_policy so that it now accepts fill not fuel 
  if (fresh && init_fuel_tank.quantity() == core_size){
    fresh = false;
    // set preference of fuel to negative - meaning we won't take fuel no more.
    cyclus::CompMap v;
    cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
    buy_policy.Set(init_fuel_commod, comp, -1);    
    buy_policy.Stop();
  }

  // generate fissile and waste material
  cyclus::CompMap fissile_v;
  cyclus::Composition::Ptr fissile_comp = cyclus::Composition::CreateFromAtom(fissile_v);
  fissile_comp = context() -> GetRecipe(fissile_out_recipe);
  Material::Ptr m = Material::Create(this, fissile_output_per_timestep, fissile_comp);
  fissile_tank.Push(m);

  cyclus::CompMap waste_v;
  cyclus::Composition::Ptr waste_comp = cyclus::Composition::CreateFromAtom(waste_v);
  waste_comp = context() -> GetRecipe(waste_recipe);
  Material::Ptr k = Material::Create(this, waste_output_per_timestep, waste_comp);
  waste_tank.Push(k);

  bool first;
  first = true;
  // start fill policy only once
  if (first){
	  // dummy comp for fill, use fill_recipe if provided
	  cyclus::CompMap fill_v;
	  cyclus::Composition::Ptr fill_comp = cyclus::Composition::CreateFromAtom(fill_v);
	  fill_comp = context() -> GetRecipe(fill_recipe);


	  buy_policy_fill.Init(this, &fertile_tank, std::string("fill"));

	  // add fill to buy_policy
	  buy_policy_fill.Set(fill_commod, fill_comp);
	  buy_policy_fill.Start();
	  first = false;
  }

  // generate power
  if (init_fuel_tank.quantity() == core_size){
  cyclus::toolkit::RecordTimeSeries<cyclus::toolkit::POWER>(this, power_cap);
  }
  else {
    std::cout << "Core is not full, not producing power: " << context()->time();  
  }

}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructCorrm(cyclus::Context* ctx) {
  return new Corrm(ctx);
}

}  // namespace corrm
