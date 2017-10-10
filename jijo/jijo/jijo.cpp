#include <chrono>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <iostream>
#include <fstream>
#include <iomanip>

#include <filesystem>
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
        ("help,h", "Directories and files generator.")
        ("chunk", po::value<size_t>(&cfg.chunk)->default_value(1048),
            "Chunk size.")
        ("depth", po::value<size_t>(&cfg.depth)->default_value(3),
            "Directories depth.")
        ("dir_count", po::value<size_t>(&cfg.dir_count)->default_value(3),
            "Directories count per depth level.")
        ("file_count", po::value<size_t>(&cfg.file_count)->default_value(5),
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

class Chicho
{
    size_t _dir_count = 0;
    size_t _files_count = 0;
    const Config& _cfg;
    fs::path _target_dir;
    const chrono::time_point<steady_clock> _start;

public:
    Chicho(const Config& cfg)
        :_dir_count(0)
        ,_files_count(0)
        ,_cfg(cfg)
        ,_start(steady_clock::now())
    {
        _target_dir = cfg.target_dir;
        std::error_code ec;
        if (!exists(_target_dir, ec))
        {
            fs::create_directory(_target_dir);
            _dir_count++;
        }
    }

    ~Chicho()
    {
        auto elapsed = (steady_clock::now() - _start);
        minutes mm = duration_cast<minutes>(elapsed);
        seconds ss = duration_cast<seconds>(elapsed % minutes(1));
        std::cout << _files_count << " files in " << _dir_count << " directories in " <<
            setfill('0') << setw(2) << mm.count() << " mns " << setw(2) << ss.count() << " secs" << endl;
    }

    fs::path create_directory_and_files(
        fs::path parent,
        const Config& cfg)
    {
        fs::path new_dir = parent / to_string(gen_random(cfg.name_length));
        error_code ec;
        size_t count = 0;
        while (exists(new_dir, ec))
        {
            if (++count > 10)
            {
                stringstream ss;
                ss << "Unable to generate a new directory name (" << new_dir << ")";
                throw runtime_error(ss.str());
            }
            new_dir = parent / to_string(gen_random(cfg.name_length));
        }

        fs::create_directory(new_dir);
        _dir_count++;

        // files generation
        for (size_t f = 0; f < cfg.file_count; f++)
        {
            fs::path new_file = new_dir / to_string(gen_random(cfg.name_length));
            error_code ec;
            size_t count = 0;
            while (exists(new_file, ec))
            {
                if (++count > 10)
                {
                    stringstream ss;
                    ss << "Unable to generate a new file name (" << new_file << ")";
                    throw runtime_error(ss.str());
                }
                new_file = new_dir / to_string(gen_random(cfg.name_length));
            }
            ofstream file(new_file);
            _files_count++;

            size_t i = 0;
            for (; i < cfg.file_length % cfg.chunk; i++)
            {
                vector<char> v = gen_random(cfg.chunk);
                file.write(&v[0], v.size());
            }
            vector<char> v = gen_random(cfg.chunk > cfg.file_length ? 
                cfg.file_length : (cfg.file_length - i * cfg.chunk));
            if (!v.empty())
                file.write(&v[0], v.size());
        }

        return new_dir;
    }

    vector<fs::path> generate(
        const fs::path& parent)
    {
        vector<fs::path> end_paths;
        for (size_t h = 0; h < _cfg.dir_count; h++)
        {
            fs::path generated = create_directory_and_files(parent, _cfg);
            end_paths.push_back(generated);
        }
        return end_paths;
    }
};

int main(int ac, char* av[])
{
    try {
        const Config cfg = Get_config(ac, av);
        if (cfg.Help())
            return 0;

        Chicho chicho(cfg);

        vector<fs::path> end_paths = chicho.generate(cfg.target_dir);
        for (size_t v = 1; v < cfg.depth; v++)
        {
            vector<fs::path> new_end_paths;
            for_each(end_paths.begin(), end_paths.end(),
                [&chicho, &new_end_paths](const fs::path& ipath)
            {
                vector<fs::path> generated = chicho.generate(ipath);
                new_end_paths.insert(new_end_paths.end(), generated.begin(), generated.end());
            });
            end_paths = new_end_paths;
        }
    }
    catch (exception& e) {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        cerr << "Exception of unknown type!\n";
        return 1;
    }

    return 0;
}
