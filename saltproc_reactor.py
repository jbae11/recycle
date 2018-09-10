import random
import copy
import math
from collections import defaultdict
import numpy as np
import scipy as sp
import h5py

from cyclus.agents import Institution, Agent, Facility
from cyclus import lib
import cyclus.typesystem as ts


class saltproc_reactor(Facility):
    """
    This reactor imports an HDF5 file from a saltproc run (or a converted SCALE output).

    It imports the following data:
        surplus fissile output composition and isotopics in time
        waste output isotopics in time
        fertile input isotopics in time
        core isotopics in time
        blanket isotopics(if applicable)
    """
    init_fuel_commod = ts.String(
        doc="The commodity name for initial loading fuel",
        tooltip="Init Fuel",
        uilabel="Init Fuel"
    )

    final_fuel_commod = ts.String(
        doc="The commodity name for final discharge fuel",
        tooltip="Final Fuel",
        uilabel="Final Fuel"
    )

    fill_commod = ts.String(
        doc="The commodity this facility will take in for fertile materials ",
        tooltip="Fill Commodity",
        uilabel="Fill Commodity"
    )

    fissile_out_commod = ts.String(
        doc="The fissile commodity this facility will output",
        tooltip="Fissile commodity",
        uilabel="Fissile Commodity"
    )

    waste_commod = ts.String(
        doc="The waste commodity this facility will output",
        tooltip="Waste commodity",
        uilabel="Waste Commodity"
    )

    db_path = ts.String(
        doc="Path to the hdf5 file",
        tooltip="Absolute path to the hdf5 file"
    )

    waste_tank = ts.ResBufMaterialInv()
    fissile_tank = ts.ResBufMaterialInv()

    driver_buf = ts.ResBufMaterialInv()
    blanket_buf = ts.ResBufMaterialInv()

    fill_tank = ts.ResBufMaterialInv()

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def enter_notify(self):
        super().enter_notify()
        self.f = h5py.File(self.db_path, 'r')
        self.max_nonzero_indx = self.get_max_nonzero_indx()
        # subject to change
        self.isos = self.f['iso names']
        self.num_isotopes = len(self.isos)
        self.waste_db = self.cum_to_nocum(self.f['waste tank composition'])
        self.fissile_db = self.cum_to_nocum(self.f['fissile tank composition'])
        self.driver_refill = self.cum_to_nocum(self.f['driver refill tank composition'])
        self.blanket_refill = self.cum_to_nocum(self.f['blanket refill tank composition'])
        self.driver_db = self.cum_to_nocum(self.f['driver composition after reproc'])    
        self.blanket_db = self.cum_to_nocum(self.f['blanket composition after reproc'])

        self.siminfo_timestep = int(self.f['siminfo_timestep'][()].decode('utf-8'))
        self.saltproc_timestep = self.siminfo_timestep * 24 * 3600
        self.buf_dict = {'driver': self.driver_buf, 'blanket': self.blanket_buf}
        self.prev_indx = 0
        self.driver_mass = sum(self.driver_db[0])
        self.blanket_mass = sum(self.blanket_db[0])
        self.driver_buf.capacity = self.driver_mass
        if self.blanket_mass == 0:
            self.blanket_buf.capacity = 0
            del self.buf_dict['blanket']
        else:
            self.blanket_buf.capacity = self.blanket_mass
        self.fresh = True
        self.loaded = False
        self.shutdown = False

    def cum_to_nocum(self, dataset):
        """ Convert cumulative array to non-cumulative array

        Parameters:
        -----------
        dataset: hdf5 dataset
        """
        new_array = np.zeros((self.max_nonzero_indx, self.num_isotopes))
        new_array[0] = dataset[0]
        for i in range(1, self.max_nonzero_indx):
            new_array[i] = dataset[i] - dataset[i-1]
        return new_array

    def get_max_nonzero_indx(self):
        self.tot_timesteps = len(self.f['driver composition after reproc'])
        for i in range(self.tot_timesteps):
            if sum(self.f['driver composition after reproc'][i]) == 0:
                return i - 1

    def tick(self):
        print('tick')
        print('=============')
        print('TIME IS %i' %self.context.time)
        if not self.fresh:
            reactor_age_sec = self.context.dt * (self.context.time - self.start_time)
            timestep_at_hdf5 = int(reactor_age_sec / self.saltproc_timestep)
            self.indx = min(timestep_at_hdf5, len(self.waste_db[:, 0])) + 1
        else:
            self.indx = -1
        # if reactor is `on'
        if self.indx > 0:
            # push waste from db to waste_tank
            self.get_waste()
            # push fissile from db to fissile_tank
            self.get_fissile()
            self.get_fill_demand()
            self.prev_indx = self.indx
            
        for key, val in self.buf_dict.items():
            print('%s qty: %f' %(key, val.quantity))
        print('tickend')
        print('=============')


    def check_core_full(self):
        for key, val in self.buf_dict.items():
            if val.quantity != val.capacity:
                return False
        return True

    def tock(self):
        # check if core is full;
        print('tock')
        if self.check_core_full():
            self.loaded = True
            print('REACTOR IS LOADED')
            # Produce power.. how?
            if self.fresh:
                self.start_time = self.context.time
                self.fresh = False
        else:
            print('DRIVER OR BLANKET IS NOT FULL IN TIME %s \n\n' %self.context.time)
            self.loaded = False

        print('tockend')
        print('=============')
        print('\n\n')

    def get_waste(self):
        waste_dump = np.zeros(self.num_isotopes)
        waste_comp = {}
        # lump all the waste generated in this timestep
        # into one waste dump
        for t in np.arange(self.prev_indx, self.indx):
            waste_dump += self.waste_db[t, :]
        # convert this into a dictionary

        waste_mass = sum(waste_dump)
        waste_comp = self.array_to_comp_dict(waste_dump)
        if waste_mass != 0:
            material = ts.Material.create(self, waste_mass, waste_comp)
            self.waste_tank.push(material)
            print('PUSHED %f kg of WASTE INTO WASTE TANK' %waste_mass)

    def get_fissile(self):
        fissile_dump = np.zeros(self.num_isotopes)
        fissile_comp = {}

        for t in np.arange(self.prev_indx, self.indx):
            fissile_dump += self.fissile_db[t, :]

        fissile_mass = sum(fissile_dump)
        fissile_comp = self.array_to_comp_dict(fissile_dump)

        if fissile_mass != 0:
            material = ts.Material.create(self, fissile_mass, fissile_comp)
            self.fissile_tank.push(material)
            print('PUSHED %f kg of FISSILE MATERIAL INTO FISSILE TANK' %fissile_mass)
        else:
            print('There is no fissile material to push')


    def get_fill_demand(self):
        self.get_fill = False

        demands = {}
        masses = {}
        for bufs in self.buf_dict.keys():
            demands[bufs] = np.zeros(self.num_isotopes)
        fill_comp_dict = {}
        # since the data is in negative:
        for t in np.arange(self.prev_indx, self.indx):
            for key, val in demands.items():
                if 'driver' in key:
                    demands[key] += -1.0 * self.driver_refill[t, :]
                if 'blanket' in key:
                    demands[key] += -1.0 * self.blanket_refill[t, :]

        for key, val in demands.items():
            masses[key] = sum(val)

        for key, val in self.buf_dict.items():
            val.pop(masses[key])
        self.qty = sum(masses.values())
        if (self.qty) == 0:
            return 0
        self.fill_comp = sum(demands.values())

        fill_comp_dict = self.array_to_comp_dict(self.fill_comp)
        if bool(fill_comp_dict):
            print('I WANT %f kg Fill material' %(self.qty))
            self.demand_mat = ts.Material.create_untracked(self.qty, fill_comp_dict)
        if self.qty != 0.0:
            self.get_fill = True


    def get_material_bids(self, requests):
        """ Gets material bids that want its `outcommod' and
            returns bid portfolio
        """
        self.write('get material bids ' + str(self.context.time) + '\n')
        bids = []
        # waste commods
        if self.waste_commod not in requests:
            return
        reqs = requests[self.waste_commod]
        for req in reqs:
            qty = min(req.target.quantity, self.waste_tank.quantity)
            # returns if the inventory is empty
            if self.waste_tank.empty():
                return
            # get the composition of the material next in line
            next_in_line = self.waste_tank.peek()
            mat = ts.Material.create_untracked(qty, next_in_line.comp())
            bids.append({'request': req, 'offer': mat})
        
        # fissile commods
        if self.fissile_out_commod not in requests:
            return
        reqs = requests[self.fissile_out_commod]
        for req in reqs:
            qty = min(req.target.quantity, self.fissile_tank.quantity)
            if self.fissile_tank.empty():
                return
            next_in_line =self.fissile_tank.peek()
            mat = ts.Material.create_untracked(qty, next_in_line.comp())
            bids.append({'request': req, 'offer':mat})

        if self.context.time == self.exit_time:
            self.shutdown = True
            reqs = requests[self.final_fuel_commod]
            for req in reqs:
                total_qty = self.driver_buf.quantity + self.blanket_buf.quantity
                qty = min(req.target.quantity, total_qty)
                if self.driver_buf.empty():
                    return
                driver = self.driver_buf.peek()
                blanket = self.blanket_buf.peek()
                driver.absorb(blanket)
                bids.append({'request': req, 'offer': driver})                                
        port = {"bids": bids}
        return port
    
    def write(self, string):
        with open('debug.txt', 'a+') as f:
            f.write(string)


    def get_material_trades(self, trades):
        """ Give out waste_commod and fissile_out_commod from
            waste_tank and fissile_tank, respectively.
        """
        self.write('get material trades ' + str(self.context.time) + '\n')
        responses = {}
        print(trades)
        for trade in trades:
            print(trade.request.commodity)
            print('\n')
        for trade in trades:
            commodity = trade.request.commodity
            if commodity == self.waste_commod:
                self.write('waste commod')
                mat_list = self.waste_tank.pop_n(self.waste_tank.count)
            if commodity == self.fissile_out_commod:
                self.write('fissile out commod')
                mat_list = self.fissile_tank.pop_n(self.fissile_tank.count)
            if commodity == self.final_fuel_commod:
                self.write('final fuel commod')
                blank_comp = self.array_to_comp_dict(self.blanket_db[self.prev_indx, :])
                driver_comp = self.array_to_comp_dict(self.driver_db[self.prev_indx, :])
                blank = ts.Material.create(self, self.blanket_mass, blank_comp)
                driv = ts.Material.create(self, self.driver_mass, driver_comp)
                driv.absorb(blank)
                mat_list = [driv]
            # absorb all materials
            # best way is to do it separately, but idk how to do it :(
            for mat in mat_list[1:]:
                mat_list[0].absorb(mat)
            responses[trade] = mat_list[0]
        return responses


    def get_material_requests(self):
        """ Ask for material fill_commod """
        self.write('get material requests ' +str(self.context.time) + '\n')
        ports = []
        if self.shutdown:
            return {}
        if not self.loaded:
            recipes = {}
            qty = {}
            mat = {}
            for key, val in self.buf_dict.items():
                if 'driver' in key:
                    recipes[key] = self.array_to_comp_dict(self.f['siminfo_driver_init_comp'])
                if 'blanket' in key:
                    recipes[key] = self.array_to_comp_dict(self.f['siminfo_blanket_init_comp'])
                qty[key] = val.capacity - val.quantity
                mat[key] = ts.Material.create_untracked(qty[key], recipes[key])

            for key, val in self.buf_dict.items():
                if 'driver' in key:
                    ports.append({'commodities': {self.init_fuel_commod: mat[key]},
                                  'constraints': qty[key]})
                if 'blanket' in key:
                    prots.append({'commodities': {self.fill_commod: mat[key]},
                                  'constraints': qty[key]})
            return ports
        elif self.get_fill:
            commods = {self.fill_commod: self.demand_mat}
            port = {'commodities': commods, 'constraints': self.qty}
            return port
        else:
            return {}


    def accept_material_trades(self, responses):
        """ Get fill_commod and store it into fill_tank """
        self.write('accept material trades ' +str(self.context.time) + '\n')
        for key, mat in responses.items():
            if key.request.commodity == self.init_fuel_commod and not self.loaded:
                self.driver_buf.push(mat)
            elif key.request.commodity == self.fill_commod:
                to_blanket = mat.extract_qty(self.blanket_buf.space)
                self.blanket_buf.push(to_blanket)
                self.driver_buf.push(mat)

    def array_to_comp_dict(self, array):
        dictionary = {}
        for i, val in enumerate(array):
            if val != 0:
                iso = self.isos[i].decode('utf8')
                dictionary[iso] = val
        return dictionary