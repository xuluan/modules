#include "testgendata.h"
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

void testgendata_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"testgendata_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "testgendata_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Testgendata* my_data = new Testgendata {};

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
        gutl::DynamicValue config = gutl::parse(buf);

        //parse primarykey
        auto& primarykey = config["testgendata"]["primarykey"];
        my_data->pkey_name = primarykey.at("name", "primarykey").as_string();
        gutl::UTL_StringToUpperCase(my_data->pkey_name);

        my_data->fpkey = primarykey.at("first", "primarykey").as_int();
        my_data->lpkey = primarykey.at("last", "primarykey").as_int();
        my_data->pkinc = primarykey.at("step", "primarykey").as_int();
        
        my_data->pkeys.clear();
        for (int i = my_data->fpkey; i <= my_data->lpkey;) {
            my_data->pkeys.push_back(i);
            i += my_data->pkinc;
        }

        gd_logger.LogInfo(my_logger, "Primary Axis: {}, Type: {}, Length: {}, [{} -- {}] "
            , my_data->pkey_name, "int", 1, my_data->fpkey, my_data->lpkey);

        //parse secondarykey
        auto& secondarykey = config["testgendata"]["secondarykey"];

        my_data->skey_name = secondarykey.at("name", "secondarykey").as_string();
        gutl::UTL_StringToUpperCase(my_data->skey_name);

        my_data->fskey = secondarykey.at("first", "secondarykey").as_int();
        my_data->lskey = secondarykey.at("last", "secondarykey").as_int();
        my_data->skinc = secondarykey.at("step", "secondarykey").as_int();
        my_data->num_skey = (my_data->lskey - my_data->fskey) / my_data->skinc + 1;
        
        my_data->skeys.clear();
        for (int i = my_data->fskey; i <= my_data->lskey;) {
            my_data->skeys.push_back(i);
            i += my_data->skinc;
        }

        gd_logger.LogInfo(my_logger, "Secondary Axis: {}, Type: {}, Length: {}, [{} -- {}] "
            , my_data->skey_name, "int", 1, my_data->fskey, my_data->lskey);

        //parse tracekey
        auto& tracekey = config["testgendata"]["tracekey"];

        my_data->trace_name = tracekey.at("name", "tracekey").as_string();
        gutl::UTL_StringToUpperCase(my_data->trace_name);

        my_data->trace_unit = "ms"; //tracekey.at("unit", "tracekey").as_string();

        my_data->tmin = tracekey.at("tmin", "tracekey").as_float();
        my_data->tmax = tracekey.at("tmax", "tracekey").as_float();
        my_data->trace_length = tracekey.at("length", "tracekey").as_int();

        AttrConfig attr_config;
        as::DataFormat t;
        attr_config.name = my_data->trace_name;
        attr_config.unit = my_data->trace_unit;
        attr_config.length = my_data->trace_length;

        std::string data_type = tracekey.at("data", "tracekey").as_map().begin()->first;

        if(data_type == "random") {
            attr_config.dataType = DataType::RANDOM;
            auto& tracekey_data = tracekey["data"]["random"];
            attr_config.randomData.min = tracekey_data.at("min", "tracekey_data").as_float();
            attr_config.randomData.max = tracekey_data.at("max", "tracekey_data").as_float();
            attr_config.randomData.type = as::string_to_data_format(tracekey_data.at("type", "tracekey_data").as_string());
            t = attr_config.randomData.type;
        } else if (data_type == "sequence") {
            attr_config.dataType = DataType::SEQUENCE;
            auto& tracekey_data = tracekey["data"]["sequence"];
            attr_config.sequenceData.min = tracekey_data.at("min", "tracekey_data").as_float();
            attr_config.sequenceData.max = tracekey_data.at("max", "tracekey_data").as_float();
            attr_config.sequenceData.step = tracekey_data.at("step", "tracekey_data").as_float();
            attr_config.sequenceData.type = as::string_to_data_format(tracekey_data.at("type", "tracekey_data").as_string());
            t = attr_config.randomData.type;
        } else {
            throw std::runtime_error("Error tracekey data, should be random or sequence : " + data_type);
        }

        gd_logger.LogInfo(my_logger, "Data Axis: {}, Type: {}, Length: {}, [{} -- {}] "
            , my_data->trace_name, as::data_format_to_string(t), my_data->trace_length, my_data->tmin, my_data->tmax);

        my_data->attrs.push_back(attr_config);

        // Add primary and secondary keys. 
        // Firstly, add two attributes, then set them to keys respectively
        // This way, we can check the data formats of the attributes. Only interger 
        // attributes can be used as keys
        job_df.AddAttribute(my_data->pkey_name.c_str(), as::DataFormat::FORMAT_U32, 1);
        job_df.AddAttribute(my_data->skey_name.c_str(), as::DataFormat::FORMAT_U32, 1);

        job_df.SetPrimaryKeyName(my_data->pkey_name.c_str());
        job_df.SetSecondaryKeyName(my_data->skey_name.c_str());

        // Add trace attribute
        job_df.AddAttribute(my_data->trace_name.c_str(), 
                            as::DataFormat::FORMAT_R32, 
                            my_data->trace_length);
        job_df.SetVolumeDataName(my_data->trace_name.c_str());

        job_df.SetDataAxisUnit("ms");
        // set the group size, to allocate buffers
        job_df.SetGroupSize(my_data->num_skey);

        // Set up data axis
        job_df.SetDataAxis(my_data->tmin, my_data->tmax, my_data->trace_length);

        // Set up primary key axis
        int num_pkeys = (my_data->lpkey - my_data->fpkey) / my_data->pkinc + 1;
        job_df.SetPrimaryKeyAxis(my_data->fpkey, my_data->lpkey, num_pkeys);

        // set up secondary key axis
        job_df.SetSecondaryKeyAxis(my_data->fskey, my_data->lskey, my_data->num_skey);

        // reset the buffers
        float *trc = static_cast<float*>(job_df.GetWritableBuffer(my_data->trace_name.c_str()));
        if (trc == nullptr) {
        gd_logger.LogError(my_logger, "Failed to get buffer to write for dataname. Error: {}");
        // cancel the job
        job_df.SetJobAborted();
        return;
        }

        //todo : fill trace data
        std::fill(trc, trc + my_data->num_skey * my_data->trace_length, 0.f); 


        auto& attrs = config["testgendata"]["attribute"];
        if(attrs.is_array()) {
            //parse attributes
            auto& arr = config["testgendata"]["attribute"].as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                AttrConfig attr_config;
                float min, max;
                as::DataFormat t;

                attr_config.name = arr[i].at("name", "attribute").as_string();
                gutl::UTL_StringToUpperCase(attr_config.name);

                attr_config.unit = ""; // arr[i].at("unit", "attribute").as_string();
                attr_config.length = arr[i].at("length", "attribute").as_int();

                std::string data_type = arr[i].at("data", "attribute").as_map().begin()->first;

                if(data_type == "random") {
                    attr_config.dataType = DataType::RANDOM;
                    auto& attr_data = arr[i]["data"]["random"];
                    attr_config.randomData.min = attr_data.at("min", "attr_data").as_float();
                    attr_config.randomData.max = attr_data.at("max", "attr_data").as_float();
                    attr_config.randomData.type = as::string_to_data_format(attr_data.at("type", "attr_data").as_string());

                    min = attr_config.randomData.min;
                    max = attr_config.randomData.max;
                    t = attr_config.randomData.type;
                } else if (data_type == "sequence") {
                    attr_config.dataType = DataType::SEQUENCE;
                    auto& attr_data = arr[i]["data"]["sequence"];
                    attr_config.sequenceData.min = attr_data.at("min", "attr_data").as_float();
                    attr_config.sequenceData.max = attr_data.at("max", "attr_data").as_float();
                    attr_config.sequenceData.step = attr_data.at("step", "attr_data").as_float();
                    attr_config.sequenceData.type = as::string_to_data_format(attr_data.at("type", "attr_data").as_string());
                    min = attr_config.sequenceData.min;
                    max = attr_config.sequenceData.max;
                    t = attr_config.sequenceData.type;

                } else {
                    throw std::runtime_error("Error attr data, should be random or sequence : " + data_type);
                }
                gd_logger.LogInfo(my_logger, "Attr {} Name: {}, Type: {}, Length: {},  [{} -- {}] "
                    , i, attr_config.name, as::data_format_to_string(t), attr_config.length, min, max);
                
                job_df.AddAttribute(attr_config.name.c_str(), t, attr_config.length);
                job_df.SetAttributeUnit(attr_config.name.c_str(), attr_config.unit.c_str());

                my_data->attrs.push_back(attr_config);  
            }
        }

        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

        int grp_size = job_df.GetGroupSize();

        //gen data to attribute buffer
        for (size_t i = 0; i < my_data->attrs.size(); ++i) {

            gd_logger.LogDebug(my_logger, "attr gen data: " + my_data->attrs[i].name);

            DataGenerator generator(my_data->attrs[i].name + ".DAT");
            void * data = job_df.GetWritableBuffer(my_data->attrs[i].name.c_str());
            if(my_data->attrs[i].dataType == DataType::RANDOM) {
                generator.gen_random_data(data, my_data->attrs[i].randomData, my_data->attrs[i].length * grp_size);
                
            } else if (my_data->attrs[i].dataType == DataType::SEQUENCE) {
                generator.gen_sequence_data(data, my_data->attrs[i].sequenceData, my_data->attrs[i].length * grp_size);
            }
        }

        gd_logger.FlushLog(my_logger);

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
 
}

