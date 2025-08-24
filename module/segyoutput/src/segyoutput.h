#ifndef SEGYOUTPUT_H
#define SEGYOUTPUT_H

#include <vector>
#include <memory>
#include <GdLogger.h>
#include <VdsStore.h>
#include <string>
#include "segy_writer.h"

struct Segyoutput {
    std::string output_url;
    SEGYWriter segy_writer;
    
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
    
    std::vector<int> pkeys;
    std::vector<int> skeys;
    
    // Output-specific fields
    bool file_initialized;
    bool header_written;
    int64_t traces_written;
    int64_t total_expected_traces;
    
    void* logger;
    
    Segyoutput() : segy_writer(nullptr), is_dry_run(false), file_initialized(false), 
                   header_written(false), traces_written(0), total_expected_traces(0),
                   primary_offset(0), secondary_offset(0), sinterval_offset(0),
                   trace_length_offset(0), data_format_code_offset(0),
                   fpkey(0), lpkey(0), fskey(0), lskey(0), pkinc(1), skinc(1),
                   trace_length(0), sinterval(0), tmin(0.0f), tmax(0.0f),
                   num_skey(0), num_pkey(0), current_pkey(0), logger(nullptr) {}
    
    ~Segyoutput() {
        if (segy_writer) {
            segy_writer->finalize();
            delete segy_writer;
            segy_writer = nullptr;
        }
    }
};

#ifdef __cplusplus
extern "C" {
#endif

void segyoutput_init(const char* myid, const char* mod_cfg);
void segyoutput_process(const char* myid);

#ifdef __cplusplus
}
#endif

#endif /* ifndef SEGYOUTPUT_H */