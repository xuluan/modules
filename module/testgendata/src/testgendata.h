#ifndef TESTGENDATA_H
#define TESTGENDATA_H

#include <vector>
#include <string>
#include "data_generator.h"

enum class DataType {
    RANDOM,
    SEQUENCE
};

struct AttrConfig {
    std::string name;
    std::string unit;
    int length;
    DataType dataType;

    union {
        RandomData randomData;
        SequenceData sequenceData;
    };
};


struct Testgendata {
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
    float* trace_buf;
    std::vector<int> pkeys;
    std::vector<int> skeys;
    std::vector<AttrConfig> attrs;
    void* logger;
};

#ifdef __cplusplus
    extern "C" {
#endif

void testgendata_init(const char* myid, const char* mod_cfg);
void testgendata_process(const char* myid);

#ifdef __cplusplus
    }
#endif

#endif /* ifndef TESTGENDATA_H */
