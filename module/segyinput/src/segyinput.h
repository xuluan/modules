#ifndef SEGYINPUT_H
#define SEGYINPUT_H

#include <vector>
#include <memory>
#include <GdLogger.h>
#include <VdsStore.h>
#include <string>
#include "segy_reader.h"

struct Segyinput {
    std::string data_url;
    bool is_dry_run;

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
    int sinterval;
    float tmin;
    float tmax;
    int num_skey;
    int num_pkey;
    int current_pkey;
    void* logger;
};

#ifdef __cplusplus
extern "C" {
#endif

void segyinput_init(const char* myid, const char* mod_cfg);
void segyinput_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef SEGYINPUT_H */
