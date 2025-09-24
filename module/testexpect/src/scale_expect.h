
#pragma once


// 
bool check_data_scale_factor(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

bool check_data_scale_agc(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

bool check_data_scale_diverge(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

