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
  cyclus::toolkit::CommodityProducer::SetCapacity(commod, throughput);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Upon entering 
void Corrm::EnterNotify() {
  cyclus::Facility::EnterNotify();
  // set core size
  core.capacity(core_size);
  fill_tank.capacity(fill_size);
  // set buy_policy to send things to inventory buffer
  buy_policy.Init(this, &core, std::string("core"), core_size, core_size);

  // Flag for first entering (used for receiving fuel or fill)
  fresh = true;

  // dummy comp, use in_recipe if provided
  cyclus::CompMap v;
  cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
  if (in_recipe != "") {
    comp = context()->GetRecipe(in_recipe);
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
  std::stringstream ss;
  std::string ans, out_str;
  if (out_commods.size() == 1) {
    out_str = out_commods.front();
  } else {
    out_str = "";
  }
  if (cyclus::toolkit::CommodityProducer::Produces(
          cyclus::toolkit::Commodity(out_str))) {
    ans = "yes";
  } else {
    ans = "no";
  }
  ss << cyclus::Facility::str();
  ss << " has facility parameters {"
     << "\n"
     << "     Output Commodity = " << out_str << ",\n"
     << "     Residence Time = " << residence_time << ",\n"
     << "     Throughput = " << throughput << ",\n"
     << " commod producer members: "
     << " produces " << out_str << "?:" << ans << "'}";
  return ss.str();
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
  std::cout << "\n TICK END---------------------------------\n";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/// After DRE, process inventory (inventory-> processing, processing->ready, ready->stocks)
void Corrm::Tock() {
  std::cout << "\n TOCK BEGINS-----------------------------\n";
  // if first tock, changes buy_policy so that it now accepts fill not fuel 
  if (fresh && core.quantity() == core_size){
    fresh = false;
    // set preference of fuel to zero
    cyclus::CompMap v;
    cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
    for (int i = 0; i != in_commods.size(); ++i) {
     buy_policy.Set(in_commods[i], comp, 1e-10);
    }
    std::cout << "\n Not fresh no mo and core is full \n";
    buy_policy.Stop();
    buy_policy.Init(this, &fill_tank, std::string("fill_tank"));

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
    buy_policy.Start();
  }
  
  std::cout << "\n core quantity : " << core.quantity();
  BeginProcessing_();  // place core to rep_tank
  std::cout << "\ndone core to rep_tank\n";
  std::cout << "\n core quantity : " << core.quantity();
  std::cout << "\n rep_tank quantity : " << rep_tank.quantity() << "\n";
  ProcessMat_();  // place rep_tank to waste
  std::cout << "\ndone rep_tank to waste \n";
  std::cout << "\n rep_tank quantity : " << rep_tank.quantity() << "\n";
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
void Corrm::BeginProcessing_() {
  while (core.count() > 0) {
    try {
      rep_tank.Push(core.Pop());
      LOG(cyclus::LEV_INFO5, "ComCnv") << prototype() << "is pushing from core to rep_tank at time "
                                       << context()->time(); 
    } catch (cyclus::Error& e) {
    e.msg(Agent::InformErrorMsg(e.msg()));
    throw e;
  }

  }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
/// Moves material from rep_tank to waste buffer.
void Corrm::ProcessMat_() {
  using cyclus::Material;
  using cyclus::ResCast;
  using cyclus::toolkit::ResBuf;
  using cyclus::toolkit::Manifest;

  while(rep_tank.count() > 0){
    try{
        waste.Push(rep_tank.Pop());
        LOG(cyclus::LEV_INFO5, "ComCnv") << prototype() << "is pushing from rep_tank to waste buffer at time "
                                       << context()->time(); 
    } catch (cyclus::Error& e) {
    e.msg(Agent::InformErrorMsg(e.msg()));
    throw e;
    } 
  }
}



// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
extern "C" cyclus::Agent* ConstructCorrm(cyclus::Context* ctx) {
  return new Corrm(ctx);
}

}  // namespace corrm
