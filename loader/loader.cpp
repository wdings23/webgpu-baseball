#include <loader/loader.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>
#include <curl/curl.h>
#else
#include <curl/curl.h>
#endif // __EMSCRIPTEN__

#include <assert.h>

#include <tinyexr/miniz.h>
#include <utils/LogPrint.h>

#include <filesystem>

#include <map>

#if defined(_DEBUG)
static std::map<std::string, uint32_t> saLoadFileNames;
#endif // _DEBUG

#if defined(__EMSCRIPTEN__)
#define ZIP_ARCHIVE_FILE_PATH "assets/total-assets.zip"
#else 
#define ZIP_ARCHIVE_FILE_PATH "D:\\test\\github-projects\\test_assets\\total-assets.zip"
#endif // __EMSCRIPTEN__

namespace Loader
{
    // Callback function to write data to file
    //size_t writeData(void* ptr, size_t size, size_t nmemb, void* pData) {
    //    size_t iTotalSize = size * nmemb;
    //    std::vector<char>* pBuffer = (std::vector<char>*)pData;
    //    uint32_t iPrevSize = (uint32_t)pBuffer->size();
    //    pBuffer->resize(pBuffer->size() + iTotalSize);
    //    char* pBufferEnd = pBuffer->data();
    //    pBufferEnd += iPrevSize;
    //    memcpy(pBufferEnd, ptr, iTotalSize);
    //
    //    return iTotalSize;
    //}

    struct MemoryInfo
    {
        char* macBuffer;
        bool mbText;
        uint32_t miCurrSize;
    };

    // Callback function to write data to file
    size_t writeData(void* ptr, size_t size, size_t nmemb, void* pData) 
    {
        MemoryInfo* pMemoryInfo = (MemoryInfo*)pData;

        size_t iTotalSize = size * nmemb;
        uint32_t iPrevSize = (uint32_t)pMemoryInfo->miCurrSize;
        if(pMemoryInfo->macBuffer == nullptr)
        {
            pMemoryInfo->macBuffer = (char*)malloc(iTotalSize + 1);
        }
        else
        {
            pMemoryInfo->macBuffer = (char*)realloc(pMemoryInfo->macBuffer, iPrevSize + iTotalSize + 1);
        }

        char* pBufferEnd = pMemoryInfo->macBuffer;
        pBufferEnd += iPrevSize;
        memcpy(pBufferEnd, ptr, iTotalSize);
        pMemoryInfo->miCurrSize = (uint32_t)(iPrevSize + iTotalSize);
        *(pMemoryInfo->macBuffer + pMemoryInfo->miCurrSize) = 0;

        return iTotalSize;
    }


    bool bDoneLoading = false;
    char* gacTempMemory = nullptr;
    uint32_t giTempMemorySize = 0;
    MemoryInfo gMemoryInfo;

#if defined(__EMSCRIPTEN__)
    emscripten_fetch_t* gpFetch = nullptr;

    /*
    **
    */
    void downloadSucceeded(emscripten_fetch_t* fetch)
    {
        DEBUG_PRINTF("received %llu bytes\n", fetch->numBytes);
        //gpFetch = fetch;
        gacTempMemory = (char*)malloc(fetch->numBytes + 1);
        memcpy(gacTempMemory, fetch->data, fetch->numBytes);
        *(gacTempMemory + fetch->numBytes) = 0;
        giTempMemorySize = fetch->numBytes;
        DEBUG_PRINTF("setting %d bytes\n", giTempMemorySize);

        emscripten_fetch_close(fetch);

        bDoneLoading = true;
    }

