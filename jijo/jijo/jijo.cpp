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
#include <chrono>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <iterator>

#include <fstream>

namespace fs = std::experimental::filesystem;
using namespace std;
using namespace std::chrono;

vector<char> gen_random(size_t len) 
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    vector<char> v(len);
    for (int i = 0; i < len; ++i) {
        v[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return v;
}

string to_string(const vector<char>& v)
{
    return string(v.begin(), v.end());
}

struct Config
{
    Config(bool is_help=false)
        :is_help(is_help)
    {}

    bool is_help;
    size_t chunk, depth, dir_count, file_count, name_length, file_length;
    string target_dir, file_ext;

    bool Help() const
    {
        return is_help;
    }
};

Config Get_config(int ac, char* av[])
{
    Config cfg;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Directories and files generator.")
        ("chunk", po::value<size_t>(&cfg.chunk)->default_value(1048),
            "Chunk size.")
        ("depth", po::value<size_t>(&cfg.depth)->default_value(10),
            "Directories depth.")
        ("dir_count", po::value<size_t>(&cfg.dir_count)->default_value(10),
            "Directories count per depth level.")
        ("file_count", po::value<size_t>(&cfg.file_count)->default_value(10),
            "Files count count per directory.")
        ("target", po::value<string>(&cfg.target_dir)->default_value("."),
            "Generate in that directory, default is current, if not exist create.")
        ("name_length", po::value<size_t>(&cfg.name_length)->default_value(5),
            "Directory/file name length.")
        ("file_length", po::value<size_t>(&cfg.file_length)->default_value(1049),
            "File length.")
        ("file_ext", po::value<string>(&cfg.file_ext)->default_value("rdm"),
            "File extension.")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        return Config(true);
    }

    return cfg;
}

int main(int ac, char* av[])
{
    try {

        const Config cfg = Get_config(ac, av);
        if (cfg.Help())
            return 0;

        const std::chrono::system_clock::time_point start = std::chrono::system_clock::now();

        size_t dir_count = 0;
        size_t files_count = 0;

        fs::path target_dir = cfg.target_dir;
        std::error_code ec;
        if (!exists(target_dir, ec))
        {
            fs::create_directory(target_dir);
            dir_count++;
        }

        for (size_t h = 0; h < cfg.dir_count; h++)
            for (size_t v = 0; v < cfg.depth; v++)
            {
                fs::path new_dir = target_dir / to_string(gen_random(cfg.name_length));
                std::error_code ec;
                size_t count = 0;
                while (exists(new_dir, ec))
                {
                    if (++count > 10)
                    {
                        cerr << "Unable to generate a new directory name (" << new_dir << ")" << endl;
                        return 1;
                    }
                    new_dir = target_dir / to_string(gen_random(cfg.name_length));
                }
                fs::create_directory(new_dir);
                dir_count++;
                for (size_t f = 0; f < cfg.file_count; f++)
                {
                    fs::path new_file = new_dir / to_string(gen_random(cfg.name_length));
                    std::error_code ec;
                    size_t count = 0;
                    while (exists(new_file, ec))
                    {
                        if (++count > 10)
                        {
                            cerr << "Unable to generate a new file name (" << new_file << ")" << endl;
                            return 1;
                        }
                        new_file = target_dir / to_string(gen_random(cfg.name_length));
                    }
                    std::ofstream file(new_file);
                    files_count++;

                    size_t i = 0;
                    for(; i < cfg.file_length % cfg.chunk; i++)
                    { 
                        vector<char> v = gen_random(cfg.chunk);
                        file.write(&v[0], v.size());
                    }
                    vector<char> v = gen_random( cfg.chunk > cfg.file_length ? cfg.file_length : (cfg.file_length - i * cfg.chunk));
                    if(!v.empty())
                        file.write(&v[0], v.size());
                }
            }

        auto i = 10 % 60;
        auto j = 75 % 60;
            const auto elapsed = duration_cast<seconds>(system_clock::now() -start).count();
        auto minutes = elapsed % 60;
        auto secs = elapsed - 60 * minutes;
        cout << files_count << " files in " << dir_count << " directories in " <<
            minutes << "mns " << secs << " secs" << endl;
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