void testgendata_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Testgendata* my_data = static_cast<Testgendata*>(job_df.GetModuleStruct(myid));
    void* my_logger = my_data->logger;

    int grp_size = job_df.GetGroupSize();

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

    // have we reached the end of data
    if (my_data->pkinc > 0) {
        if (my_data->current_pkey > my_data->lpkey) {
        // set the flag
        job_df.SetJobFinished();
        return;
        }

    }
    else {
        if (my_data->current_pkey < my_data->lpkey) {
        // set the flag
        job_df.SetJobFinished();
        return;
        }
    }

    int* pkey;
    pkey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
    if(pkey == nullptr) {
        gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of pkey is NULL");
        job_df.SetJobAborted();
        _clean_up();
        return;
    }

    std::fill(pkey, pkey + grp_size, my_data->current_pkey);
    gd_logger.LogInfo(my_logger, "Process primary key {}\n", pkey[0]);

    int* skey;
    skey = static_cast<int*>(job_df.GetWritableBuffer(job_df.GetSecondaryKeyName()));
    if(skey == nullptr) {
        gd_logger.LogError(my_logger, "DF returned a nullptr to the buffer of skey is NULL");
        job_df.SetJobAborted();
        _clean_up();
        return;
    }

    std::copy(my_data->skeys.begin(), my_data->skeys.end(), skey);
      
    //gen data to attribute buffer
    for (size_t i = 0; i < my_data->attrs.size(); ++i) {

        gd_logger.LogDebug(my_logger, "attr gen data: " + my_data->attrs[i].name);

        DataGenerator generator(my_data->attrs[i].name + ".DAT");
        void * data = job_df.GetWritableBuffer(my_data->attrs[i].name.c_str());
        if(my_data->attrs[i].dataType == DataType::RANDOM) {
            generator.gen_random_data(data, my_data->attrs[i].randomData, my_data->attrs[i].length * grp_size);
            
        }
    }

    // prepare for the next call
    my_data->current_pkey = my_data->current_pkey + my_data->pkinc;
}
