// Including SDKDDKVer.h defines the highest available Windows platform.

// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.

// boost::program_options must be built:
// C:\dev\boost_1_65_1>bootstrap.bat
// C:\dev\boost_1_65_1>bjam.exe --build-type=complete msvc stage --with-program_options address-model=64
#include <SDKDDKVer.h>
#include <stdio.h>
#include <tchar.h>

#include <filesystem>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <iterator>

#include <fstream>

namespace fs = std::experimental::filesystem;
using namespace std;

string gen_random(const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    string s;
    s.reserve(len);
    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return s;
}

int main(int ac, char* av[])
{
    try {

        size_t chunk, depth, dir_count, file_count, name_length;
        string target_dir;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "Directories and files generator.")
            ("chunk", po::value<size_t>(&chunk)->default_value(1048), "Chunk size.")
            ("depth", po::value<size_t>(&depth)->default_value(10), "Directories depth.")
            ("dir_count", po::value<size_t>(&dir_count)->default_value(10), "Directories count per depth level.")
            ("file_count", po::value<size_t>(&file_count)->default_value(10), "Files count count per directory.")
            ("target", po::value<string>(&target_dir)->default_value("."), "Generate in that directory, default is current.")
            ("name_length", po::value<size_t>(&name_length)->default_value(5), "Directory/file name length.")
            ;

        po::variables_map vm;
        po::store(po::parse_command_line(ac, av, desc), vm);
        po::notify(vm);

        if (vm.count("help")) {
            cout << desc << "\n";
            return 0;
        }

        fs::directory_entry root(fs::path(target_dir));
        for (size_t h = 0; h < dir_count; h++)
            h_path
            for (size_t v = 0; v < depth; v++)
            {
                string name = gen_random(name_length);
                fs::path = current
                bool exists(const std::filesystem::path& p, std::error_code& ec)
                fs::create_directory(name);
                for (size_t f = 0; f < file_count; f++)
                {

                }

            }

        for (auto& p : fs::recursive_directory_iterator(target_dir))
            std::cout << p << '\n';


    }
    catch (exception& e) {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        cerr << "Exception of unknown type!\n";
    }

    return 0;
}
