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



//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Upon entering 
void Corrm::EnterNotify() {
  cyclus::Facility::EnterNotify();
  // set core size
  core.capacity(core_size);
  fill_tank.capacity(fill_size);
  
  // set depletion and reprocessing frequency per timestep
  dep_freq = floor(context()->dt() / dt);
  std::cout << "\n depletion frequency is " << dep_freq << "\n";


  std::cout << "\nwaste buffer capacity: " << waste.capacity();
  std::cout << "\n pa tank capacity: " << pa_tank.capacity();
  std::cout << "\n rep tank capacity: " << rep_tank.capacity(); 
  std::cout << "\n fill tank capacity: " << fill_tank.capacity();
  std::cout << "\n core capacity: " << core.capacity();

  // set buy_policy to send things to inventory buffers
  buy_policy.Init(this, &core, std::string("core"));

  // Flag for first entering (used for receiving fuel or fill)
  fresh = true;

  // dummy comp, use in_recipe if provided
  cyclus::CompMap v;
  cyclus::Composition::Ptr comp = cyclus::Composition::CreateFromAtom(v);
  if (in_recipe != "") {
    comp = context()->GetRecipe(in_recipe);
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


  std::cout << prototype() << " has entered!";
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
std::string Corrm::str() {

}

// only requires GetMatlRequest and AcceptMatlTrades because sell_policy handles outgoing waste
std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr> Corrm::GetMatlRequests(){
  using cyclus::RequestPortfolio;
  using cyclus::Material;
  using cyclus::CapacityConstraint;


  std::set<RequestPortfolio<Material>::Ptr> ports;
  Material::Ptr m;

  // dummy comp for fill, use fill_recipe if provided
  cyclus::CompMap fill_v;
  cyclus::Composition::Ptr fill_comp = cyclus::Composition::CreateFromAtom(fill_v);
  if (fill_recipe != "") {
    fill_comp = context()->GetRecipe(fill_recipe);
  }  


  // add fill_commod_pref to add request.
  RequestPortfolio<Material>::Ptr port(new RequestPortfolio<Material>());
  std::vector<cyclus::Request<Material>*> mreqs;
  for (int i = 0; i != fill_commods.size(); ++i){
    std::string commod = fill_commods[i];
    double pref = fill_commod_prefs[i];
    m = Material::CreateUntracked(fill_tank.space(), fill_comp);
    cyclus::Request<Material>* r = port->AddRequest(m, this, commod, pref, true);
    mreqs.push_back(r);
  }


  // not sure what this will do
  cyclus::CapacityConstraint<Material> cc(fill_tank.space());
  port->AddConstraint(cc);

  return ports;
}



// Accept Materials and push to the fill_tank
void Corrm::AcceptMatlTrades(
    const std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                                 cyclus::Material::Ptr> >& responses) {
  std::vector< std::pair<cyclus::Trade<cyclus::Material>,
                         cyclus::Material::Ptr> >::const_iterator it;
  for (it = responses.begin(); it != responses.end(); ++it) {
    fill_tank.Push(it->second);
  }
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

  std::cout << "\n core quantity : " << core.quantity();
  core_to_rep();  // place core to rep_tank
  std::cout << "\ndone core to rep_tank\n";
  std::cout << "\n core quantity : " << core.quantity();
  std::cout << "\n rep_tank quantity : " << rep_tank.quantity() << "\n";
  std::cout << "\n fill_tank quantity : " << fill_tank.quantity() << "\n";
  refill_core();
  Separate();
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
  // Get material that is in rep tank
  cyclus::Material::Ptr rep_stream = rep_tank.Pop(rep_tank.quantity());
  double orig_qty = rep_stream->quantity();

  std::map<std::string, cyclus::toolkit::ResBuf<cyclus::Material> > streambufs;
  std::string str_pa = "pa";
  std::string str_waste = "waste";
  streambufs[str_pa] = pa_tank;
  streambufs[str_waste] = waste;

  // Call SepMaterial to separate materials and set in map
  std::map<std::string, cyclus::Material::Ptr> stagedsep;
  stagedsep[str_pa] = SepMaterial(pa_tank_stream, rep_stream);
  stagedsep[str_waste] = SepMaterial(waste_stream, rep_stream);

  // Send the respective streams to their buffers
  std::map<std::string, cyclus::Material::Ptr>::iterator it;
  for (it = stagedsep.begin(); it != stagedsep.end(); ++it){
    std::string name = it->first;
    cyclus::Material::Ptr m = it->second;
    if (m->quantity() > 0){
        streambufs[name].Push(rep_stream->ExtractComp(m->quantity(), m->comp()));
    }
  }

  // If there is anything left after the reprocessing
  // push the remaining material back into core.
  if (rep_stream->quantity() > 0){
    core.Push(rep_stream);
  }

}

cyclus::Material::Ptr SepMaterial(std::map<int, double> effs,
                                  cyclus::Material::Ptr mat) {
  cyclus::CompMap cm = mat->comp()->mass();
  cyclus::compmath::Normalize(&cm, mat->quantity());
  double tot_qty = 0;
  cyclus::CompMap sepcomp;

  cyclus::CompMap::iterator it;
  for (it = cm.begin(); it != cm.end(); ++it) {
    int nuc = it->first;
    int elem = (nuc / 10000000) * 10000000;
    double eff = 0;
    if (effs.count(nuc) > 0) {
      eff = effs[nuc];
    } else if (effs.count(elem) > 0) {
      eff = effs[elem];
    } else {
      continue;
    }

    double qty = it->second;
    double sepqty = qty * eff;
    sepcomp[nuc] = sepqty;
    tot_qty += sepqty;
  }

  cyclus::Composition::Ptr c = cyclus::Composition::CreateFromMass(sepcomp);
  return cyclus::Material::CreateUntracked(tot_qty, c);
};

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -



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
