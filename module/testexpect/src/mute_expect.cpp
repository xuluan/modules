
#include "testexpect.h"
#include "attrcalc_expect.h"
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include <GdLogger.h>


// <3000, >9000, window_size=0
bool check_data_mute_3000_9000_0(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    
    int length =  attr_data.length;

    float* dst = static_cast<float *> (attr_data.data);

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }
    
    // get time info
    auto& job_df = df::GeoDataFlow::GetInstance();
    size_t grp_size = job_df.GetGroupSize();
    size_t trc_length = job_df.GetDataVectorLength();

    std::string trc_name = job_df.GetVolumeDataName();
    as::DataFormat trc_fmt;
    int ttt_length;
    float trc_min; 
    float trc_max;
    float trc_step;
    int time_offset;
    float except_val;
    int grp_id;
    int trc_id;
    
    job_df.GetAttributeInfo(trc_name.c_str(), trc_fmt, ttt_length, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, ttt_length);
    trc_step = (trc_max - trc_min)/trc_length;


    for(int i = 0; i < length; ++i) {

        grp_id = i / trc_length;
        trc_id = i % trc_length;
        
        time_offset = trc_min + trc_id*trc_step;
        
        if (time_offset <= 3000) {
            except_val = 0.0;
        } else if (time_offset >= 9000) {
            except_val = 0.0;
        } else {
            except_val = 100.0;
        }

        if(!is_equal_float_double(dst[i], except_val)) {
            gd_logger.LogInfo(my_data->logger, "idx={} [{}][{}] time_offset={} val={} expect={}",
                 i, grp_id, trc_id, time_offset, dst[i], except_val);
            throw std::runtime_error("check_data fail, at index " + std::to_string(i) + ", expect " + std::to_string(except_val) + " but got " + std::to_string(dst[i]));
        } 


    }

    return true;
}

// <3000, >9000, window_size=2000
bool check_data_mute_3000_9000_plus_2000(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    
    int length =  attr_data.length;

    float* dst = static_cast<float *> (attr_data.data);

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }
    
    // get time info
    auto& job_df = df::GeoDataFlow::GetInstance();
    size_t grp_size = job_df.GetGroupSize();
    size_t trc_length = job_df.GetDataVectorLength();

    std::string trc_name = job_df.GetVolumeDataName();
    as::DataFormat trc_fmt;
    int ttt_length;
    float trc_min; 
    float trc_max;
    float trc_step;
    int time_offset;
    float except_val;
    int grp_id;
    int trc_id;
    
    job_df.GetAttributeInfo(trc_name.c_str(), trc_fmt, ttt_length, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, ttt_length);
    trc_step = (trc_max - trc_min)/trc_length;


    for(int i = 0; i < length; ++i) {
        
        grp_id = i / trc_length;
        trc_id = i % trc_length;
        
        time_offset = trc_min + trc_id*trc_step;
        
        if (time_offset <= 3000) {
            except_val = 0.0;
        } else if (time_offset <= 3000 + 2000) {
            except_val = 100.0 * (time_offset - 3000) / 2000;     //3500->0.25, 4000->0.5, 4500->0.75, 5000->1.0
        } else if (time_offset >= 9000) {
            except_val = 0.0;
        } else if (time_offset >= 9000 - 2000) {
            except_val = 100.0 * (1.0 - 1.0 * (time_offset -7000) / 2000); //7000->1.0, 7500->0.75, 8000->0.5, 8500->0.25 
        } else {
            except_val = 100.0;
        }

        if(!is_equal_float_double(dst[i], except_val)) {
            gd_logger.LogInfo(my_data->logger, "idx={} [{}][{}] time_offset={} val={} expect={}",
                 i, grp_id, trc_id, time_offset, dst[i], except_val);
            throw std::runtime_error("check_data fail, at index " + std::to_string(i) + ", expect " + std::to_string(except_val) + " but got " + std::to_string(dst[i]));
        } 


    }

    return true;
}

