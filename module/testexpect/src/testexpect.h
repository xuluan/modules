#ifndef TESTGENEXPECT_H
#define TESTGENEXPECT_H

#include <vector>
#include <string>
#include "ArrowStore.h"

#define DEBUG_DUMP 0

#if DEBUG_DUMP
void pdump(char* p, size_t len);
#endif

enum class CheckPattern {
    SKIP,  //dont check, default: skip
    SAME,  //no change : same
    ATTRCALC_PLUS_MUL, //  INLINE+CROSSLINE*2.7
    ATTRCALC_COMPLEX_1, //  SIN((INLINE + CROSSLINE) * 0.1) + COS(INLINE * 0.2) * sin(tan(CROSSLINE))
    MUTE_3000_9000_0,   // >9000, <3000, window_size=0
    MUTE_3000_9000_PLUS_2000, // >9000, <3000, window_size=2000
    MUTE_3000_9000_SUB_2000,  // >9000, <3000, window_size=-2000
    MUTE_GT_EXPR_500_MUL_CROSSLINE, // >expr, expr=500*crossline
    SCALE_FACTOR,
    SCALE_EXPR,
    SCALE_AGC,
    SCALE_DIVERGE,
    CHECK_PATTERN_NUM
};

struct AttrConfig {
    std::string name;
    std::string unit;
    int length;
    as::DataFormat type;
    CheckPattern check_pattern;
    std::vector<char> data;
    // Constructor for safety
    AttrConfig(const std::string& n, const std::string& u, int len, as::DataFormat t, CheckPattern c)
        : name(n), unit(u), length(len), type(t),  check_pattern(c){
    }    
};

struct AttrData {
    void* data;
    size_t length;
    as::DataFormat type;
    AttrData(void* d, size_t len, as::DataFormat t)
        : data(d), length(len), type(t){
    }     
};

struct Testexpect {
    std::string pkey_name;
    std::string skey_name;
    std::string trace_name;
    std::string trace_unit;
    int fpkey;
    int lpkey;
    int fskey;
    int lskey;
    int pkinc;
    int skinc;
    int trace_length;
    float tmin;
    float tmax;
    int num_skey;
    int current_pkey;
    int group_size;

    std::vector<AttrConfig> attrs;
    void* logger;
};

std::string to_string(CheckPattern c);
CheckPattern to_checkpattern(std::string s);
void* get_and_check_data_valid(Testexpect* my_data, std::string  attr_name, int length, as::DataFormat format, std::map<std::string, AttrData>& variables);
bool is_equal_float_double(float a, double b);
bool is_equal_to_specified_data(void* data, size_t data_len, size_t data_idx, as::DataFormat data_fmt, double b);
bool load_data (std::vector<char>& data, const std::string& file_name, size_t length);

#ifdef __cplusplus
    extern "C" {
#endif

void testexpect_init(const char* myid, const char* mod_cfg);
void testexpect_process(const char* myid);

#ifdef __cplusplus
    }
#endif

#endif /* ifndef TESTGENEXPECT_H */
