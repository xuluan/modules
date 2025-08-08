#ifndef TESTGENEXPECT_H
#define TESTGENEXPECT_H

#include <vector>
#include <string>
#include "ArrowStore.h"

enum class CheckPattern {
    SAME,  //no change, default
    ATTR_PLUS_MUL //  INLINE+CROSSLINE*2.7
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

#ifdef __cplusplus
    extern "C" {
#endif

void testexpect_init(const char* myid, const char* mod_cfg);
void testexpect_process(const char* myid);

#ifdef __cplusplus
    }
#endif

#endif /* ifndef TESTGENEXPECT_H */
