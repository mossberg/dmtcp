/****************************************************************************
 *  Copyright (C) 2012-2013 by Artem Y. Polyakov <artpol84@gmail.com>       *
 *                                                                          *
 *  This file is part of the RM plugin for DMTCP                        *
 *                                                                          *
 *  RM plugin is free software: you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public License as   *
 *  published by the Free Software Foundation, either version 3 of the      *
 *  License, or (at your option) any later version.                         *
 *                                                                          *
 *  RM plugin is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU Lesser General Public License for more details.                     *
 *                                                                          *
 *  You should have received a copy of the GNU Lesser General Public        *
 *  License along with DMTCP:dmtcp/src.  If not, see                        *
 *  <http://www.gnu.org/licenses/>.                                         *
 ****************************************************************************/

#include "discover_dmtcpinput.h"

using namespace std;

void resources_input::trim(string &str, string delim)
{
  size_t first = 0;

  first = str.find_first_of(delim, first);
  while (first != string::npos) {
    size_t last = str.find_first_not_of(delim, first);
    if (last != string::npos) {
      str.erase(first, last - first);
      first = str.find_first_of(delim, first);
    } else {
      str.erase(first, str.length() - first);
      first = string::npos;
    }
  }
}

 bool resources_input::get_checkpoint_filename(string &str, string &ckptname)
  {
    size_t pos = str.find_last_of("/");
    if (pos != string::npos) {
      ckptname.clear();
      ckptname.insert(0, str, pos + 1, str.length() - pos + 1);
      return true;
    }
    return false;
  }

  bool resources_input::is_serv_slot(string &str)
  {
    string serv_names[] = {"orted", "orterun", "mpiexec", "mpirun"};
    uint size = sizeof (serv_names) / sizeof (serv_names[0]);
    uint i;
    for (i = 0; i < size; i++) {
      if (str.find("ckpt_" + serv_names[i]) != string::npos)
        return true;
    }
    return false;
  }

  bool resources_input::is_launch_process(string &str)
  {
    string serv_names[] = {"orterun", "mpiexec", "mpirun"};
    uint size = sizeof (serv_names) / sizeof (serv_names[0]);
    uint i;
    for (i = 0; i < size; i++) {
      if (str.find("ckpt_" + serv_names[i]) != string::npos)
        return true;
    }
    return false;
  }

  void resources_input::count_slots(string &str, uint &slots, uint &srv_slots, bool &is_launch)
  {
    string delim = " ";
    slots = srv_slots = 0;
    size_t start_pos = 0, match_pos;
    str += ' ';
    if ((start_pos = str.find_first_not_of(delim, start_pos)) == string::npos)
      return;
    while (start_pos != string::npos &&
           (match_pos = str.find_first_of(delim, start_pos)) != string::npos) {
      size_t sublen = match_pos - start_pos;
      if (sublen > 0) {
        string sub(str.substr(start_pos, sublen));
        string ckptname;
        if (get_checkpoint_filename(sub, ckptname)) {
          if (is_serv_slot(ckptname)) {
            is_launch = is_launch_process(ckptname);
            srv_slots++;
          } else
            slots++;
        }
      }
      start_pos = match_pos;
      start_pos = str.find_first_not_of(delim, start_pos);
    }
  }

  bool resources_input::add_host(string &str, uint &node_id)
  {
    string delim = ":";
    size_t start_pos = 0;
    size_t match_pos;
    string hostname = "";
    string mode = "";
    bool is_launch;

    // get host name
    if ((match_pos = str.find(delim)) == string::npos)
      return false;
    if (match_pos - start_pos > 0) {
      size_t sublen = match_pos - start_pos;
      hostname = str.substr(start_pos, sublen);
      trim(hostname, " \n\t"); // delete spaces, newlines and tabs
    } else {
      return false;
    }
    start_pos = match_pos + delim.length();

    // skip mode
    if ((match_pos = str.find(delim, start_pos)) == string::npos)
      return false;
    if (!(match_pos - start_pos > 0))
      return false;
    else {
      size_t sublen = match_pos - start_pos;
      mode = str.substr(start_pos, sublen);
    }
    start_pos = match_pos + delim.length();

    // process checkpoints
    size_t sublen = str.length() - start_pos;
    string ckpts(str.substr(start_pos, sublen));
    trim(ckpts, "\n");
    uint slots, srv_slots;
    count_slots(ckpts, slots, srv_slots, is_launch);

    if (node_map.find(hostname) != node_map.end()) {
      node_map[hostname].slots += slots;
      node_map[hostname].srv_slots += srv_slots;
      node_map[hostname].is_launch = node_map[hostname].is_launch || is_launch;
      node_ckpt_map[hostname] += ' ' + ckpts;
    } else {
      node_map[hostname].id = node_id;
      node_id++;
      node_map[hostname].slots = slots;
      node_map[hostname].srv_slots = srv_slots;
      node_map[hostname].name = hostname;
      node_map[hostname].mode = mode;
      node_map[hostname].is_launch = is_launch;
      node_ckpt_map[hostname] = ckpts;
    }

    return true;
  }

resources_input::resources_input(string str) : resources(input)
{
  string delim = "::";
  uint hostid = 0;

  _valid = false;
  size_t start_pos = 0, match_pos;

  if ((match_pos = str.find(delim)) == string::npos)
    return;
  start_pos = match_pos + delim.length();
  while ((match_pos = str.find(delim, start_pos)) != string::npos) {
    size_t sublen = match_pos - start_pos;
    if (sublen > 0) {
      string sub(str.substr(start_pos, sublen));
      if (add_host(sub, hostid))
        _valid = true;
    }
    start_pos = match_pos + delim.length();
  }

  if (start_pos != str.length()) {
    size_t sublen = str.length() - start_pos;
    if (sublen > 0) {
      string sub(str.substr(start_pos, sublen));
      if (add_host(sub, hostid))
        _valid = true;
    }
    start_pos = match_pos + delim.length();
  }
}

void resources_input::writeout_old(string env_var, resources &r)
{
  mapping_t map;
  string warning = "";
  if (!map_to(r, map, warning))
    return;
  if( warning != "" ){
    cout << "DMTCP_DISCOVER_RM_WARNING=\'" << warning << "\'" << endl;
  }
  
  cout << env_var + "=\'" << endl;
  for (size_t i = 0; i < r.ssize(); i++) {
    if (map[i].size()) {
      cout << ":: " + r[i].name + " :" + sorted_v[0]->mode + ": ";
      for (size_t j = 0; j < map[i].size(); j++) {
        int k = map[i][j];
        string name = sorted_v[k]->name;
        cout << node_ckpt_map[name];
      }
      cout << endl;
    }
  }
  cout << "\'" << endl;
}

void resources_input::writeout_new(string env_var, resources &r)
{
  mapping_t map;
  string warning = "";
  if (!map_to(r, map, warning))
    return;
  if( warning != "" ){
    cout << "DMTCP_DISCOVER_RM_WARNING=\'" << warning << "\'" << endl;
  }
  
  cout << env_var + "_IDS=\'" << r.ssize() << "\'" << endl;

  for (size_t i = 0; i < r.ssize(); i++) {
    cout << env_var + "_" << r[i].id << "=\'";
    if (map[i].size()) {
      for (size_t j = 0; j < map[i].size(); j++) {
        int k = map[i][j];
        string name = sorted_v[k]->name;
        cout << node_ckpt_map[name] << " ";
      }
      cout << "\'" << endl;
    }
  }
}