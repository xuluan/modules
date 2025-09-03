#ifndef VDSOUTPUT_H
#define VDSOUTPUT_H

#include <vector>
#include <memory>
#include <GdLogger.h>
#include <VdsStore.h>
#include <string>
#include "Converter.h"

struct Vdsoutput {
    std::string url;
    std::unique_ptr<Converter> m_converter;

    std::string pkey_name;
    std::string skey_name;
    std::string trace_name;
    int primary_offset;
    int secondary_offset;
    int sinterval_offset;
    int trace_length_offset;
    int data_format_code_offset;
    
    int fpkey;
    int lpkey;
    int fskey;
    int lskey;
    int pkinc;
    int skinc;
    int trace_length;
    int trace_start;
    int trace_end;      
    int sinterval;
    float tmin;
    float tmax;
    int num_skey;
    int num_pkey;
    int current_pkey;


    std::vector<std::string> attributes;
    OpenVDS::CompressionMethod compression_method;
    float tolerance;
    int lod_levels;
    int brick_size;
    
    void* logger;
};

#ifdef __cplusplus
extern "C" {
#endif

void vdsoutput_init(const char* myid, const char* mod_cfg);
void vdsoutput_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef VDSOUTPUT_H */