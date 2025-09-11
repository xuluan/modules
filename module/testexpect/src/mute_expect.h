
#pragma once


// <3000, >9000, window_size=0
bool check_data_mute_3000_9000_0(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

// <3000, >9000, window_size=2000
bool check_data_mute_3000_9000_plus_2000(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

// <3000, >9000, window_size=-2000
bool check_data_mute_3000_9000_sub_2000(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

// > expr, expr=500*crossline
bool check_data_mute_gt_expr_500_mul_crossline(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

