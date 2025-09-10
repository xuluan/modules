#include "mute.h"
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <vector>
#include <cmath>
#include "fort.hpp"
#include <iostream>
#include <utl_yaml_parser.h>
#include <utl_string.h>

#define DEBUG_DUMP 1

void mute_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"mute_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "mute_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Mute* my_data = new Mute {};

    my_data->logger = my_logger;

    auto& job_df = df::GeoDataFlow::GetInstance();

    // A handy function to clean up resources if errors happen
    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
        delete my_data;
        }
    };

    try {

        // parse job parameters
        mc::ModuleConfig mod_conf {};  
        mod_conf.Parse(buf);
        mod_conf.GetText("mute.compare_direction", my_data->compare_direction);
        if(mod_conf.HasError()) {
            throw std::runtime_error("Failed to get mute compare_direction. Error: " + mod_conf.ErrorMessage());
        }
        if (my_data->compare_direction != "<" && my_data->compare_direction != ">" ) {
            throw std::runtime_error("compare_direction is invalid: " + my_data->compare_direction);
        }

        if (mod_conf.Has("mute.threshold.value")) {
            my_data->expr_enable = false;
            my_data->threshold_value = mod_conf.GetInt("mute.threshold.value");
            if(mod_conf.HasError()) {
                throw std::runtime_error("Failed to get mute threshold.value. Error: " + mod_conf.ErrorMessage());
            }

        }else if (mod_conf.Has("mute.threshold.expr")) {
            my_data->expr_enable = true;
            mod_conf.GetText("mute.threshold.expr", my_data->threshold_expr);
            if(mod_conf.HasError()) {
                throw std::runtime_error("Failed to get mute threshold.expr. Error: " + mod_conf.ErrorMessage());
            }

        } else{
            throw std::runtime_error("Failed to get mute threshold. Error: " + mod_conf.ErrorMessage());
        }

        gutl::UTL_StringToUpperCase(my_data->threshold_expr);

        my_data->tapering_window_size = mod_conf.GetInt("mute.tapering_window_size");

        if(mod_conf.HasError()) {
            throw std::runtime_error("Failed to get mute tapering_window_size. Error: " + mod_conf.ErrorMessage());
        }

        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));
        
        if (my_data->expr_enable) {
    
            // Check the validity of the expression 

            std::vector<std::string> variables;
            std::map<std::string, int> vars_map;

            const char* attr_name;
            int length;
            as::DataFormat attr_fmt;
            float min;
            float max;

            for(int i = 0; i< job_df.GetNumAttributes(); i++) {
                attr_name = job_df.GetAttributeName(i);
                job_df.GetAttributeInfo(attr_name, attr_fmt, length, min, max);
                variables.push_back(attr_name);
                vars_map[attr_name] = length;
                // gd_logger.LogInfo(my_logger, "attribute '{}' length is {}.", attr_name, length);
                
            }

            //parse the expression
            gexpr::ExpressionParser parser;
            bool success = parser.parse(my_data->threshold_expr, variables, my_data->expression);
            if(!success) {
                throw std::runtime_error(parser.get_errors());
            }
        
            for (const std::string& str : parser.get_used_variables()) {

                if(vars_map[str] != 1) {
                    throw std::runtime_error("Attribute length should be 1, but " + str + " length = " + std::to_string(vars_map[str]));                    
                }
            }

            // my_data->used_variables = parser.get_used_variables();

        }

        gd_logger.FlushLog(my_logger);

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
 
}