// // <3000, >9000, window_size=-2000
bool check_data_mute_3000_9000_sub_2000(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    
    int length =  attr_data.length;

    float* dst = static_cast<float *> (attr_data.data);

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }
    
    // get time info
    auto& job_df = df::GeoDataFlow::GetInstance();
    size_t grp_size = job_df.GetGroupSize();
    size_t trc_length = job_df.GetDataVectorLength();

    std::string trc_name = job_df.GetVolumeDataName();
    as::DataFormat trc_fmt;
    int ttt_length;
    float trc_min; 
    float trc_max;
    float trc_step;
    int time_offset;
    float except_val;
    int grp_id;
    int trc_id;
    
    job_df.GetAttributeInfo(trc_name.c_str(), trc_fmt, ttt_length, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, ttt_length);
    trc_step = (trc_max - trc_min)/trc_length;


    for(int i = 0; i < length; ++i) {
        
        grp_id = i / trc_length;
        trc_id = i % trc_length;
        
        time_offset = trc_min + trc_id*trc_step;
        
        if (time_offset <= 3000 - 2000) {
            except_val = 0.0;
        } else if (time_offset <= 3000) {
            except_val = 100.0 * (time_offset - 1000) / 2000;     //1500->0.25, 2000->0.5, 2500->0.75, 3000->1.0
        } else if (time_offset >= 9000 + 2000) {
            except_val = 0.0;
        } else if (time_offset >= 9000) {
            except_val = 100.0 * (1.0 - 1.0 * (time_offset - 9000) / 2000); //9000->1.0, 9500->0.75, 10000->0.5, 10500->0.25 
        } else {
            except_val = 100.0;
        }

        if(!is_equal_float_double(dst[i], except_val)) {
            gd_logger.LogInfo(my_data->logger, "idx={} [{}][{}] time_offset={} val={} expect={}",
                 i, grp_id, trc_id, time_offset, dst[i], except_val);
            throw std::runtime_error("check_data fail, at index " + std::to_string(i) + ", expect " + std::to_string(except_val) + " but got " + std::to_string(dst[i]));
        } 


    }

    return true;
}

// >expr, expr=500*crossline
bool check_data_mute_gt_expr_500_mul_crossline(Testexpect* my_data, std::string&  attr_name, AttrData& attr_data, std::map<std::string, AttrData>& variables)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    
    int length =  attr_data.length;

    float* dst = static_cast<float *> (attr_data.data);

    gd_logger.LogInfo(my_data->logger, "length={}", length);

    if( !dst) {
        throw std::runtime_error("check_data fail, attr " + attr_name + "got data is null ");
    }

    
    
    // get time info
    auto& job_df = df::GeoDataFlow::GetInstance();
    size_t grp_size = job_df.GetGroupSize();
    size_t trc_length = job_df.GetDataVectorLength();

    std::string trc_name = job_df.GetVolumeDataName();
    as::DataFormat trc_fmt;
    int ttt_length;
    float trc_min; 
    float trc_max;
    float trc_step;
    int time_offset;
    float except_val;
    int grp_id;
    int trc_id;
    int threshold_val;
    
    job_df.GetAttributeInfo(trc_name.c_str(), trc_fmt, ttt_length, trc_min, trc_max);
    job_df.GetDataAxis(trc_min, trc_max, ttt_length);
    trc_step = (trc_max - trc_min)/trc_length;


    for(int i = 0; i < length; ++i) {
        
        grp_id = i / trc_length;
        trc_id = i % trc_length;

        // crossline: 11, 13, 15, 17, 19 ...
        // threshold_val: 5500, 6500, 7500 ...
        threshold_val = (11  + grp_id *2) * 500;         

        time_offset = trc_min + trc_id*trc_step;
        
        if (time_offset >= threshold_val) {
            except_val = 0.0;
        } else {
            except_val = 100.0;
        }

        if(!is_equal_float_double(dst[i], except_val)) {
            gd_logger.LogInfo(my_data->logger, "idx={} [{}][{}] time_offset={} val={} expect={}",
                 i, grp_id, trc_id, time_offset, dst[i], except_val);
            throw std::runtime_error("check_data fail, at index " + std::to_string(i) + ", expect " + std::to_string(except_val) + " but got " + std::to_string(dst[i]));
        } 


    }

    return true;
}