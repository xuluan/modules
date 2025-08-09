
#pragma once


// INLINE+CROSSLINE*2.7
bool check_data_plus_mul(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);

//  SIN((INLINE + CROSSLINE) * 0.1) + COS(INLINE * 0.2) * sin(tan(CROSSLINE))
bool check_data_complex_1(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables);