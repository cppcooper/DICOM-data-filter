#include <core.h>
#include <dicom-file.h>
#include <db-interface.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

OrthancPluginErrorCode WriteDicomFile(DicomFile dicom, const char *uuid){
    const fs::path storage_root(globals::storage_location);
    char* content = nullptr;
    size_t size = 0;
    bool cleanup = false;
    if(dicom.IsValid()) {
        DBInterface::HandlePHI(dicom);
        auto filtered = dicom.ApplyFilter(globals::filter_list);
        if (std::get<0>(filtered)) {
            content = std::get<0>(filtered);
            size = std::get<1>(filtered);
            if(size == 0){
                // todo: probably a better error code
                return OrthancPluginErrorCode_EmptyRequest;
            }
            cleanup = true;
        }
        // write to disk
        // todo: add uuid directory portion
        fs::path master_path = fs::path(storage_root)
                .append("/by-uuid/")
                .append(uuid)
                .append(".DCM");
        fs::create_directories(master_path);
        std::fstream file(master_path, std::ios::binary | std::ios::out);
        file.write(content,size);
        file.close();
        // set file permissions
        // todo: permission debug info?
        fs::file_status master_status = fs::status(master_path);
        master_status.permissions(globals::file_permissions);

        // create hard links
        auto hardlink_to = [&](std::string group, std::string folder) {
            fs::path link = fs::path(storage_root)
                    .append(group)
                    .append(folder)
                    .append(uuid)
                    .append(".DCM");
            fs::create_directories(link);
            fs::create_hard_link(master_path, link);
            fs::permissions(link, globals::file_permissions);
        };
        // todo: integrate json settings to enable/disable individual hard links
        // todo: replace placeholders
        std::string DOB_placeholder;
        std::string PID_placeholder;
        std::string SD_placeholder;
        hardlink_to("/by-dob/", DOB_placeholder);
        hardlink_to("/by-patient-id/", PID_placeholder);
        hardlink_to("/by-study-date/", SD_placeholder);
        if(cleanup){
            delete[] content;
        }
    }
    return OrthancPluginErrorCode_BadFileFormat;
}

OrthancPluginErrorCode StorageCreateCallback(const char *uuid,
                                             const void *content,
                                             int64_t size,
                                             OrthancPluginContentType type) {
    const fs::path storage_root(globals::storage_location);
    fs::path path;
    switch (type) {
        case OrthancPluginContentType_Dicom:
            return WriteDicomFile(DicomFile(content, size), uuid);
        case OrthancPluginContentType_DicomAsJson:
            //todo: implement DicomFile json parser? or write a json file to disk?
            path = fs::path(storage_root)
                    .append("/json/")
                    .append(uuid)
                    .append(".json");
            break;
        case OrthancPluginContentType_DicomUntilPixelData:
            // todo: will the plugin need to handle this? what does it look like and its use case?
            path = fs::path(storage_root)
                    .append("/no-pixel/")
                    .append(uuid)
                    .append(".DCM");
            break;
        case _OrthancPluginContentType_INTERNAL:
            // todo: anything? what even is this?
            path = fs::path(storage_root)
                    .append("/internal/")
                    .append(uuid);
            break;
        case OrthancPluginContentType_Unknown:
            //todo: write plain jane fstream write?
            path = fs::path(storage_root)
                    .append("/unknown-files/")
                    .append(uuid);
            break;
    }
    std::fstream file(path, std::ios::out | std::ios::binary);
    if (file.is_open()) {
        file.write((const char *) content, size);
        if (file.good()) {
            file.close();
            return OrthancPluginErrorCode_Success;
        }
        OrthancPluginLogWarning(globals::context, "StorageCreateCallback: but write out appears bad");
    }
    return OrthancPluginErrorCode_FileStorageCannotWrite;
}

OrthancPluginErrorCode StorageReadWholeCallback(OrthancPluginMemoryBuffer64 *target,
                                                const char *uuid,
                                                OrthancPluginContentType type) {
    const fs::path storage_root(globals::storage_location);
    fs::path path;
    switch(type){
        case OrthancPluginContentType_Dicom:
            path = fs::path(storage_root)
                    .append("/by-uuid/")
                    .append(uuid)
                    .append(".DCM");
            break;
        case OrthancPluginContentType_DicomAsJson:
            path = fs::path(storage_root)
                    .append("/json/")
                    .append(uuid)
                    .append(".json");
            break;
        case OrthancPluginContentType_DicomUntilPixelData:
            path = fs::path(storage_root)
                    .append("/no-pixel/")
                    .append(uuid)
                    .append(".DCM");
            break;
        case _OrthancPluginContentType_INTERNAL:
            path = fs::path(storage_root)
                    .append("/internal/")
                    .append(uuid);
            break;
        case OrthancPluginContentType_Unknown:
            path = fs::path(storage_root)
                    .append("/unknown-files/")
                    .append(uuid);
            break;
    }
    // todo: is the buffer ready? without this call..
    OrthancPluginCreateMemoryBuffer64(globals::context, target, fs::file_size(path));
    std::fstream file(path, std::ios::in | std::ios::binary);
    if(file.is_open()){
        file.read((char*)target->data, target->size);
        if(file.good()){
            file.close();
            return OrthancPluginErrorCode_Success;
        }
        OrthancPluginLogWarning(globals::context, "StorageReadWholeCallback: opened file, but couldn't read it");
        return OrthancPluginErrorCode_StorageAreaPlugin;
    }
    return OrthancPluginErrorCode_InexistentFile;
}

OrthancPluginErrorCode StorageReadRangeCallback(OrthancPluginMemoryBuffer64 *target,
                                                const char *uuid,
                                                OrthancPluginContentType type,
                                                uint64_t rangeStart) {
    const fs::path storage_root(globals::storage_location);
    fs::path path;
    switch(type){
        case OrthancPluginContentType_Dicom:
            path = fs::path(storage_root)
                    .append("/by-uuid/")
                    .append(uuid)
                    .append(".DCM");
            break;
        case OrthancPluginContentType_DicomAsJson:
            path = fs::path(storage_root)
                    .append("/json/")
                    .append(uuid)
                    .append(".json");
            break;
        case OrthancPluginContentType_DicomUntilPixelData:
            path = fs::path(storage_root)
                    .append("/no-pixel/")
                    .append(uuid)
                    .append(".DCM");
            break;
        case _OrthancPluginContentType_INTERNAL:
            path = fs::path(storage_root)
                    .append("/internal/")
                    .append(uuid);
            break;
        case OrthancPluginContentType_Unknown:
            path = fs::path(storage_root)
                    .append("/unknown-files/")
                    .append(uuid);
            break;
    }
    // todo: is the buffer ready? without this call..
    OrthancPluginCreateMemoryBuffer64(globals::context, target, fs::file_size(path));
    std::fstream file(path, std::ios::in | std::ios::binary);
    if(file.is_open()){
        /*
        file.read((char*)target->data, target->size);
        if(file.good()){
            file.close();
            return OrthancPluginErrorCode_Success;
        }
        OrthancPluginLogWarning(globals::context, "StorageReadWholeCallback: opened file, but couldn't read it");
        return OrthancPluginErrorCode_StorageAreaPlugin;
        /**/
    }
    return OrthancPluginErrorCode_InexistentFile;
}

OrthancPluginErrorCode StorageRemoveCallback(const char *uuid, OrthancPluginContentType type) {
    return OrthancPluginErrorCode_InexistentFile;
}
