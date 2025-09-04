#include "segyinput.h"
#include "ArrowStore.h"
#include "GeoDataFlow.h"
#include <ModuleConfig.h>
#include <GdLogger.h>
#include <string>
#include <VdsStore.h>
#include <filesystem>
#include "fort.hpp"
#include <utl_yaml_parser.h>
#include <utl_string.h>
#include "segy_reader.h"

void segyinput_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"segyinput_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "segyinput_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Segyinput* my_data = new Segyinput {};

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

        auto& segyin_config = config["segyinput"];

        //parse data_url
        my_data->data_url = segyin_config.at("url", "segyinput").as_string();
        if (my_data->data_url.empty()) {
            throw std::runtime_error("Error: segyinput data_url is empty");
        }
        gd_logger.LogInfo(my_logger, "segyinput data_url: {}", my_data->data_url);

        if (!std::filesystem::exists(my_data->data_url)) {
            throw std::runtime_error("Error: segyinput data_url does not exist: " + my_data->data_url);
        }

        //parse run_mode
        std::string run_mode = segyin_config.at("run_mode", "segyinput").as_string();
        if (run_mode != "dry-run" && run_mode != "actual-run") {
            throw std::runtime_error("Error: segyinput run_mode is invalid: " + run_mode);
        }
      
        gd_logger.LogInfo(my_logger, "segyinput run_mode: {}", run_mode);
            
        

        if (run_mode == "dry-run") {
            my_data->is_dry_run = true;

            if (!my_data->segy_reader.printTextualHeader(my_data->data_url)) {
                throw std::runtime_error("Error: failed to print textual header from segy file: " + my_data->data_url);
            }

        } else { // actual-run
            my_data->is_dry_run = false;
            my_data->pkey_name = segyin_config.at("primary_name", "segyinput").as_string();
            gutl::UTL_StringToUpperCase(my_data->pkey_name);
            my_data->skey_name = segyin_config.at("secondary_name", "segyinput").as_string();
            gutl::UTL_StringToUpperCase(my_data->skey_name);
            my_data->trace_name = segyin_config.at("data_name", "segyinput").as_string();
            gutl::UTL_StringToUpperCase(my_data->trace_name);

            my_data->primary_offset = segyin_config.at("primary_offset", "segyinput").as_int();
            my_data->secondary_offset = segyin_config.at("secondary_offset", "segyinput").as_int();
            my_data->sinterval_offset = segyin_config.at("sinterval_offset", "segyinput").as_int();
            my_data->trace_length_offset = segyin_config.at("trace_length_offset", "segyinput").as_int();
            my_data->data_format_code_offset = segyin_config.at("data_format_code_offset", "segyinput").as_int();
            my_data->segy_reader.addCustomField("inlinenumber", my_data->primary_offset, 4);
            my_data->segy_reader.addCustomField("crosslinenumber", my_data->secondary_offset, 4);
            my_data->segy_reader.addCustomField("numSamplesKey", my_data->trace_length_offset, 2);
            my_data->segy_reader.addCustomField("sampleIntervalKey", my_data->sinterval_offset, 2);
            my_data->segy_reader.addCustomField("dataSampleFormatCodeKey", my_data->data_format_code_offset, 2);


            if(!my_data->segy_reader.initialize(my_data->data_url)) {
                throw std::runtime_error("Error: failed to initialize SEGY reader for file: " + my_data->data_url + ", Error msg: " + my_data->segy_reader.getErrMsg());
            }

            // get segy info
            my_data->segy_reader.getPrimaryKeyAxis(my_data->fpkey, my_data->lpkey, my_data->num_pkey, my_data->pkinc);
            my_data->segy_reader.getSecondaryKeyAxis(my_data->fskey, my_data->lskey, my_data->num_skey, my_data->skinc);
            my_data->segy_reader.getDataAxis(my_data->tmin, my_data->tmax, my_data->trace_length, my_data->sinterval);

            gd_logger.LogInfo(my_logger, "getPrimaryKeyAxis {} {} {} {}", my_data->fpkey, my_data->lpkey, my_data->num_pkey, my_data->pkinc);
            gd_logger.LogInfo(my_logger, "getSecondaryKeyAxis {} {} {} {}", my_data->fskey, my_data->lskey, my_data->num_skey, my_data->skinc);
            gd_logger.LogInfo(my_logger, "getDataAxis {} {} {} {}", my_data->tmin, my_data->tmax, my_data->trace_length, my_data->sinterval);

            try {
                int i = segyin_config.at("primary_start", "segyinput").as_int();
                if((i <= my_data->lpkey) && (i >= my_data->fpkey)) { //valid
                    my_data->fpkey = (i - my_data->lpkey)/my_data->pkinc * my_data->pkinc + my_data->lpkey;
                    gd_logger.LogInfo(my_logger, "my_data->fpkey INPUT: {} UPDATE {}", i, my_data->fpkey);
                }
            } catch (const std::exception& e) {
                gd_logger.LogDebug(my_logger, e.what());
            }

            try {
                int i = segyin_config.at("primary_end", "segyinput").as_int();
                if((i <= my_data->lpkey) && (i >= my_data->fpkey)) { //valid
                    my_data->lpkey = i;
                    gd_logger.LogInfo(my_logger, "my_data->lpkey INPUT: {} UPDATE {}", i, my_data->lpkey);
                }
            } catch (const std::exception& e) {
                gd_logger.LogDebug(my_logger, e.what());
            }

            my_data->num_pkey = (my_data->lpkey - my_data->fpkey)/my_data->pkinc + 1;

            try {
                int i = segyin_config.at("secondary_start", "segyinput").as_int();
                if((i <= my_data->lskey) && (i >= my_data->fskey)) { //valid
                    my_data->fskey = i;
                    gd_logger.LogInfo(my_logger, "my_data->fskey INPUT: {} UPDATE {}", i, my_data->fskey);
                }
            } catch (const std::exception& e) {
                gd_logger.LogDebug(my_logger, e.what());
            }

            try {
                int i = segyin_config.at("secondary_end", "segyinput").as_int();
                if((i <= my_data->lskey) && (i >= my_data->fskey)) { //valid
                    my_data->lskey = i;
                    gd_logger.LogInfo(my_logger, "my_data->lskey INPUT: {} UPDATE {}", i, my_data->lskey);

                }
            } catch (const std::exception& e) {
                gd_logger.LogDebug(my_logger, e.what());
            }
            my_data->num_skey = (my_data->lskey - my_data->fskey)/my_data->skinc + 1;

            my_data->trace_start = 0;
            my_data->trace_end = my_data->trace_length - 1;

            try {
                int i = segyin_config.at("trace_start", "segyinput").as_int();
                if((i <= my_data->trace_end) && (i >= my_data->trace_start)) { //valid
                    my_data->trace_start = i;
                    my_data->tmin = my_data->sinterval/ 1000.0 * i;
                    gd_logger.LogInfo(my_logger, "my_data->trace_start INPUT: {} UPDATE {}", i, my_data->trace_start);
                }
            } catch (const std::exception& e) {
                gd_logger.LogDebug(my_logger, e.what());
            }

            try {
                int i = segyin_config.at("trace_end", "segyinput").as_int();
                if((i <= my_data->trace_end) && (i >= my_data->trace_start)) { //valid
                    my_data->trace_end = i;
                    my_data->tmax = my_data->sinterval/ 1000.0 * i;
                    gd_logger.LogInfo(my_logger, "my_data->trace_end INPUT: {} UPDATE {}", i, my_data->trace_end);
                }
            } catch (const std::exception& e) {
                gd_logger.LogDebug(my_logger, e.what());
            }

            my_data->trace_length = my_data->trace_end - my_data->trace_start + 1;

            gd_logger.LogInfo(my_logger, "Updated:");
            
            gd_logger.LogInfo(my_logger, "getPrimaryKeyAxis {} {} {} {}", my_data->fpkey, my_data->lpkey, my_data->num_pkey, my_data->pkinc);
            gd_logger.LogInfo(my_logger, "getSecondaryKeyAxis {} {} {} {}", my_data->fskey, my_data->lskey, my_data->num_skey, my_data->skinc);
            gd_logger.LogInfo(my_logger, "getDataAxis {} {} {} {}", my_data->tmin, my_data->tmax, my_data->trace_length, my_data->sinterval);

            my_data->current_pkey = my_data->fpkey;

            // Add primary and secondary attribute
            job_df.AddAttribute(my_data->pkey_name.c_str(), as::DataFormat::FORMAT_U32, 1);
            job_df.AddAttribute(my_data->skey_name.c_str(), as::DataFormat::FORMAT_U32, 1);

            job_df.SetPrimaryKeyName(my_data->pkey_name.c_str());
            job_df.SetSecondaryKeyName(my_data->skey_name.c_str());
        
            // Add trace attribute
            as::DataFormat type;
            SEGY::DataSampleFormatCode format = my_data->segy_reader.getSampleFormatCode();
            switch (format) {
                case SEGY::DataSampleFormatCode::Int8:
                    type = as::DataFormat::FORMAT_U8;
                    break;
                case SEGY::DataSampleFormatCode::Int16:
                    type = as::DataFormat::FORMAT_U16;
                    break;
                case SEGY::DataSampleFormatCode::Int32:
                    type = as::DataFormat::FORMAT_U32;
                    break;
                case SEGY::DataSampleFormatCode::IEEEFloat:
                    type = as::DataFormat::FORMAT_R32;
                    break;
                default:
                    throw std::runtime_error("Error: unsupported SEGY data sample format");
            }

            gd_logger.LogInfo(my_logger, "trace type : {}", std::to_string(static_cast<int>(type)));
            job_df.AddAttribute(my_data->trace_name.c_str(), type, my_data->trace_length);
            
            job_df.SetVolumeDataName(my_data->trace_name.c_str());    
            
            job_df.SetDataAxisUnit("ms");
            // set the group size, to allocate buffers
            job_df.SetGroupSize(my_data->num_skey);

            // Set up data axis
            job_df.SetDataAxis(my_data->tmin, my_data->tmax, my_data->trace_length);

            // Set up primary key axis
            job_df.SetPrimaryKeyAxis(my_data->fpkey, my_data->lpkey, my_data->num_pkey);

            // set up secondary key axis
            job_df.SetSecondaryKeyAxis(my_data->fskey, my_data->lskey, my_data->num_skey);

            my_data->skeys.clear();
            for (int i = my_data->fskey; i <= my_data->lskey;) {
                my_data->skeys.push_back(i);
                i += my_data->skinc;
            }

            // add sttributes
            auto& attrs = config["segyinput"]["attribute"];
            if(attrs.is_array()) {
                //parse attributes
                auto& arr = config["segyinput"]["attribute"].as_array();
                for (size_t i = 0; i < arr.size(); ++i) {
                    auto& attr = arr[i];
                    std::string name = attr.at("name", "attribute").as_string();
                    gutl::UTL_StringToUpperCase(name);

                    std::string datatype = attr.at("datatype", "attribute").as_string();
                    int offset = attr.at("offset", "attribute").as_int();
                    SEGY::DataSampleFormatCode format;
                    as::DataFormat type;

                    if (datatype == "int8") {
                        format = SEGY::DataSampleFormatCode::Int8;
                        type = as::DataFormat::FORMAT_U8;
                    } else if (datatype == "int16") {
                        format = SEGY::DataSampleFormatCode::Int16;
                        type = as::DataFormat::FORMAT_U16;
                    } else if (datatype == "int32") {
                        format = SEGY::DataSampleFormatCode::Int32;
                        type = as::DataFormat::FORMAT_U32;
                    } else if (datatype == "float") {
                        format = SEGY::DataSampleFormatCode::IEEEFloat;
                        type = as::DataFormat::FORMAT_R32;
                    } else {
                        throw std::runtime_error("Error: segyinput the datatype of attribute " + name + " is invalid: " + datatype);
                    }
                    my_data->segy_reader.addAttrField(name, offset, 4, format);
                    job_df.AddAttribute(name.c_str(), type, 1);
                    job_df.SetAttributeUnit(name.c_str(), "");
                }
            }   
            my_data->segy_reader.printFileInfo();
        }
            
        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }

}

