#include "segyoutput.h"
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

bool check_offset(int offset, int width, int header_size)
{
    if(offset > 0 && (offset + width - 1) <= header_size) {
        return true;
    }
    return false;
}

void segyoutput_init(const char* myid, const char* buf)
{
    std::string logger_name = std::string {"segyoutput_"} + myid;
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    void* my_logger = gd_logger.Init(logger_name);
    gd_logger.LogInfo(my_logger, "segyoutput_init");

    // Need to pass the pointer to DF after init function returns
    // So we use raw pointer instead of a smart pointer here
    Segyoutput* my_data = new Segyoutput {};

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

        auto& segyout_config = config["segyoutput"];

        //parse output_url
        my_data->output_url = segyout_config.at("output_url", "segyoutput").as_string();
        if (my_data->output_url.empty()) {
            throw std::runtime_error("Error: segyoutput output_url is empty");
        }
        gd_logger.LogInfo(my_logger, "segyoutput output_url: {}", my_data->output_url);

        // Check if parent directory exists
        std::filesystem::path output_path(my_data->output_url);
        std::filesystem::path parent_dir = output_path.parent_path();
        if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
            throw std::runtime_error("Error: segyoutput parent directory does not exist: " + parent_dir.string());
        }            
        // Get data flow information to configure SEGY writer
        my_data->pkey_name = job_df.GetPrimaryKeyName();
        my_data->skey_name = job_df.GetSecondaryKeyName();
        my_data->trace_name = job_df.GetVolumeDataName();
        
        gd_logger.LogInfo(my_logger, "Primary key: {}, Secondary key: {}, Trace data: {}", 
                            my_data->pkey_name, my_data->skey_name, my_data->trace_name);

        // Get axis information from data flow
        job_df.GetPrimaryKeyAxis(my_data->fpkey, my_data->lpkey, my_data->num_pkey);
        job_df.GetSecondaryKeyAxis(my_data->fskey, my_data->lskey, my_data->num_skey);
        job_df.GetDataAxis(my_data->tmin, my_data->tmax, my_data->trace_length);
        
        // Calculate increments
        my_data->pkinc = (my_data->num_pkey > 1) ? (my_data->lpkey - my_data->fpkey) / (my_data->num_pkey - 1) : 1;
        my_data->skinc = (my_data->num_skey > 1) ? (my_data->lskey - my_data->fskey) / (my_data->num_skey - 1) : 1;
        
        // Get sample interval from data flow
        my_data->sinterval = static_cast<int>(job_df.GetDataSampleRate() * 1000); // Convert to microseconds

        //update 
        try {
            int i = segyout_config.at("primary_start", "segyoutput").as_int();
            if((i <= my_data->lpkey) && (i >= my_data->fpkey)) { //valid
                my_data->fpkey = (i - my_data->lpkey)/my_data->pkinc * my_data->pkinc + my_data->lpkey;
                gd_logger.LogInfo(my_logger, "my_data->fpkey INPUT: {} UPDATE {}", i, my_data->fpkey);
            }
        } catch (const std::exception& e) {
            gd_logger.LogDebug(my_logger, e.what());
        }

        try {
            int i = segyout_config.at("primary_end", "segyoutput").as_int();
            if((i <= my_data->lpkey) && (i >= my_data->fpkey)) { //valid
                my_data->lpkey = i;
                gd_logger.LogInfo(my_logger, "my_data->lpkey INPUT: {} UPDATE {}", i, my_data->lpkey);
            }
        } catch (const std::exception& e) {
            gd_logger.LogDebug(my_logger, e.what());
        }

        my_data->num_pkey = (my_data->lpkey - my_data->fpkey)/my_data->pkinc + 1;

        try {
            int i = segyout_config.at("secondary_start", "segyoutput").as_int();
            if((i <= my_data->lskey) && (i >= my_data->fskey)) { //valid
                my_data->fskey = i;
                gd_logger.LogInfo(my_logger, "my_data->fskey INPUT: {} UPDATE {}", i, my_data->fskey);
            }
        } catch (const std::exception& e) {
            gd_logger.LogDebug(my_logger, e.what());
        }

        try {
            int i = segyout_config.at("secondary_end", "segyoutput").as_int();
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
            int i = segyout_config.at("trace_start", "segyoutput").as_int();
            if((i <= my_data->trace_end) && (i >= my_data->trace_start)) { //valid
                my_data->trace_start = i;
                my_data->tmin += my_data->sinterval/ 1000.0 * i;
                gd_logger.LogInfo(my_logger, "my_data->trace_start INPUT: {} UPDATE {}", i, my_data->trace_start);
            }
        } catch (const std::exception& e) {
            gd_logger.LogDebug(my_logger, e.what());
        }

        try {
            int i = segyout_config.at("trace_end", "segyoutput").as_int();
            if((i <= my_data->trace_end) && (i >= my_data->trace_start)) { //valid
                my_data->trace_end = i;
                my_data->tmax = my_data->tmin + (my_data->sinterval/ 1000.0 )*(i - my_data->trace_start);
                gd_logger.LogInfo(my_logger, "my_data->trace_end INPUT: {} UPDATE {}", i, my_data->trace_end);
            }
        } catch (const std::exception& e) {
            gd_logger.LogDebug(my_logger, e.what());
        }

        my_data->trace_length = my_data->trace_end - my_data->trace_start + 1;
        my_data->current_pkey = my_data->fpkey;//todo: implement logic for slice cube


        my_data->skeys.clear();
        for (int i = my_data->fskey; i <= my_data->lskey;) {
            my_data->skeys.push_back(i);
            i += my_data->skinc;
        }

        gd_logger.LogInfo(my_logger, "Primary axis: {} to {} ({} values, inc={})", 
                            my_data->fpkey, my_data->lpkey, my_data->num_pkey, my_data->pkinc);
        gd_logger.LogInfo(my_logger, "Secondary axis: {} to {} ({} values, inc={})", 
                            my_data->fskey, my_data->lskey, my_data->num_skey, my_data->skinc);
        gd_logger.LogInfo(my_logger, "Data axis: {} to {} ({} samples, interval={}μs)", 
                            my_data->tmin, my_data->tmax, my_data->trace_length, my_data->sinterval);

        // Parse header field offsets from configuration
        try {
            my_data->primary_offset = segyout_config.at("primary_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->primary_offset = 189;
        }

        try {
            my_data->secondary_offset = segyout_config.at("secondary_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->secondary_offset = 193;
        }

        try {
            my_data->sinterval_offset = segyout_config.at("sinterval_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->sinterval_offset = 17;
        }

        try {
            my_data->trace_length_offset = segyout_config.at("trace_length_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
                my_data->trace_length_offset = 21;
        }

        try {
            my_data->data_format_code_offset = segyout_config.at("data_format_code_offset", "segyoutput").as_int();
        } catch (const std::exception& e) {
            my_data->data_format_code_offset = 25;
        }

        // Determine data format from trace attribute
        as::DataFormat trace_format;
        int trace_attr_length;
        float min_val, max_val;
        job_df.GetAttributeInfo(my_data->trace_name.c_str(), trace_format, trace_attr_length, min_val, max_val);
        
        SEGY::DataSampleFormatCode segy_format;
        switch (trace_format) {
            case as::DataFormat::FORMAT_U8:
                segy_format = SEGY::DataSampleFormatCode::Int8;
                break;
            case as::DataFormat::FORMAT_U16:
                segy_format = SEGY::DataSampleFormatCode::Int16;
                break;
            case as::DataFormat::FORMAT_U32:
                segy_format = SEGY::DataSampleFormatCode::Int32;
                break;
            case as::DataFormat::FORMAT_R32:
                segy_format = SEGY::DataSampleFormatCode::IEEEFloat;
                break;
            default:
                throw std::runtime_error("Error: unsupported data format for SEGY output");
        }
        SEGYWriteInfo write_info;

        // Configure SEGY write info
        write_info.headerEndianness = SEGY::Endianness::BigEndian; // Standard SEGY big-endian
        write_info.dataSampleFormatCode = segy_format;
        write_info.sampleCount = my_data->trace_length;
        write_info.sampleInterval = my_data->sinterval;
        
        // Calculate trace byte size based on format
        int sample_size = 0;
        switch (segy_format) {
            case SEGY::DataSampleFormatCode::Int8:
                sample_size = 1;
                break;
            case SEGY::DataSampleFormatCode::Int16:
                sample_size = 2;
                break;
            case SEGY::DataSampleFormatCode::Int32:
            case SEGY::DataSampleFormatCode::IEEEFloat:
                sample_size = 4;
                break;
            default:
                throw std::runtime_error("Error: unsupported SEGY data format");
        }
        write_info.traceByteSize = my_data->trace_length * sample_size;

        // Set coordinate system
        write_info.minInline = my_data->fpkey;
        write_info.maxInline = my_data->lpkey;
        write_info.inlineCount = my_data->num_pkey;
        write_info.minCrossline = my_data->fskey;
        write_info.maxCrossline = my_data->lskey;
        write_info.crosslineCount = my_data->num_skey;
        write_info.primaryStep = my_data->pkinc;
        write_info.secondaryStep = my_data->skinc;

        // Set textual header content
        write_info.textualHeaderContent = "C01 SEGY file created by segyoutput module\n";
        write_info.textualHeaderContent += "C02 Inline range: " + std::to_string(my_data->fpkey) + " - " + std::to_string(my_data->lpkey) + "\n";
        write_info.textualHeaderContent += "C03 Crossline range: " + std::to_string(my_data->fskey) + " - " + std::to_string(my_data->lskey) + "\n";
        write_info.textualHeaderContent += "C04 Sample count: " + std::to_string(my_data->trace_length) + "\n";
        write_info.textualHeaderContent += "C05 Sample interval: " + std::to_string(my_data->sinterval) + " microseconds\n";

        // Calculate total expected traces
        my_data->total_expected_traces = static_cast<int64_t>(my_data->num_pkey) * my_data->num_skey;

        if(!check_offset(my_data->primary_offset, 4, 240)) {
            throw std::runtime_error("Error: segyoutput the offset of attribute " + my_data->pkey_name + " is invalid: " + std::to_string(my_data->primary_offset));
        }

        if(!check_offset(my_data->secondary_offset, 4, 240)) {
            throw std::runtime_error("Error: segyoutput the offset of attribute " + my_data->skey_name + " is invalid: " + std::to_string(my_data->secondary_offset));
        }


        if(!check_offset(my_data->trace_length_offset, 2, 400)) {
            throw std::runtime_error("Error: segyoutput the offset of NumSamples is invalid: " + std::to_string(my_data->trace_length_offset));
        }


        if(!check_offset(my_data->data_format_code_offset, 2, 400)) {
            throw std::runtime_error("Error: segyoutput the offset of DataFormatCode is invalid: " + std::to_string(my_data->data_format_code_offset));
        }
        

        if(!check_offset(my_data->sinterval_offset, 2, 400)) {
            throw std::runtime_error("Error: segyoutput the offset of SampleInterval is invalid: " + std::to_string(my_data->sinterval_offset));
        }

        my_data->segy_writer.addTraceField(my_data->pkey_name, my_data->primary_offset, 4, SEGY::DataSampleFormatCode::Int32);
        my_data->segy_writer.addTraceField(my_data->skey_name, my_data->secondary_offset, 4, SEGY::DataSampleFormatCode::Int32);

        my_data->segy_writer.addBinaryField("NumSamples", my_data->trace_length_offset, 2, SEGY::DataSampleFormatCode::Int16);
        my_data->segy_writer.addBinaryField("SampleInterval", my_data->sinterval_offset, 2, SEGY::DataSampleFormatCode::Int16);
        my_data->segy_writer.addBinaryField("DataFormatCode", my_data->data_format_code_offset, 2, SEGY::DataSampleFormatCode::Int16);

        // add sttributes
        auto& attrs = config["segyoutput"]["attribute"];
        if(attrs.is_array()) {
            //parse attributes
            auto& arr = config["segyoutput"]["attribute"].as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                auto& attr = arr[i];
                std::string name = attr.at("name", "attribute").as_string();
                gutl::UTL_StringToUpperCase(name);

                if(name == my_data->pkey_name || name == my_data->skey_name || name == my_data->trace_name) {
                    //todo log warning
                    continue;
                }

                std::string datatype = attr.at("datatype", "attribute").as_string();
                int offset = attr.at("offset", "attribute").as_int();
                int width;
                SEGY::DataSampleFormatCode format;
                as::DataFormat type;

                if (datatype == "int8") {
                    format = SEGY::DataSampleFormatCode::Int8;
                    type = as::DataFormat::FORMAT_U8;
                    width = 1;
                } else if (datatype == "int16") {
                    format = SEGY::DataSampleFormatCode::Int16;
                    type = as::DataFormat::FORMAT_U16;
                    width = 2;
                } else if (datatype == "int32") {
                    format = SEGY::DataSampleFormatCode::Int32;
                    type = as::DataFormat::FORMAT_U32;
                    width = 4;
                } else if (datatype == "float") {
                    format = SEGY::DataSampleFormatCode::IEEEFloat;
                    type = as::DataFormat::FORMAT_R32;
                    width = 4;
                } else {
                    throw std::runtime_error("Error: segyoutput the datatype of attribute " + name + " is invalid: " + datatype);
                }
                if(!check_offset(offset, width, 240)) {
                    throw std::runtime_error("Error: segyoutput the offset of attribute " + name + " is invalid: " + std::to_string(offset));
                }                
                my_data->segy_writer.addTraceField(name, offset, width, format);
                job_df.AddAttribute(name.c_str(), type, 1);
                job_df.SetAttributeUnit(name.c_str(), "");
            }
        }   

        // Initialize the writer and create output file
        if (!my_data->segy_writer.initialize(my_data->output_url, write_info)) {
            throw std::runtime_error("Error: failed to initialize SEGY writer for file: " + my_data->output_url + 
                                    ", Error: " + my_data->segy_writer.getLastError());
        }
        
        my_data->file_initialized = true;
        my_data->header_written = true;
        
        gd_logger.LogInfo(my_logger, "SEGY writer initialized successfully");
        gd_logger.LogInfo(my_logger, "Expected total traces: {}", my_data->total_expected_traces);
            
        job_df.SetModuleStruct(myid, static_cast<void*>(my_data));

    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
}

void segyoutput_process(const char* myid)
{
    auto& gd_logger = gdlog::GdLogger::GetInstance();
    auto& job_df = df::GeoDataFlow::GetInstance();

    Segyoutput* my_data = static_cast<Segyoutput*>(job_df.GetModuleStruct(myid));
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

    try {        
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

        std::string data_name = job_df.GetVolumeDataName();

        std::ofstream file(my_data->output_url, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Error: write trace, primary: " + my_data->output_url);
        }
        //write data: primary, secondary, trace and attributes
        for(int i = 0; i < job_df.GetNumAttributes(); i++) {

            std::string attr_name = job_df.GetAttributeName(i);

            char *data = static_cast<char*>(job_df.GetWritableBuffer(attr_name.c_str()));
            int bytesize = my_data->segy_writer.getTraceByteSize();
            if(data == nullptr) {
                throw std::runtime_error("DF returned a nullptr to the buffer of attribute: " + attr_name);
            }

            SEGY::HeaderField field = my_data->segy_writer.getTraceField(attr_name);

            if (field.defined()) {
                bytesize =  field.fieldWidth;
            }

            for(int j = my_data->fskey; j <= my_data->lskey; j+=my_data->skinc) {
                if(attr_name == data_name) {
                    if(!my_data->segy_writer.writeTraceData(file, my_data->current_pkey, j, data)) {
                        throw std::runtime_error("Error: write trace, primary: " + std::to_string(i)
                            + ", secondary: "+ std::to_string(j) + ", error:"  + my_data->segy_writer.getErrMsg());
                    }
                } else {
                    if(!my_data->segy_writer.writeTraceHeader(file, my_data->current_pkey, j, data, field.byteLocation, field.fieldWidth)) {
                        throw std::runtime_error("Error: write trace, primary: " + std::to_string(i) 
                            + ", secondary: "+ std::to_string(j) + ", error:"  + my_data->segy_writer.getErrMsg());
                    }
                }
                data += bytesize;
            }
        }
        file.close();      
    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, "Exception in segyoutput_process: {}", e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
    // prepare for the next call
    my_data->current_pkey = my_data->current_pkey + my_data->pkinc;
}