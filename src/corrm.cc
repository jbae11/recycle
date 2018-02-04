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

  using cyclus::toolkit::Commodity;
  Commodity commod = Commodity(out_commods.front());
  cyclus::toolkit::CommodityProducer::Add(commod);
}



typedef std::pair<double, std::map<int, double> > Stream;
typedef std::map<std::string, Stream> StreamSet;


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Upon entering 
void Corrm::EnterNotify() {
  cyclus::Facility::EnterNotify();
  // set core size
  core.capacity(core_size);
  fill_tank.capacity(fill_size);
  fissile_tank.capacity(fissile_size);

  // set depletion and reprocessing frequency per timestep
  dep_freq = floor(context()->dt() / dt);
  std::cout << "\n depletion frequency is " << dep_freq << "\n";


  std::cout << "\nwaste capacity: " << waste.capacity();
  std::cout << "\n pa tank capacity: " << pa_tank.capacity();
  std::cout << "\n fill tank capacity: " << fill_tank.capacity();
  std::cout << "\n core capacity: " << core.capacity();
  std::cout << "\n rep tank capacity: " << rep_tank.capacity(); 

  // set buy_policy to send things to inventory buffer
  buy_policy.Init(this, &core, std::string("core"), core_size, 1e-6);

  // Flag for first entering (used for receiving fuel or fill)
  fresh = true;

  // dummy comp, use in_recipe if provided
  cyclus::CompMap v;
  cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
  if (in_recipe != "") {
    comp = context()->GetRecipe(in_recipe);
  }


  // Check rep_frac validity
  if (rep_frac > 1 || rep_frac < 0){
    std::stringstream ss;
    ss << "rep_frac must be between 0 and 1!";
    throw cyclus::ValueError(ss.str());
  }


  // Check in-commod_prefs size and set default if not specified
  if (in_commod_prefs.size() == 0) {
    for (int i = 0; i < in_commods.size(); ++i) {
      in_commod_prefs.push_back(cyclus::kDefaultPref);
    }
  } else if (in_commod_prefs.size() != in_commods.size()) {
    std::stringstream ss;
    ss << "in_commod_prefs has " << in_commod_prefs.size()
       << " values, expected " << in_commods.size();
    throw cyclus::ValueError(ss.str());
  }

  // Check fill_commod_prefs size and set default if not specified
  if (fill_commod_prefs.size() == 0) {
    for (int i = 0; i < fill_commods.size(); ++i) {
      fill_commod_prefs.push_back(cyclus::kDefaultPref);
    }
  } else if (fill_commod_prefs.size() != fill_commods.size()) {
    std::stringstream ss;
    ss << "fill_commod_prefs has " << fill_commod_prefs.size()
       << " values, expected " << fill_commods.size();
    throw cyclus::ValueError(ss.str());
  }

  // set buy_policy for incommod with preference
  for (int i = 0; i != in_commods.size(); ++i) {
    buy_policy.Set(in_commods[i], comp, in_commod_prefs[i]);
  }
  buy_policy.Start();

  buy_policy.Init(this, &fill_tank, std::string("fill_tank"), fill_size, fill_size);

  // dummy comp for fill, use fill_recipe if provided
  cyclus::CompMap fill_v;
  cyclus::Composition::Ptr fill_comp = cyclus::Composition::CreateFromAtom(fill_v);
  if (fill_recipe != "") {
    fill_comp = context()->GetRecipe(fill_recipe);
  }  

  // add all to buy_policy
  for (int i = 0; i != fill_commods.size(); ++i){
      buy_policy.Set(fill_commods[i], fill_comp, fill_commod_prefs[i]);
  }
  std::cout << "\n Now accepting fill";
  buy_policy.Start();

  // set sell_policy for out_commod (from waste buff)
  if (out_commods.size() == 1) {
    sell_policy.Init(this, &waste, std::string("waste"))
        .Set(out_commods.front())
        .Start();
  } else {
    std::stringstream ss;
    ss << "out_commods has " << out_commods.size() << " values, expected 1.";
    throw cyclus::ValueError(ss.str());
  }

  std::cout << prototype() << " has entered!";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string Corrm::str() {

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/// Before DRE, display current capacity of archetype
void Corrm::Tick() {
  std::cout << "\n TICK BEGINS------------------------------- \n";
  // Set available capacity for fill_tank
  std::cout << "\n fill_tank size" << fill_size;
  std::cout << "\n fill_tank quantity" << fill_tank.quantity();
  std::cout << "\n fill_tank capacity" << fill_tank.capacity();
  std::cout << "\n fill_tank space" << fill_tank.space();
  std::cout << "\n core space" << core.space();


  // if core is full,
  // transmute, send to rep_tank 
  if (core.quantity() == core_size){
  Deplete();
  std::cout << "\n Deplete!! \n";

  //Separate();
  std::cout << "\n core quantity : " << core.quantity();
  core_to_rep();  // place core to rep_tank
  std::cout << "\ndone core to rep_tank\n";
  std::cout << "\n core quantity : " << core.quantity();
  std::cout << "\n rep_tank quantity : " << rep_tank.quantity() << "\n";
  
  std::cout << "\n fill_tank quantity : " << fill_tank.quantity() << "\n";
  refill_core();



  rep_to_waste();  // place rep_tank to waste
  std::cout << "\ndone rep_tank to waste \n";
  std::cout << "\n rep_tank quantity : " << rep_tank.quantity() << "\n";

  std::cout << "\n TICK END---------------------------------\n";
  }

  else {
    std::cout << "Core is not full, not producing power";
  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void Corrm::Tock() {
  std::cout << "\n TOCK BEGINS-----------------------------\n";

  // if the core is full for the first time, changes buy_policy so that it now accepts fill not fuel 
  if (fresh && core.quantity() == core_size){
    fresh = false;
    std::cout << "\n Not fresh no mo and core is full \n";
    // set preference of fuel to negative - meaning we won't take fuel no more.
    cyclus::CompMap v;
    cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
    for (int i = 0; i != in_commods.size(); ++i) {
     buy_policy.Set(in_commods[i], comp, -1);
    }
    std::cout << "\n Not accepting fuel anymore \n";
    
    buy_policy.Start();
  }
  
  std::cout << "\n TOCK END-----------------------------\n";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/// Adds material to inventory buffer
void Corrm::AddMat_(cyclus::Material::Ptr mat) {
  LOG(cyclus::LEV_INFO5, "ComCnv") << prototype() << " is initially holding "
                                   << core.quantity() << " total.";
  try {
    core.Push(mat);
  } catch (cyclus::Error& e) {
    e.msg(Agent::InformErrorMsg(e.msg()));
    throw e;
  }

}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/// Moves all material from core to rep_tank.
void Corrm::core_to_rep() {
  double qty = core.quantity() * rep_frac;
  Record_buff("core", "rep_tank", qty);
  rep_tank.Push(core.Pop(qty));
  LOG(cyclus::LEV_INFO5, "ComCnv") << prototype() << "is pushing from core to rep_tank at time "
                                   << context()->time(); 
}

void Corrm::refill_core() {
  // check criticality given all from fill_tank
  // not critical add U233 from Pa_tank
  // still not critical get from fissile_tank
  // cannot make it critical, throw error
  double qty = core.quantity() * rep_frac;
  if (qty > fill_tank.quantity()){
    std::stringstream ss;
    ss << "Not enough material in fill tank :(";
    throw cyclus::ValueError(ss.str());
  }
  Record_buff("fill_tank", "core", qty);
  core.Push(fill_tank.Pop(qty));
  LOG(cyclus::LEV_INFO5, "ComCnv") << prototype() << "is pushing from fill_tank to core at time "
                                   << context()->time();

}


void Corrm::Deplete() {
  cyclus::Material::Ptr dep_core = core.Pop(core.quantity());
  core.Push(dep_core);

  // test composition. This should be done in the ROM
  // U238 (90%), Pa233(5%), Xe135(5%) to test reprocessing
  cyclus::CompMap m;
  m[922380000] = 90;
  m[912330000] = 5;
  m[541350000] = 5;

  cyclus::Composition::Ptr c1 = cyclus::Composition::CreateFromMass(m);
  dep_core->Transmute(c1);
}

// Stream and StreamSet type definition for ease
typedef std::pair<double, std::map<int, double> > Stream;
typedef std::map<std::string, Stream> StreamSet;

void Corrm::Separate(){
  cyclus::Material::Ptr rep_stream = core.Pop(rep_tank.quantity());
  rep_tank.Push(rep_stream);

  cyclus::CompMap cm = rep_stream->comp()->mass();
  cyclus::compmath::Normalize(&cm, rep_stream->quantity());


}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/// Moves material from rep_tank to waste buffer.
void Corrm::rep_to_waste() {
  using cyclus::Material;
  using cyclus::ResCast;
  using cyclus::toolkit::ResBuf;
  using cyclus::toolkit::Manifest;

  while(rep_tank.count() > 0){
    try{
        double qty = rep_tank.quantity();
        waste.Push(rep_tank.Pop());
        Record_buff("rep_tank", "waste_buffer", qty);
        LOG(cyclus::LEV_INFO5, "ComCnv") << prototype() << "is pushing from rep_tank to waste buffer at time "
                                       << context()->time(); 
    } catch (cyclus::Error& e) {
    e.msg(Agent::InformErrorMsg(e.msg()));
    throw e;
    } 
  }
}


void Corrm::Record_buff(std::string sender, std::string receiver, double quantity){
  context()
      ->NewDatum("CorrmBuffRecord")
      ->AddVal("AgentId", id())
      ->AddVal("Time", context()->time())
      ->AddVal("Sender", sender)
      ->AddVal("Receiver", receiver)
      ->AddVal("Quantity", quantity)
      ->Record();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructCorrm(cyclus::Context* ctx) {
  return new Corrm(ctx);
}

}  // namespace corrm