void segyinput_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Segyinput* my_data = static_cast<Segyinput*>(job_df.GetModuleStruct(myid));
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

    if(my_data->is_dry_run) {
        // set the flag
        job_df.SetJobFinished();
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

    // setup primary and secondary

    int grp_size = job_df.GetGroupSize();
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
    
    // setup data and attributes
    std::string primary_name = job_df.GetPrimaryKeyName();

    std::string secondary_name = job_df.GetSecondaryKeyName();

    std::string data_name = job_df.GetVolumeDataName();

    //get primary and secondary
    for(int i = 0; i < job_df.GetNumAttributes(); i++) {

        std::string attr_name = job_df.GetAttributeName(i);

        if((attr_name == primary_name) || (attr_name == secondary_name)) 
            continue;

        void *data = job_df.GetWritableBuffer(attr_name.c_str());

        if(attr_name == data_name) {
            if(!my_data->segy_reader.readTraceByPriIdx(my_data->current_pkey, my_data->fskey, my_data->lskey, my_data->trace_start, my_data->trace_end, data)) {
                throw std::runtime_error("Error: read trace " + my_data->segy_reader.getErrMsg());
            }
        } else {
            if(!my_data->segy_reader.readAttrByPriIdx(attr_name, my_data->current_pkey, my_data->fskey, my_data->lskey, data)) {
                throw std::runtime_error("Error: read attribute " + my_data->segy_reader.getErrMsg());
            }
        }

    }

    // prepare for the next call
    my_data->current_pkey = my_data->current_pkey + my_data->pkinc;
}
