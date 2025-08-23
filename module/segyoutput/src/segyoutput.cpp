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

        //parse run_mode
        std::string run_mode = segyout_config.at("run_mode", "segyoutput").as_string();
        if (run_mode != "dry-run" && run_mode != "actual-run") {
            throw std::runtime_error("Error: segyoutput run_mode is invalid: " + run_mode);
        }
      
        gd_logger.LogInfo(my_logger, "segyoutput run_mode: {}", run_mode);

        if (run_mode == "dry-run") {
            my_data->is_dry_run = true;
            
            // In dry-run mode, just validate configuration and report what would be written
            gd_logger.LogInfo(my_logger, "Dry-run mode: SEGY output file would be created at: {}", my_data->output_url);
            
        } else { // actual-run
            my_data->is_dry_run = false;
            
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
            
            gd_logger.LogInfo(my_logger, "Primary axis: {} to {} ({} values, inc={})", 
                             my_data->fpkey, my_data->lpkey, my_data->num_pkey, my_data->pkinc);
            gd_logger.LogInfo(my_logger, "Secondary axis: {} to {} ({} values, inc={})", 
                             my_data->fskey, my_data->lskey, my_data->num_skey, my_data->skinc);
            gd_logger.LogInfo(my_logger, "Data axis: {} to {} ({} samples, interval={}μs)", 
                             my_data->tmin, my_data->tmax, my_data->trace_length, my_data->sinterval);

            // Parse header field offsets from configuration
            my_data->primary_offset = segyout_config.at("primary_offset", "segyoutput").as_int();
            my_data->secondary_offset = segyout_config.at("secondary_offset", "segyoutput").as_int();
            my_data->sinterval_offset = segyout_config.at("sinterval_offset", "segyoutput").as_int();
            my_data->trace_length_offset = segyout_config.at("trace_length_offset", "segyoutput").as_int();
            my_data->data_format_code_offset = segyout_config.at("data_format_code_offset", "segyoutput").as_int();

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

            // Configure SEGY write info
            my_data->write_info.headerEndianness = SEGY::Endianness::Big; // Standard SEGY big-endian
            my_data->write_info.dataSampleFormatCode = segy_format;
            my_data->write_info.sampleCount = my_data->trace_length;
            my_data->write_info.sampleInterval = my_data->sinterval;
            
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
            my_data->write_info.traceByteSize = my_data->trace_length * sample_size;
            
            // Set coordinate system
            my_data->write_info.minInline = my_data->fpkey;
            my_data->write_info.maxInline = my_data->lpkey;
            my_data->write_info.inlineCount = my_data->num_pkey;
            my_data->write_info.minCrossline = my_data->fskey;
            my_data->write_info.maxCrossline = my_data->lskey;
            my_data->write_info.crosslineCount = my_data->num_skey;
            my_data->write_info.primaryStep = my_data->pkinc;
            my_data->write_info.secondaryStep = my_data->skinc;
            my_data->write_info.isPrimaryInline = true; // Assume inline is primary
            
            // Set header field locations
            my_data->write_info.primaryKey = {my_data->primary_offset, 4, true};
            my_data->write_info.secondaryKey = {my_data->secondary_offset, 4, true};
            my_data->write_info.numSamplesKey = {my_data->trace_length_offset, 2, true};
            my_data->write_info.sampleIntervalKey = {my_data->sinterval_offset, 2, true};
            my_data->write_info.dataSampleFormatCodeKey = {my_data->data_format_code_offset, 2, true};

            // Set textual header content
            my_data->write_info.textualHeaderContent = "C01 SEGY file created by segyoutput module\n";
            my_data->write_info.textualHeaderContent += "C02 Inline range: " + std::to_string(my_data->fpkey) + " - " + std::to_string(my_data->lpkey) + "\n";
            my_data->write_info.textualHeaderContent += "C03 Crossline range: " + std::to_string(my_data->fskey) + " - " + std::to_string(my_data->lskey) + "\n";
            my_data->write_info.textualHeaderContent += "C04 Sample count: " + std::to_string(my_data->trace_length) + "\n";
            my_data->write_info.textualHeaderContent += "C05 Sample interval: " + std::to_string(my_data->sinterval) + " microseconds\n";

            // Calculate total expected traces
            my_data->total_expected_traces = static_cast<int64_t>(my_data->num_pkey) * my_data->num_skey;
            
            // Create SEGY writer
            my_data->segy_writer = new SEGYWriter(my_data->write_info);
            
            // Initialize the writer and create output file
            if (!my_data->segy_writer->initialize(my_data->output_url)) {
                throw std::runtime_error("Error: failed to initialize SEGY writer for file: " + my_data->output_url + 
                                       ", Error: " + my_data->segy_writer->getLastError());
            }
            
            my_data->file_initialized = true;
            my_data->header_written = true;
            my_data->current_pkey = my_data->fpkey;
            
            gd_logger.LogInfo(my_logger, "SEGY writer initialized successfully");
            gd_logger.LogInfo(my_logger, "Expected total traces: {}", my_data->total_expected_traces);
            gd_logger.LogInfo(my_logger, "Expected file size: {} bytes", my_data->segy_writer->getExpectedFileSize());
        }
            
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

    if(my_data->is_dry_run) {
        // In dry-run mode, just log what would be processed and finish
        gd_logger.LogInfo(my_logger, "Dry-run: would process data and write to SEGY file");
        job_df.SetJobFinished();
        return;
    }

    try {
        // Get current data from GeoDataFlow
        int grp_size = job_df.GetGroupSize();
        
        // Get primary and secondary key data
        const int* pkey = static_cast<const int*>(job_df.GetWritableBuffer(job_df.GetPrimaryKeyName()));
        if(pkey == nullptr) {
            throw std::runtime_error("Error: DF returned nullptr for primary key buffer");
        }
        
        const int* skey = static_cast<const int*>(job_df.GetWritableBuffer(job_df.GetSecondaryKeyName()));
        if(skey == nullptr) {
            throw std::runtime_error("Error: DF returned nullptr for secondary key buffer");
        }
        
        // Get trace data
        const void* trace_data = job_df.GetWritableBuffer(job_df.GetVolumeDataName());
        if(trace_data == nullptr) {
            throw std::runtime_error("Error: DF returned nullptr for trace data buffer");
        }
        
        gd_logger.LogInfo(my_logger, "Processing primary key {}, group size: {}", pkey[0], grp_size);
        
        // Calculate trace data size per trace
        int trace_byte_size = my_data->write_info.traceByteSize;
        const char* trace_data_ptr = static_cast<const char*>(trace_data);
        
        // Write traces for current primary key
        for(int i = 0; i < grp_size; i++) {
            int current_inline = pkey[i];
            int current_crossline = skey[i];
            
            // Calculate offset for current trace data
            const void* current_trace_data = trace_data_ptr + (i * trace_byte_size);
            
            // Create custom headers map for additional attributes
            std::map<std::string, int> custom_headers;
            
            // Process all attributes to find additional header fields
            for(int attr_idx = 0; attr_idx < job_df.GetNumAttributes(); attr_idx++) {
                std::string attr_name = job_df.GetAttributeName(attr_idx);
                
                // Skip primary key, secondary key, and trace data
                if(attr_name == my_data->pkey_name || 
                   attr_name == my_data->skey_name || 
                   attr_name == my_data->trace_name) {
                    continue;
                }
                
                // Get attribute data for custom headers
                const void* attr_data = job_df.GetWritableBuffer(attr_name.c_str());
                if(attr_data != nullptr) {
                    // Assume attribute is integer type for SEGY headers
                    const int* attr_int_data = static_cast<const int*>(attr_data);
                    custom_headers[attr_name] = attr_int_data[i];
                }
            }
            
            // Write trace to SEGY file
            if(!my_data->segy_writer->addTrace(current_inline, current_crossline, 
                                              current_trace_data, custom_headers)) {
                throw std::runtime_error("Error: failed to write trace (" + 
                                       std::to_string(current_inline) + ", " + 
                                       std::to_string(current_crossline) + "): " + 
                                       my_data->segy_writer->getLastError());
            }
            
            my_data->traces_written++;
            
            // Log progress periodically
            if(my_data->traces_written % 1000 == 0) {
                double progress = (double)my_data->traces_written / my_data->total_expected_traces * 100.0;
                gd_logger.LogInfo(my_logger, "Progress: {}/{} traces written ({:.1f}%)", 
                                 my_data->traces_written, my_data->total_expected_traces, progress);
            }
        }
        
        // Flush traces to ensure they are written to disk
        if(!my_data->segy_writer->flushTraces()) {
            throw std::runtime_error("Error: failed to flush traces to disk");
        }
        
        // Check if we've written all expected traces
        if(my_data->traces_written >= my_data->total_expected_traces) {
            gd_logger.LogInfo(my_logger, "All traces written successfully: {}/{}", 
                             my_data->traces_written, my_data->total_expected_traces);
            
            // Finalize the SEGY file
            if(!my_data->segy_writer->finalize()) {
                throw std::runtime_error("Error: failed to finalize SEGY file: " + 
                                       my_data->segy_writer->getLastError());
            }
            
            gd_logger.LogInfo(my_logger, "SEGY file finalized successfully: {}", my_data->output_url);
            job_df.SetJobFinished();
        }
        
    } catch (const std::exception& e) {
        gd_logger.LogError(my_logger, "Exception in segyoutput_process: {}", e.what());
        job_df.SetJobAborted();
        _clean_up();
        return;
    }
}