void mute_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Mute* my_data = static_cast<Mute*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;

    auto _clean_up = [&] ()-> void {
        if (my_data != nullptr) {
        delete my_data;
        }
    };

    if (job_df.JobFinished()) {
        _clean_up();
        return;
    }

    try {

        int* pkey;
        pkey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
        if(pkey == nullptr) {
            throw std::runtime_error("DF returned a nullptr to the buffer of pkey is NULL");
        }

        size_t grp_size = job_df.GetGroupSize();
        size_t trc_length = job_df.GetDataVectorLength();


        gd_logger.LogInfo(my_logger, "Process primary key {}", pkey[0]);

        // 1. Calculate the value of the threshold. 
        
        std::vector<int> threshold_values(grp_size);

        if (my_data->expr_enable) {

            // Calculate the threshold according to the expression.
            std::map<std::string, gexpr::AttrData> variables;

            const char* attr_name;
            int length;
            as::DataFormat attr_fmt;
            float min;
            float max;
            void* data;
            gexpr::AttrData attr_data;
            std::vector<double> result_data(grp_size);
            gexpr::AttrData result_attr = {result_data.data(), (size_t)grp_size, as::DataFormat::FORMAT_R64};

            for(int i = 0; i< job_df.GetNumAttributes(); i++) {
                attr_name = job_df.GetAttributeName(i);
                job_df.GetAttributeInfo(attr_name, attr_fmt, length, min, max);
                data = job_df.GetWritableBuffer(attr_name);
                attr_data = {data, (size_t)length * grp_size, attr_fmt};
                variables[attr_name] = attr_data;
            }

            gexpr::ExpressionEvaluator evaluator;
            bool success = evaluator.evaluate(my_data->expression, variables, &result_attr);
            
            if(!success) {
                throw std::runtime_error(evaluator.get_errors());
            }

            // Convert data type to integer
            std::transform(result_data.begin(), result_data.end(), threshold_values.begin(),
                 [](auto val) { return int(val); }); 

            // for(int i = 0; i< result_data.size(); i++){
            //     gd_logger.LogInfo(my_logger, "threshold[{}]={}", i, result_data[i]);
            // }

        } else {
            // Threshold value
            std::fill(threshold_values.begin(), threshold_values.end(),  my_data->threshold_value);

        }

        for(int i=0; i<grp_size; i++) {
            gd_logger.LogDebug(my_logger, "threshold value of group {} is {}", i, threshold_values[i]);
        }

        // 2. Calculate the mute factor for each data in each grp
 
        std::vector<float> mute_factors(grp_size * trc_length);

        // 2.1 Get trace data information
        std::string trc_name = job_df.GetVolumeDataName();
        as::DataFormat trc_fmt;
        int length;
        float trc_min; 
        float trc_max;
        float trc_step;
        
        job_df.GetAttributeInfo(trc_name.c_str(), trc_fmt, length, trc_min, trc_max);
        job_df.GetDataAxis(trc_min, trc_max, length);
        trc_step = (trc_max - trc_min)/length;
        float *trc = static_cast<float*>(job_df.GetWritableBuffer(trc_name.c_str()));
        if (trc == nullptr) {
            gd_logger.LogError(my_logger, "Failed to get buffer to write for dataname. Error: {}");
            // cancel the job
            job_df.SetJobAborted();
            return;
        }

        gd_logger.LogInfo(my_logger, "Trace data info: length={} step={}. {}--{}", trc_length, trc_step, trc_min, trc_max);

        // 2.2 Calculate the mute factor for each grp

        for(int i=0; i<grp_size; i++) {

            int wind_left, wind_right, tapering_window_size;

            // Mute range and window position

            if (my_data->compare_direction == ">") {
                // ">", Perform the mute operation on the right side
                if (my_data->tapering_window_size >= 0) {
                    tapering_window_size = my_data->tapering_window_size;
                    wind_left = threshold_values[i] - tapering_window_size;
                    wind_right =  threshold_values[i];
                    
                } else {
                    tapering_window_size = my_data->tapering_window_size * -1;
                    wind_left = threshold_values[i];
                    wind_right = threshold_values[i] + tapering_window_size;
                }
            } else {
                // "<", Perform the mute operation on the left side
                if (my_data->tapering_window_size >= 0) {
                    tapering_window_size = my_data->tapering_window_size;
                    wind_left = threshold_values[i];
                    wind_right = threshold_values[i] + tapering_window_size;  
                } else {
                    tapering_window_size = my_data->tapering_window_size * -1;
                    wind_left = threshold_values[i] - tapering_window_size;
                    wind_right = threshold_values[i];
                }
            }
            
            gd_logger.LogDebug(my_logger, "threshold value of group {}. threshold={}, window={} - {}", i,
                            threshold_values[i], wind_left, wind_right);

            // Calculate the mute factor for each item

            for(int trc_idx=0; trc_idx < trc_length; trc_idx++){
                // ">"
                int time_offset = trc_min + trc_idx*trc_step;

                if (my_data->compare_direction == ">") {
                    
                    if (time_offset < wind_left) {
                        mute_factors[i*trc_length + trc_idx] = 1.0;
                        // gd_logger.LogInfo(my_logger, "Trace data[{}][{}] time={} factor=1", i, trc_idx, time_offset);
                    } else if (time_offset < wind_right) {
                        if (tapering_window_size !=0 ) {
                            mute_factors[i*trc_length + trc_idx] = 1.0*(tapering_window_size - (time_offset-wind_left))/tapering_window_size;
                            // gd_logger.LogInfo(my_logger, "Trace data[{}][{}] time={} factor={:.2f}", i,
                            //     trc_idx, time_offset, 1.0*(tapering_window_size - (time_offset-wind_left))/tapering_window_size);
                        }
                    } else {
                        mute_factors[i*trc_length + trc_idx] = 0.0;
                        // gd_logger.LogInfo(my_logger, "Trace data[{}][{}] time={} factor=0.", i, trc_idx, time_offset);
                    }
                } else {
                    if (time_offset <= wind_left) {
                        mute_factors[i*trc_length + trc_idx] = 0.0;
                        // gd_logger.LogInfo(my_logger, "Trace data[{}][{}] time={} factor=0", i, trc_idx, time_offset);
                    } else if (time_offset <= wind_right) {
                        if (tapering_window_size !=0 ) {
                        mute_factors[i*trc_length + trc_idx] =  1.0*(time_offset-wind_left)/tapering_window_size;
                            // gd_logger.LogInfo(my_logger, "Trace data[{}][{}] time={} factor={:.2f}", i,
                            //     trc_idx, time_offset, 1.0*(time_offset-wind_left)/tapering_window_size);
                        }
                    } else {
                        mute_factors[i*trc_length + trc_idx] = 1.0;
                        // gd_logger.LogInfo(my_logger, "Trace data[{}][{}] time={} factor=1", i, trc_idx, time_offset);
                    }  
                }

            }

        }

        // Multiply the two arrays to obtain the result:  'TRACE_DATA * FACTOR'

        std::vector<double> mute_result_data(grp_size*trc_length);
        gexpr::AttrData mute_result_attr = {mute_result_data.data(), (size_t)trc_length*grp_size, as::DataFormat::FORMAT_R64};

        // gexpr::ExpressionEvaluator mute_evaluator;
        // std::string mute_expression_string = trc_name + "*FACTOR";
        // gexpr::ExpressionTree mute_expression;

        // std::map<std::string, gexpr::AttrData> mute_variables;
        // gexpr::AttrData mute_attr_data;

        // std::vector<std::string> mute_variables_name = {trc_name, "FACTOR"};
        
        // mute_attr_data = {job_df.GetWritableBuffer(trc_name.c_str()),  (size_t)grp_size * trc_length, trc_fmt};
        // mute_variables[trc_name] = mute_attr_data;

        // mute_attr_data = {mute_factors.data(), (size_t)grp_size * trc_length, as::DataFormat::FORMAT_R32};
        // mute_variables["FACTOR"] = mute_attr_data;

        // gexpr::ExpressionParser mute_parser;
        // bool mute_success;
        // mute_success = mute_parser.parse(mute_expression_string, mute_variables_name, mute_expression);

        // if(!mute_success) {
        //     throw std::runtime_error(mute_parser.get_errors());
        // } else {
        //     gd_logger.LogInfo(my_logger, "expression parse success." + mute_expression_string);
        // }

        // mute_success = mute_evaluator.evaluate(mute_expression, mute_variables, &mute_result_attr);
        // if(!mute_success) {
        //     throw std::runtime_error(mute_evaluator.get_errors());
        // }

        // Use vector_compute() to compute the result
        
        gexpr::AttrData mute_attr_data_trc, mute_attr_data_factor, mute_attr_data;
        mute_attr_data_trc = {job_df.GetWritableBuffer(trc_name.c_str()),  (size_t)grp_size * trc_length, trc_fmt};
        mute_attr_data_factor = {mute_factors.data(), (size_t)grp_size * trc_length, as::DataFormat::FORMAT_R32};
        bool mute_success;
        mute_success = gexpr::vector_compute(gexpr::AttributeOp::OP_MUL,
            &mute_result_attr, &mute_attr_data_trc, &mute_attr_data_factor);
        if(!mute_success) {
            throw std::runtime_error("vector compute failed!");
        }

#if DEBUG_DUMP 
        for( int skey_idx = 0; skey_idx < grp_size; skey_idx++ ) {
            for ( int trc_idx = 0; trc_idx < trc_length; trc_idx++ ) {
                auto *data = trc + skey_idx * trc_length + trc_idx;
                gd_logger.LogInfo(my_logger, "Trace Data[{:2}][{:2}] time={:8}ms {:8.2f} * {:4.2f} = {:8.2f}",
                    skey_idx, trc_idx, trc_min+trc_idx*trc_step,
                    trc[skey_idx*trc_length + trc_idx], 
                    mute_factors[skey_idx*trc_length + trc_idx], 
                    mute_result_data[skey_idx*trc_length + trc_idx]);
            }
        }
#endif
        // data type convert
        mute_attr_data = {job_df.GetWritableBuffer(trc_name.c_str()),  (size_t)grp_size * trc_length, trc_fmt};
        gexpr::convert_vector(&mute_attr_data, &mute_result_attr);

        // for( int skey_idx = 0; skey_idx < grp_size; skey_idx++ ) {
        //     for ( int trc_idx = 0; trc_idx < trc_length; trc_idx++ ) {
        //         auto *data = trc + skey_idx * trc_length + trc_idx;
        //         gd_logger.LogInfo(my_logger, "Trace Data[{:2}][{:2}] = {}", skey_idx, trc_idx, *data);
        //     }
        // }

        gd_logger.LogInfo(my_logger, "Process primary key {} finished.", pkey[0]);
        
    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
    }

}