    /*
    **
    */
    void downloadFailed(emscripten_fetch_t* fetch)
    {
        DEBUG_PRINTF("!!! error fetching data !!!\n");
        emscripten_fetch_close(fetch);

        giTempMemorySize = 0;

        bDoneLoading = true;
    }
#endif // __EMSCRIPTEN__

#if defined(__EMSCRIPTEN__)

#if defined(EMBEDDED_FILES)
    uint32_t loadFile(
        char** pacFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        DEBUG_PRINTF("load %s\n", filePath.c_str());

        DEBUG_PRINTF("zip file archive %s\n", ZIP_ARCHIVE_FILE_PATH);

        // check if the total asset zip file exists
        FILE* fp = fopen(ZIP_ARCHIVE_FILE_PATH, "rb");
        if(fp)
        {
            // read in zip file into memory
            fseek(fp, 0, SEEK_END);
            size_t iFileSize = (uint32_t)ftell(fp) + 1;
            fseek(fp, 0, SEEK_SET);
            char* acFileContent = (char*)malloc(iFileSize);
            fread(acFileContent, sizeof(char), iFileSize, fp);
            fclose(fp);
            DEBUG_PRINTF("%s : %d zip file to extract: %s\n",
                __FILE__,
                __LINE__,
                ZIP_ARCHIVE_FILE_PATH);
            mz_zip_archive zipArchive;
            memset(&zipArchive, 0, sizeof(zipArchive));

            // init from memory
            mz_bool status = mz_zip_reader_init_mem(&zipArchive, acFileContent, iFileSize, 0);
            if(!status)
            {
                DEBUG_PRINTF("%s : %d status = %d\n",
                    __FILE__,
                    __LINE__,
                    status);
                assert(0);
            }

            // locate file in the zip archive
            int32_t iFileIndex = mz_zip_reader_locate_file(&zipArchive, filePath.c_str(), nullptr, 0);
            if(iFileIndex == -1)
            {
                DEBUG_PRINTF("%s : %d can\'t find file \"%s\"\n",
                    __FILE__,
                    __LINE__,
                    filePath.c_str());
                assert(0);
            }
            DEBUG_PRINTF("%s : %d file index = %d\n",
                __FILE__,
                __LINE__,
                iFileIndex);

            // extract to memory
            size_t iUncompressedSize;
            void* buffer = mz_zip_reader_extract_file_to_heap(&zipArchive, filePath.c_str(), &iUncompressedSize, 0);
            if(buffer == nullptr)
            {
                DEBUG_PRINTF("%s : %d can\'t uncompress file \"%s\"\n",
                    __FILE__,
                    __LINE__,
                    filePath.c_str());
                assert(0);
            }
            free(acFileContent);
            gMemoryInfo.macBuffer = (char*)malloc(iUncompressedSize + 1);
            memcpy(gMemoryInfo.macBuffer, buffer, iUncompressedSize);
            gMemoryInfo.macBuffer[iUncompressedSize] = '\0';
            gMemoryInfo.miCurrSize = (uint32_t)iUncompressedSize;

            mz_free(buffer);
            mz_zip_reader_end(&zipArchive);

            gMemoryInfo.miCurrSize = (uint32_t)iUncompressedSize;

            DEBUG_PRINTF("%s : %d \"%s\" uncompressed size: %d\n",
                __FILE__,
                __LINE__,
                filePath.c_str(),
                (uint32_t)iUncompressedSize);

        }
        
        *pacFileContentBuffer = gMemoryInfo.macBuffer;

#if defined(_DEBUG)
        saLoadFileNames[filePath] = gMemoryInfo.miCurrSize;
#endif // _DEBUG

        return (uint32_t)gMemoryInfo.miCurrSize;
    }
#endif // EMBEDDED_FILES

    void loadFileFree(void* pData)
    {
        //emscripten_fetch_close(gpFetch);
        free(gacTempMemory);
    }

#else 

    /*
    **
    */
    void loadFile(
        std::vector<char>& acFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        std::string url = "http://127.0.0.1:8000/" + filePath;

        CURL* curl;
        CURLcode res;

        curl = curl_easy_init();
        if(curl)
        {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &acFileContentBuffer);

            res = curl_easy_perform(curl);

            if(acFileContentBuffer.size() <= 0)
            {
                url = "http://127.0.0.1:8080/" + filePath;
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                res = curl_easy_perform(curl);
            }
            else
            {
                int iDebug = 1;
            }

            curl_easy_cleanup(curl);
        }

