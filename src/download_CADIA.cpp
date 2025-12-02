#include "download_CADIA.h"

#include <iostream>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <curl/curl.h>
#include <string>

// ----------------------------------------------------------
// Build "YYYYMM01" string
// ----------------------------------------------------------
static std::string make_date(int year, int month) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d%02d01", year, month);
    return std::string(buf);
}

// ----------------------------------------------------------
// libcurl write callback for downloading file bodies
// ----------------------------------------------------------
static size_t write_data(void* ptr, size_t size, size_t nmemb, void* userdata) {
    FILE* stream = static_cast<FILE*>(userdata);
    return std::fwrite(ptr, size, nmemb, stream);
}

// ----------------------------------------------------------
// HEAD request to check if URL exists (HTTP 200)
// ----------------------------------------------------------
static bool url_exists(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && code == 200);
}

// ----------------------------------------------------------
// download_caida - main routine
// ----------------------------------------------------------
std::string download_caida(int max_months_back) {
    // Get current time
    time_t t = time(nullptr);
    tm* now = localtime(&t);

    int year  = now->tm_year + 1900;
    int month = now->tm_mon + 1; // 1-12 current month

    // Start from LAST month
    month -= 1;
    if (month == 0) {
        month = 12;
        year -= 1;
    }

    for (int i = 0; i < max_months_back; ++i) {
        // Build date for this iteration
        std::string date = make_date(year, month);
        std::string filename = date + ".as-rel2.txt.bz2";

        // 1) Check local file first
        if (std::filesystem::exists(filename)) {
            std::cout << "[CAIDA] Using local file: " << filename << "\n";
            return filename;
        }

        // 2) If not local, check if URL exists
        std::string url =
            "https://publicdata.caida.org/datasets/as-relationships/serial-2/" +
            filename;

        std::cout << "[CAIDA] Checking URL: " << url << " ... ";

        if (url_exists(url)) {
            std::cout << "FOUND, downloading...\n";

            CURL* curl = curl_easy_init();
            if (!curl) {
                std::cerr << "[CAIDA] curl_easy_init() failed.\n";
                return "";
            }

            FILE* fp = std::fopen(filename.c_str(), "wb");
            if (!fp) {
                std::cerr << "[CAIDA] Failed to open output file: "
                          << filename << "\n";
                curl_easy_cleanup(curl);
                return "";
            }

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            CURLcode res = curl_easy_perform(curl);

            std::fclose(fp);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                std::cerr << "[CAIDA] Download failed: "
                          << curl_easy_strerror(res) << "\n";
                std::filesystem::remove(filename);
                return "";
            }

            std::cout << "[CAIDA] Downloaded successfully: "
                      << filename << "\n";
            return filename;
        } else {
            std::cout << "not found.\n";
        }

        // 3) Go back one more month
        month -= 1;
        if (month == 0) {
            month = 12;
            year -= 1;
        }
    }

    std::cerr << "[CAIDA] No dataset found or downloaded within "
              << max_months_back << " months.\n";
    return "";
}
