#ifndef CYCAMORE_SRC_SEPARATIONS_H_
#define CYCAMORE_SRC_SEPARATIONS_H_

#include "cyclus.h"

namespace cycamore {

cyclus::Material::Ptr SepMaterial(std::map<int, double> effs, cyclus::Material::Ptr mat);

class Separations : public cyclus::Facility {
 public:
  Separations(cyclus::Context* ctx);
  virtual ~Separations(){};

  virtual void Tick();
  virtual void Tock();
  virtual void EnterNotify();

  virtual void AcceptMatlTrades(const std::vector<std::pair<
      cyclus::Trade<cyclus::Material>, cyclus::Material::Ptr> >& responses);

  virtual std::set<cyclus::RequestPortfolio<cyclus::Material>::Ptr>
  GetMatlRequests();

  virtual std::set<cyclus::BidPortfolio<cyclus::Material>::Ptr> GetMatlBids(
      cyclus::CommodMap<cyclus::Material>::type& commod_requests);

  virtual void GetMatlTrades(
      const std::vector<cyclus::Trade<cyclus::Material> >& trades,
      std::vector<std::pair<cyclus::Trade<cyclus::Material>,
                            cyclus::Material::Ptr> >& responses);

  #pragma cyclus clone
  #pragma cyclus initfromcopy
  #pragma cyclus infiletodb
  #pragma cyclus initfromdb
  #pragma cyclus schema
  #pragma cyclus annotations
  #pragma cyclus snapshot
  // the following pragmas are ommitted and the functions are generated
  // manually in order to handle the vector of resource buffers:
  //
  //     #pragma cyclus snapshotinv
  //     #pragma cyclus initinv

  virtual cyclus::Inventories SnapshotInv();
  virtual void InitInv(cyclus::Inventories& inv);

 private:
  #pragma cyclus var { \
    "doc" : "Maximum quantity of feed material that can be processed per time step.", \
    "units": "kg", \
  }
  double throughput;

  #pragma cyclus var { \
    "doc": "Ordered list of commodities on which to request feed material to separate.", \
    "uitype": ["oneormore", "incommodity"], \
  }
  std::vector<std::string> feed_commods;

  #pragma cyclus var { \
    "default": [], \
    "doc": "Feed commodity request preferences for each of the given feed commodities (same order)." \
           " If unspecified, default is to use zero for all preferences.", \
  }
  std::vector<double> feed_commod_prefs;

  #pragma cyclus var { \
    "doc": "Name for recipe to be used in feed requests." \
           " Empty string results in use of an empty dummy recipe.", \
    "uitype": "recipe", \
    "default": "", \
  }
  std::string feed_recipe;

  #pragma cyclus var { \
    "doc" : "Amount of feed material to keep on hand.", \
    "units" : "kg", \
  }
  double feedbuf_size;

  #pragma cyclus var { \
    "capacity" : "feedbuf_size", \
  }
  cyclus::toolkit::ResBuf<cyclus::Material> feed;

  #pragma cyclus var { \
    "doc" : "Maximum amount of leftover separated material (not included in" \
            " any other stream) that can be stored." \
            " If full, the facility halts operation until space becomes available.", \
    "default": 1e299, \
  }
  double leftoverbuf_size;

  #pragma cyclus var { \
    "doc": "Commodity on which to trade the leftover separated material stream." \
           " This MUST NOT be the same as any commodity used to define the other separations streams.", \
    "uitype": "outcommodity", \
    "default": "default-waste-stream", \
  }
  std::string leftover_commod;

  #pragma cyclus var { \
    "capacity" : "leftoverbuf_size", \
  }
  cyclus::toolkit::ResBuf<cyclus::Material> leftover;

  #pragma cyclus var { \
    "alias": ["streams", "commod", ["info", "buf_size", ["efficiencies", "comp", "eff"]]], \
    "uitype": ["oneormore", "outcommodity", ["pair", "double", ["oneormore", "nuclide", "double"]]], \
    "doc": "Output streams for separations." \
           " Each stream must have a unique name identifying the commodity on which its material is traded," \
           " a max buffer capacity in kg (neg values indicate infinite size)," \
           " and a set of component efficiencies." \
           " 'comp' is a component to be separated into the stream" \
           " (e.g. U, Pu, etc.) and 'eff' is the mass fraction of the component" \
           " that is separated from the feed into this output stream." \
           " If any stream buffer is full, the facility halts operation until space becomes available.", \
  }
  std::map<std::string,std::pair<double,std::map<int,double> > > streams_;

  // custom SnapshotInv and InitInv and EnterNotify are used to persist this
  // state var.
  std::map<std::string, cyclus::toolkit::ResBuf<cyclus::Material> > streambufs;
};

}  // namespace cycamore

#endif  // CYCAMORE_SRC_SEPARATIONS_H_