        if(bTextFile)
        {
            acFileContentBuffer.push_back(0);
        }
    }

    /*
    **
    */
    uint32_t loadFile(
        char** pacFileContentBuffer,
        std::string const& filePath,
        bool bTextFile)
    {
        DEBUG_PRINTF("load %s\n", filePath.c_str());

        // check if the total asset zip file exists
        FILE* fp = fopen(ZIP_ARCHIVE_FILE_PATH, "rb");
        if(fp)
        {
            // read in zip file into memory
            fseek(fp, 0, SEEK_END);
            size_t iFileSize = (uint32_t)ftell(fp) + 1;
            fseek(fp, 0, SEEK_SET);
            char* acFileContent = (char*)malloc(iFileSize);
            fread(acFileContent, sizeof(char), iFileSize, fp);
            fclose(fp);
            DEBUG_PRINTF("%s : %d zip file to extract: %s\n",
                __FILE__,
                __LINE__,
                ZIP_ARCHIVE_FILE_PATH);
            mz_zip_archive zipArchive;
            memset(&zipArchive, 0, sizeof(zipArchive));

            // init from memory
            mz_bool status = mz_zip_reader_init_mem(&zipArchive, acFileContent, iFileSize, 0);
            if(!status)
            {
                DEBUG_PRINTF("%s : %d status = %d\n",
                    __FILE__,
                    __LINE__,
                    status);
                assert(0);
            }

            // locate file in the zip archive
            int32_t iFileIndex = mz_zip_reader_locate_file(&zipArchive, filePath.c_str(), nullptr, 0);
            if(iFileIndex == -1)
            {
                DEBUG_PRINTF("%s : %d can\'t find file \"%s\"\n",
                    __FILE__,
                    __LINE__,
                    filePath.c_str());
                assert(0);
            }
            DEBUG_PRINTF("%s : %d file index = %d\n",
                __FILE__,
                __LINE__,
                iFileIndex);

            // extract to memory
            size_t iUncompressedSize;
            void* buffer = mz_zip_reader_extract_file_to_heap(&zipArchive, filePath.c_str(), &iUncompressedSize, 0);
            if(buffer == nullptr)
            {
                DEBUG_PRINTF("%s : %d can\'t uncompress file \"%s\"\n",
                    __FILE__,
                    __LINE__,
                    filePath.c_str());
                assert(0);
            }
            free(acFileContent);
            gMemoryInfo.macBuffer = (char*)malloc(iUncompressedSize + 1);
            memcpy(gMemoryInfo.macBuffer, buffer, iUncompressedSize);
            gMemoryInfo.macBuffer[iUncompressedSize] = '\0';
            gMemoryInfo.miCurrSize = (uint32_t)iUncompressedSize;

            mz_free(buffer);
            mz_zip_reader_end(&zipArchive);

            gMemoryInfo.miCurrSize = (uint32_t)iUncompressedSize;

            DEBUG_PRINTF("%s : %d \"%s\" uncompressed size: %d\n",
                __FILE__,
                __LINE__,
                filePath.c_str(),
                (uint32_t)iUncompressedSize);
            
        }
        else
        {
            // download 
            std::string url = "http://127.0.0.1:8000/" + filePath;

            CURL* curl;
            CURLcode res;

            curl = curl_easy_init();
            if(curl)
            {
                assert(gMemoryInfo.macBuffer == nullptr);

                gMemoryInfo.mbText = bTextFile;
                
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &gMemoryInfo);

                res = curl_easy_perform(curl);

                if(gacTempMemory == nullptr)
                {
                    url = "http://127.0.0.1:8080/" + filePath;
                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    res = curl_easy_perform(curl);
                }
                else
                {
                    int iDebug = 1;
                }

                curl_easy_cleanup(curl);
            }
        }

        *pacFileContentBuffer = gMemoryInfo.macBuffer;

#if defined(_DEBUG)
        saLoadFileNames[filePath] = gMemoryInfo.miCurrSize;
#endif // _DEBUG

        return (uint32_t)gMemoryInfo.miCurrSize;
    }

    void loadFileFree(void* pData)
    {
        //emscripten_fetch_close(gpFetch);
        //free(gacTempMemory);
        free(gMemoryInfo.macBuffer);
        gMemoryInfo.macBuffer = nullptr;
        gMemoryInfo.miCurrSize = 0;
    }

#endif // __EMSCRIPTEN__

#if defined(_DEBUG)
    uint64_t compressFiles(std::string const& outputFilePath)
    {
        auto dirEnd = outputFilePath.rfind("\\");
        if(dirEnd == std::string::npos)
        {
            dirEnd = outputFilePath.rfind("/");
        }
        std::string dir = outputFilePath.substr(0, dirEnd);

        mz_zip_archive  zipArchive;
        memset(&zipArchive, 0, sizeof(zipArchive));
        mz_zip_writer_init_file(&zipArchive, outputFilePath.c_str(), 0);

        uint64_t iTotalCompressedSize = 0;
        FILE* outputFileP = fopen(outputFilePath.c_str(), "wb");
        for(auto const& keyValue : saLoadFileNames)
        {
            std::string fullPath = dir + "/" + keyValue.first;
            mz_bool bSuccess = mz_zip_writer_add_file(
                &zipArchive,
                keyValue.first.c_str(),
                fullPath.c_str(),
                nullptr,
                0,
                MZ_DEFAULT_COMPRESSION
            );

            if(!bSuccess)
            {
                std::filesystem::path cwd = std::filesystem::current_path();
                std::string newDir = cwd.string();
                fullPath = newDir + keyValue.first;
                bSuccess = mz_zip_writer_add_file(
                    &zipArchive,
                    keyValue.first.c_str(),
                    fullPath.c_str(),
                    nullptr,
                    0,
                    MZ_DEFAULT_COMPRESSION
                );

                if(!bSuccess)
                {
                    fullPath = newDir + "/../" + keyValue.first;
                    bSuccess = mz_zip_writer_add_file(
                        &zipArchive,
                        keyValue.first.c_str(),
                        fullPath.c_str(),
                        nullptr,
                        0,
                        MZ_DEFAULT_COMPRESSION
                    );
                }
            }

            assert(bSuccess);

        }
        mz_zip_writer_finalize_archive(&zipArchive);
        mz_zip_writer_end(&zipArchive);

        return zipArchive.m_archive_size;
    }

#endif // _DEBUG

}

