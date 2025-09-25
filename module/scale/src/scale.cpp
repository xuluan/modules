#include "GeoDataFlow.h"
#include "ArrowStore.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include "scale.h"
#include "scale_factor.h"
#include "scale_agc.h"
#include "scale_diverge.h"

void scale_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"scale_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "scale_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Scale* my_data = new Scale {};

    my_data->logger = my_logger;

    auto& job_df = df::GeoDataFlow::GetInstance();

    // A handy function to clean up resources if errors happen
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
            delete my_data;
        }
    };

    // parse job parameters
    mc::ModuleConfig mod_conf = mc::ModuleConfig {};
    
    mod_conf.Parse(buf);

    if (mod_conf.Has("scale.method.factor")) {
        my_data->method = SCALE_METHOD_FACTOR;
        my_data->factor = mod_conf.GetFloat("scale.method.factor.value");
        if(mod_conf.HasError()) {
            gd_logger.LogError(my_logger, "Failed to get scale.method.factor. Error: {}", mod_conf.ErrorMessage().c_str());
            job_df.SetJobAborted();
            _clean_up();
            return;
        }
    } else if (mod_conf.Has("scale.method.agc")) {
        my_data->method = SCALE_METHOD_AGC;
        my_data->window_size = mod_conf.GetFloat("scale.method.agc.window_size");
        if(mod_conf.HasError()) {
            gd_logger.LogError(my_logger, "Failed to get 'scale.method.agc.window_size'. Error: {}", mod_conf.ErrorMessage().c_str());
            job_df.SetJobAborted();
            _clean_up();
            return;
        }
        
    } else if    (mod_conf.Has("scale.method.diverge")) {
        my_data->method = SCALE_METHOD_DIVERGE;
        my_data->dvg_a = mod_conf.GetFloat("scale.method.diverge.a");
        if(mod_conf.HasError()) {
            gd_logger.LogError(my_logger, "Failed to get 'scale.method.diverge.a'. Error: {}", mod_conf.ErrorMessage().c_str());
            job_df.SetJobAborted();
            _clean_up();
            return;
        }
        my_data->dvg_v = mod_conf.GetFloat("scale.method.diverge.v");
        if(mod_conf.HasError()) {
            gd_logger.LogError(my_logger, "Failed to get 'scale.method.diverge.v'. Error: {}", mod_conf.ErrorMessage().c_str());
            job_df.SetJobAborted();
            _clean_up();
            return;
        }
    } else {
        gd_logger.LogError(my_logger, "Error: unknown scaling method");
        job_df.SetJobAborted();
        _clean_up();
    }

    job_df.SetModuleStruct(myid, static_cast<void*>(my_data));
}

void scale_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Scale* my_data = static_cast<Scale*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;


    // A handy function to clean up resources if errors happen, 
    // or job has finished
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
            delete my_data;
        }
    };

    if (job_df.JobFinished()) {
        _clean_up();
        return;
    }

    int* pkey;
    pkey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
    if(pkey == nullptr) {
        gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of pkey is NULL");
        job_df.SetJobAborted();
        _clean_up();
        return;
    }

    gd_logger.LogInfo(my_logger, "Process primary key {}\n", pkey[0]);

    std::string trace_name = job_df.GetVolumeDataName();

    void *trc_void = static_cast<void *>(job_df.GetWritableBuffer(trace_name.c_str()));
    if (trc_void == nullptr) {
        gd_logger.LogError(my_logger, "nullptr to the buffer of {}", trace_name);
        // cancel the job
        job_df.SetJobAborted();
        return;
    }

    as::DataFormat trc_fmt;
    int length;
    float trc_min; 
    float trc_max;
    float trc_step;
    
    job_df.GetAttributeInfo(trace_name.c_str(), trc_fmt, length, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, length);

    // store trc info to my_data struct
    my_data->attr_name = trace_name;
    my_data->trc_data = trc_void;
    my_data->grp_size = job_df.GetGroupSize();
    my_data->trc_len = job_df.GetDataVectorLength();
    my_data->trc_fmt = trc_fmt;
    my_data->trc_min = trc_min;
    my_data->sinterval = (trc_max-trc_min)/(my_data->trc_len -1);

    bool ret;

    if (my_data->method == SCALE_METHOD_FACTOR) {
        ret = get_scale_data_factor(my_data);
    } else if (my_data->method == SCALE_METHOD_AGC) {
        ret = get_scale_data_agc(my_data);
    } else if (my_data->method == SCALE_METHOD_DIVERGE) {
        ret = get_scale_data_diverge(my_data);
    } else {
        gd_logger.LogError(my_logger, "Unsupported method.");
        job_df.SetJobAborted();
        return;
    }

    if(!ret){
        gd_logger.LogError(my_logger, "Failed to call scale method.");
        job_df.SetJobAborted();
        return;
    }

}
